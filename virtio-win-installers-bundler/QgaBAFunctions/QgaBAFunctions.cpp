#include <windows.h>
#include <msiquery.h>
#include <objbase.h>
#include <commctrl.h>
#include <new>
#include <stdio.h>
#include <BootstrapperApplicationTypes.h>
#include <BAFunctions.h>

namespace
{
struct Policy
{
    IBootstrapperEngine* engine;
    BOOTSTRAPPER_ACTION original;
    BOOTSTRAPPER_DISPLAY display;
    bool activeResume;
    bool resume = false;
    bool detected = false;
    bool failed = false;
    bool ready = false;
    bool canModify = false;
    BOOL is64 = FALSE;
    int actual = -1; // -1 absent, -2 both families, 0 Agent only, 1 full.
    LONGLONG raw = -1;
    LONGLONG pendingKind = 0;
    LONGLONG pendingSource = -1;
    LONGLONG pendingTarget = -1;
    LONGLONG action = 0;
    LONGLONG target = -1;
    bool switching = false;

    /// Retain the engine and original entry intent before StdBA maps maintenance actions.
    Policy(const BA_FUNCTIONS_CREATE_ARGS* args) : engine(args->pEngine),
        original(args->pCommand->action), display(args->pCommand->display),
        activeResume(args->pCommand->resumeType == BOOTSTRAPPER_RESUME_TYPE_INTERRUPTED ||
            args->pCommand->resumeType == BOOTSTRAPPER_RESUME_TYPE_REBOOT ||
            args->pCommand->resumeType == BOOTSTRAPPER_RESUME_TYPE_SUSPEND)
    {
        engine->AddRef();
    }
    ~Policy() { engine->Release(); }

    void Fail(LPCWSTR message)
    {
        failed = true;
        engine->Log(BOOTSTRAPPER_LOG_LEVEL_ERROR, message);
        engine->SetVariableString(L"QgaLifecycleError", message, FALSE);
    }
    /// Read a numeric variable; a read failure latches policy failure rather than a default.
    bool Number(LPCWSTR name, LONGLONG& value)
    {
        if (SUCCEEDED(engine->GetVariableNumeric(name, &value))) return true;
        Fail(L"Cannot read a QGA lifecycle variable. No packages will be applied.");
        return false;
    }
    /// Record engine state and latch failure if the operation's intent cannot be written.
    bool Set(LPCWSTR name, LONGLONG value)
    {
        if (SUCCEEDED(engine->SetVariableNumeric(name, value))) return true;
        Fail(L"Cannot save QGA lifecycle intent. No packages will be applied.");
        return false;
    }
    /// Evaluate a predicate without treating an evaluation error as permission to proceed.
    bool Test(LPCWSTR expression, BOOL& value)
    {
        if (SUCCEEDED(engine->EvaluateCondition(expression, &value))) return true;
        Fail(L"Cannot evaluate QGA inventory. No packages will be applied.");
        return false;
    }
    void ClearPending()
    {
        Set(L"QgaPendingKind", 0);
        Set(L"QgaPendingSource", -1);
        Set(L"QgaPendingTarget", -1);
    }
    /// Admit pending intent only for a matching active continuation, never ordinary maintenance.
    void Initialize()
    {
        Number(L"QgaVssEnabled", raw);
        if (raw < -1 || raw > 1) Fail(L"QgaVssEnabled must be -1 (automatic), 0 (Agent only), or 1 (with VSS).");
        Number(L"QgaPendingKind", pendingKind);
        Number(L"QgaPendingSource", pendingSource);
        Number(L"QgaPendingTarget", pendingTarget);
        // ARP loads persisted variables too. It is not an active continuation.
        resume = activeResume && (pendingTarget == 0 || pendingTarget == 1) &&
            pendingSource >= -1 && pendingSource <= 1 &&
            ((pendingKind == BOOTSTRAPPER_ACTION_INSTALL && original == BOOTSTRAPPER_ACTION_INSTALL &&
              (pendingSource == -1 || pendingSource == pendingTarget)) ||
             (pendingKind == BOOTSTRAPPER_ACTION_MODIFY && original == BOOTSTRAPPER_ACTION_MODIFY)) &&
            (raw == -1 || raw == pendingTarget);
        Set(L"QgaResumeAllowed", resume ? 1 : 0);
        // A GUI Apply-changes plan resumes as /repair. The public command cannot
        // distinguish that continuation from a new Repair: do not infer a switch.
        if (activeResume && pendingKind == BOOTSTRAPPER_ACTION_MODIFY && original == BOOTSTRAPPER_ACTION_REPAIR)
            Fail(L"An interrupted QGA change cannot be resumed by Repair. Reopen Modify to retry the change.");
        if (activeResume && pendingKind != 0 && !resume && raw == -1 &&
            original != BOOTSTRAPPER_ACTION_UNINSTALL && original != BOOTSTRAPPER_ACTION_UNSAFE_UNINSTALL &&
            original != BOOTSTRAPPER_ACTION_REPAIR)
            Fail(L"The interrupted QGA operation does not match this command. Reopen Modify with an explicit choice.");
    }

    // Use only public MSI APIs and read-only cached metadata. Family search alone
    // selects the highest version and cannot reject duplicate or per-user products.
    /// Accept at most one machine-context MSI per family, with readable matching cached metadata.
    bool Inventory(LPCWSTR variable, bool expected, bool& present)
    {
        WCHAR family[40] = {};
        SIZE_T length = ARRAYSIZE(family);
        if (FAILED(engine->GetVariableString(variable, family, &length))) return false;
        present = false;
        for (DWORD index = 0;; ++index)
        {
            WCHAR product[39] = {};
            UINT result = MsiEnumRelatedProductsW(family, 0, index, product);
            if (result == ERROR_NO_MORE_ITEMS) return present == expected;
            if (result != ERROR_SUCCESS || present) return false;
            WCHAR state[8] = {};
            DWORD count = ARRAYSIZE(state);
            if (MsiGetProductInfoExW(product, nullptr, MSIINSTALLCONTEXT_MACHINE,
                INSTALLPROPERTY_PRODUCTSTATE, state, &count) != ERROR_SUCCESS || wcscmp(state, L"5")) return false;
            WCHAR path[MAX_PATH] = {};
            count = ARRAYSIZE(path);
            if (MsiGetProductInfoExW(product, nullptr, MSIINSTALLCONTEXT_MACHINE,
                INSTALLPROPERTY_LOCALPACKAGE, path, &count) != ERROR_SUCCESS) return false;
            MSIHANDLE database = 0, summary = 0;
            result = MsiOpenDatabaseW(path, MSIDBOPEN_READONLY, &database);
            if (result == ERROR_SUCCESS) result = MsiGetSummaryInformationW(database, nullptr, 0, &summary);
            WCHAR platform[128] = {};
            UINT type = 0;
            INT integer = 0;
            FILETIME time = {};
            count = ARRAYSIZE(platform);
            if (result == ERROR_SUCCESS)
                result = MsiSummaryInfoGetPropertyW(summary, 7, &type, &integer, &time, platform, &count);
            if (summary) MsiCloseHandle(summary);
            if (database) MsiCloseHandle(database);
            if (result != ERROR_SUCCESS ||
                (is64 ? wcsncmp(platform, L"x64;", 4) : wcsncmp(platform, L"Intel;", 6))) return false;
            present = true;
        }
    }
    /// Reconcile searches and MSI inventory once, before StdBA evaluates the detected page.
    void Detect()
    {
        if (failed) return;
        LONGLONG full = 0, agent = 0, service = 0, installed = 0;
        Number(L"QgaFullState", full);
        Number(L"QgaNoVssState", agent);
        Number(L"QgaServicePresent", service);
        Number(L"WixBundleInstalled", installed);
        Test(L"VersionNT64", is64);
        bool haveFull = false, haveAgent = false;
        if ((full != 2 && full != 5) || (agent != 2 && agent != 5) ||
            !Inventory(L"QgaFullFamily", full == 5, haveFull) ||
            !Inventory(L"QgaNoVssFamily", agent == 5, haveAgent) ||
            (service && !haveFull && !haveAgent))
        {
            Fail(L"QGA inventory is ambiguous, unavailable, or in an unsupported architecture or user context. Use its original installer to resolve it.");
            return;
        }
        actual = haveFull ? (haveAgent ? -2 : 1) : (haveAgent ? 0 : -1);
        if (actual == -2 && !(resume && pendingKind == BOOTSTRAPPER_ACTION_MODIFY &&
            pendingSource >= 0 && pendingSource != pendingTarget))
        {
            Fail(L"Both QGA variants are installed without matching active change intent. Resolve the conflicting installations first.");
            return;
        }
        if (resume && ((pendingKind == BOOTSTRAPPER_ACTION_INSTALL && actual >= 0 && actual != pendingTarget) ||
            (pendingKind == BOOTSTRAPPER_ACTION_MODIFY && actual >= 0 && actual != pendingSource && actual != pendingTarget)))
        {
            Fail(L"The installed QGA variant no longer matches the interrupted operation. Reopen Modify with an explicit choice.");
            return;
        }
        if (original == BOOTSTRAPPER_ACTION_REPAIR && actual < 0)
        {
            Fail(L"Repair requires one recognized installed QGA variant. Reopen Modify to select a variant explicitly.");
            return;
        }
        canModify = installed && (original == BOOTSTRAPPER_ACTION_MODIFY ||
            (original == BOOTSTRAPPER_ACTION_INSTALL && display == BOOTSTRAPPER_DISPLAY_FULL));
        Set(L"QgaCanModify", canModify ? 1 : 0);
        target = resume ? pendingTarget : (raw == -1 ? (actual < 0 ? 1 : actual) : raw);
        if (actual >= 0 && raw != -1 && raw != actual && !canModify && !resume &&
            original != BOOTSTRAPPER_ACTION_UNINSTALL && original != BOOTSTRAPPER_ACTION_UNSAFE_UNINSTALL)
            Fail(L"This QGA choice conflicts with the installed variant. Only Modify can change the variant; upgrades and Repair preserve it.");
        Set(L"QgaVssTarget", target);
        Set(L"QgaVssEffective", target);
        detected = !failed;
        // A completed or stale record must not become a new maintenance request.
        if (!activeResume || (resume && actual == pendingTarget)) ClearPending();
    }
    void ServiceFailure(LPCWSTR name, LPCWSTR reason, DWORD error)
    {
        WCHAR message[512] = {};
        swprintf_s(message, ARRAYSIZE(message), L"QGA VSS prerequisite %ls: %ls (Win32 %lu). No service policy will be changed.",
            name, reason, error);
        Fail(message);
    }
    /// Traverse actual SCM configuration with bounds; never activate or reconfigure services.
    bool ServiceDependencies(SC_HANDLE manager, LPCWSTR name, DWORD depth, DWORD& remaining)
    {
        // Bound invalid/custom dependency graphs without starting or reconfiguring services.
        if (depth >= 32 || !remaining)
        {
            ServiceFailure(name, L"dependency chain exceeds the supported bound", ERROR_CIRCULAR_DEPENDENCY);
            return false;
        }
        --remaining;
        SC_HANDLE service = OpenServiceW(manager, name, SERVICE_QUERY_CONFIG);
        if (!service)
        {
            ServiceFailure(name, L"cannot read service configuration", GetLastError());
            return false;
        }
        DWORD bytes = 0;
        BOOL queried = QueryServiceConfigW(service, nullptr, 0, &bytes);
        DWORD error = GetLastError();
        auto config = !queried && error == ERROR_INSUFFICIENT_BUFFER && bytes ?
            static_cast<QUERY_SERVICE_CONFIGW*>(HeapAlloc(GetProcessHeap(), 0, bytes)) : nullptr;
        bool usable = config && QueryServiceConfigW(service, config, bytes, &bytes);
        if (!usable)
            ServiceFailure(name, L"cannot read service configuration", config ? GetLastError() :
                (error == ERROR_INSUFFICIENT_BUFFER ? ERROR_NOT_ENOUGH_MEMORY : error));
        else if (config->dwStartType == SERVICE_DISABLED)
        {
            ServiceFailure(name, L"service is disabled", ERROR_SERVICE_DISABLED);
            usable = false;
        }
        for (LPCWSTR dependency = usable ? config->lpDependencies : nullptr;
             dependency && *dependency && usable; dependency += wcslen(dependency) + 1)
        {
            if (*dependency == SC_GROUP_IDENTIFIER)
            {
                ServiceFailure(dependency, L"service-group dependencies are unsupported", ERROR_NOT_SUPPORTED);
                usable = false;
            }
            else usable = ServiceDependencies(manager, dependency, depth + 1, remaining);
        }
        if (config) HeapFree(GetProcessHeap(), 0, config);
        CloseServiceHandle(service);
        return usable;
    }
    /// Gate operations touching full QGA; Agent-only operations otherwise need no VSS services.
    bool Dependencies()
    {
        if (target != 1 && actual != 1 && actual != -2) return true;
        // Re-read at planning/apply, not only at Detect or from checkbox visibility.
        SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!manager)
        {
            ServiceFailure(L"SCM", L"cannot read service configuration", GetLastError());
            return false;
        }
        DWORD remaining = 64;
        bool usable = ServiceDependencies(manager, L"SENS", 0, remaining) &&
            ServiceDependencies(manager, L"COMSysApp", 0, remaining);
        CloseServiceHandle(manager);
        return usable;
    }
    /// Read the established plan action while preserving the narrower original-entry authorization.
    void Plan()
    {
        if (ready || failed) return;
        ready = true;
        if (!detected) { Fail(L"QGA detection did not complete. No packages will be applied."); return; }
        Number(L"WixBundleAction", action);
        if (action == BOOTSTRAPPER_ACTION_UNINSTALL || action == BOOTSTRAPPER_ACTION_UNSAFE_UNINSTALL)
        {
            target = -1;
            Dependencies();
            return;
        }
        if (action != BOOTSTRAPPER_ACTION_INSTALL && action != BOOTSTRAPPER_ACTION_MODIFY &&
            action != BOOTSTRAPPER_ACTION_REPAIR) return;
        Number(L"QgaVssTarget", target);
        if (target != 0 && target != 1) { Fail(L"Select a valid QGA variant before applying changes."); return; }
        // Default full-UI maintenance is authorized only by its native Apply
        // changes (RepairButton) plan, never by a new bundle's Install plan.
        bool authorized = canModify && (original == BOOTSTRAPPER_ACTION_MODIFY ||
            (original == BOOTSTRAPPER_ACTION_INSTALL && action == BOOTSTRAPPER_ACTION_REPAIR));
        switching = (actual >= 0 && target != actual) || actual == -2 ||
            (resume && pendingKind == BOOTSTRAPPER_ACTION_MODIFY);
        if (switching && !authorized && !(resume && pendingKind == BOOTSTRAPPER_ACTION_MODIFY))
            Fail(L"Only explicit Modify can change the installed QGA variant. Repair and upgrades cannot switch it.");
        if (original == BOOTSTRAPPER_ACTION_REPAIR && switching)
            Fail(L"Repair cannot change the installed QGA variant.");
        BOOL age = FALSE;
        Test(L"QgaFullVersion <= QgaPayloadVersion AND QgaNoVssVersion <= QgaPayloadVersion", age);
        if (!age) Fail(L"A newer QGA version is installed. Use matching or newer QGA payloads.");
        Set(L"QgaVssEffective", target);
        Dependencies();
    }
};
Policy* policy = nullptr;

/// Limit requested-state and file-versioning overrides to the four authored QGA package IDs.
bool QgaPackage(LPCWSTR id, int& flavor, bool& x64)
{
    if (!wcscmp(id, L"QgaFull64")) { flavor = 1; x64 = true; }
    else if (!wcscmp(id, L"QgaFull86")) { flavor = 1; x64 = false; }
    else if (!wcscmp(id, L"QgaNoVss64")) { flavor = 0; x64 = true; }
    else if (!wcscmp(id, L"QgaNoVss86")) { flavor = 0; x64 = false; }
    else return false;
    return true;
}
/// Apply policy at supported SDK events; all unhandled events retain StdBA's behavior.
HRESULT WINAPI Dispatch(BA_FUNCTIONS_MESSAGE message, const LPVOID args, LPVOID results, LPVOID context)
{
    Policy& p = *static_cast<Policy*>(context);
    switch (message)
    {
    case BA_FUNCTIONS_MESSAGE_ONDETECTBEGIN:
        p.detected = false;
        break;
    case BA_FUNCTIONS_MESSAGE_ONDETECTPACKAGEBEGIN:
        // Searches have finished; prepare UI variables before StdBA shows its detected page.
        if (!p.detected) p.Detect();
        if (p.failed) static_cast<BA_ONDETECTPACKAGEBEGIN_RESULTS*>(results)->fCancel = TRUE;
        break;
    case BA_FUNCTIONS_MESSAGE_ONPLANBEGIN:
        p.ready = false;
        break;
    case BA_FUNCTIONS_MESSAGE_ONPLANPACKAGEBEGIN:
    {
        auto a = static_cast<const BA_ONPLANPACKAGEBEGIN_ARGS*>(args);
        auto r = static_cast<BA_ONPLANPACKAGEBEGIN_RESULTS*>(results);
        p.Plan();
        // StdBA discards callback HRESULTs. Cancellation is the supported gate.
        if (p.failed) { r->fCancel = TRUE; break; }
        int flavor = -1;
        bool x64 = false;
        if (QgaPackage(a->wzPackageId, flavor, x64) &&
            (p.action == BOOTSTRAPPER_ACTION_INSTALL || p.action == BOOTSTRAPPER_ACTION_MODIFY ||
             p.action == BOOTSTRAPPER_ACTION_REPAIR))
        {
            bool selected = flavor == p.target && x64 == !!p.is64;
            r->requestedState = !selected ? BOOTSTRAPPER_REQUEST_STATE_ABSENT :
                ((p.action == BOOTSTRAPPER_ACTION_REPAIR || p.switching) &&
                 a->state == BOOTSTRAPPER_PACKAGE_STATE_PRESENT ? BOOTSTRAPPER_REQUEST_STATE_REPAIR : BOOTSTRAPPER_REQUEST_STATE_PRESENT);
        }
        break;
    }
    case BA_FUNCTIONS_MESSAGE_ONPLANMSIPACKAGE:
    {
        auto a = static_cast<const BA_ONPLANMSIPACKAGE_ARGS*>(args);
        int flavor = -1;
        bool x64 = false;
        if (QgaPackage(a->wzPackageId, flavor, x64))
            // Apply to rollback too: restoring the intended cached original is not
            // a forward downgrade. All other packages keep their native policy.
            static_cast<BA_ONPLANMSIPACKAGE_RESULTS*>(results)->fileVersioning = BOOTSTRAPPER_MSI_FILE_VERSIONING_ALL;
        break;
    }
    case BA_FUNCTIONS_MESSAGE_ONAPPLYBEGIN:
    {
        p.Plan();
        if (p.action == BOOTSTRAPPER_ACTION_INSTALL || p.action == BOOTSTRAPPER_ACTION_MODIFY ||
            p.action == BOOTSTRAPPER_ACTION_REPAIR || p.action == BOOTSTRAPPER_ACTION_UNINSTALL ||
            p.action == BOOTSTRAPPER_ACTION_UNSAFE_UNINSTALL) p.Dependencies();
        if (!p.failed)
        {
            if (p.switching || p.action == BOOTSTRAPPER_ACTION_INSTALL)
            {
                p.Set(L"QgaPendingSource", p.actual == -2 ? p.pendingSource : p.actual);
                p.Set(L"QgaPendingTarget", p.target);
                // Written before Burn registration saves state; no synchronous SaveState exists.
                p.Set(L"QgaPendingKind", p.resume ? p.pendingKind :
                    (p.switching ? BOOTSTRAPPER_ACTION_MODIFY : BOOTSTRAPPER_ACTION_INSTALL));
            }
            else p.ClearPending();
        }
        if (p.failed) static_cast<BA_ONAPPLYBEGIN_RESULTS*>(results)->fCancel = TRUE;
        break;
    }
    case BA_FUNCTIONS_MESSAGE_ONAPPLYCOMPLETE:
        if (static_cast<const BA_ONAPPLYCOMPLETE_ARGS*>(args)->restart != BOOTSTRAPPER_APPLY_RESTART_INITIATED)
            p.ClearPending(); // Normally persisted at Quit, not crash-atomic here.
        break;
    }
    return S_OK;
}
}

/// Validate the SDK ABI before publishing the single policy context and event dispatcher.
extern "C" HRESULT WINAPI BAFunctionsCreate(const BA_FUNCTIONS_CREATE_ARGS* args, BA_FUNCTIONS_CREATE_RESULTS* results)
{
    const DWORD64 version = (DWORD64(2024) << 48) | (DWORD64(1) << 32) | (DWORD64(1) << 16);
    if (!args || !results || args->cbSize < sizeof(*args) || results->cbSize < sizeof(*results) ||
        args->qwBAFunctionsAPIVersion != version || !args->pEngine || !args->pCommand || policy) return E_INVALIDARG;
    policy = new (std::nothrow) Policy(args);
    if (!policy) return E_OUTOFMEMORY;
    policy->Initialize();
    results->pfnBAFunctionsProc = Dispatch;
    results->pvBAFunctionsProcContext = policy;
    return S_OK;
}
extern "C" void WINAPI BAFunctionsDestroy(const BA_FUNCTIONS_DESTROY_ARGS*, BA_FUNCTIONS_DESTROY_RESULTS*)
{
    delete policy;
    policy = nullptr;
}
