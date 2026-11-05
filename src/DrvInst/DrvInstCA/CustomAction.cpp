/*
 * Copyright (C) 2024 Red Hat, Inc.
 *
 * Written By: Vadim Rozenfeld <vrozenfe@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met :
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and / or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of their contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "stdafx.h"
#include "ConfigClass.h"
#include "DevProps.h"
#include "DriverTransaction.h"
#include <msidefs.h>


LPCWSTR vcsInstallDriverQuery =
L"SELECT `DriverInstallTable`.`Component`, `DriverInstallTable`.`FileName`, `DriverInstallTable`.`Directory`, `DriverInstallTable`.`Flags`, `DriverInstallTable`.`Sequence`, `Component`.`ComponentId` FROM `DriverInstallTable`, `Component` WHERE `DriverInstallTable`.`Component` = `Component`.`Component` ORDER BY `Sequence`";
enum eInstallDriverQuery { feqComponent = 1, feqFileName, feqDirectory, feqFlags, feqSequence, feqIdentity };
const UINT COST_DRIVER_EXCEPTION = 200000;


static HRESULT Save(const PCWSTR filename);
static HRESULT Restore(const PCWSTR filename);
static HRESULT RefuseNestedDriverEffects(MSIHANDLE install);

UINT __stdcall SaveSettings(
    MSIHANDLE hInstall
)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;
    LPWSTR pszCustomActionData = NULL;

    hr = WcaInitialize(hInstall, "SaveSettings");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "SaveSettings Initialized.");

    hr = WcaGetProperty(L"CustomActionData", &pszCustomActionData);
    ExitOnFailure(hr, "Failed to get Custom Action Data.");
    WcaLog(LOGMSG_STANDARD, "Custom Action Data = '%ws'.", pszCustomActionData);

    hr = Save(pszCustomActionData);
    ExitOnFailure(hr, "Custom action failed");

LExit:
    ReleaseStr(pszCustomActionData);
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall RestoreSettings(
    MSIHANDLE hInstall
)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;
    LPWSTR pszCustomActionData = NULL;

    hr = WcaInitialize(hInstall, "RestoreSettings");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "RestoreSettings Initialized.");

    hr = WcaGetProperty(L"CustomActionData", &pszCustomActionData);
    ExitOnFailure(hr, "Failed to get Custom Action Data.");
    WcaLog(LOGMSG_STANDARD, "Custom Action Data = '%ws'.", pszCustomActionData);

    hr = Restore(pszCustomActionData);
    ExitOnFailure(hr, "Custom action failed");

LExit:
    ReleaseStr(pszCustomActionData);
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

// Read-only MSI metadata; never depend on private DIFx persistence to admit a shared handoff.
static HRESULT CheckLegacyProduct(MSIHANDLE hInstall, PCWSTR product,
    const std::vector<std::wstring>& retiringProducts, PCWSTR driverTable)
{
    wchar_t cache[MAX_PATH];
    DWORD size = _countof(cache);
    UINT er = MsiGetProductInfoW(product, INSTALLPROPERTY_LOCALPACKAGE, cache, &size);
    if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
    PMSIHANDLE database;
    er = MsiOpenDatabaseW(cache, MSIDBOPEN_READONLY, &database);
    if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
    MSICONDITION table = MsiDatabaseIsTablePersistentW(database, driverTable);
    if (table == MSICONDITION_NONE) return S_OK;
    if (table != MSICONDITION_TRUE) return E_FAIL;

    PMSIHANDLE view;
    std::wstring query = L"SELECT `Component`.`Component`, `Component`.`ComponentId`, `Component`.`Attributes` FROM `Component`, `";
    query += driverTable;
    query += L"` WHERE `Component`.`Component` = `";
    query += driverTable;
    query += L"`.`Component`";
    er = MsiDatabaseOpenViewW(database, query.c_str(), &view);
    if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
    er = MsiViewExecute(view, 0);
    if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
    PMSIHANDLE current = MsiGetActiveDatabase(hInstall);
    if (!current) return E_FAIL;
    PMSIHANDLE record;
    while ((er = MsiViewFetch(view, &record)) == ERROR_SUCCESS)
    {
        wchar_t component[73], guid[39];
        size = _countof(component);
        er = MsiRecordGetStringW(record, 1, component, &size);
        if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
        size = _countof(guid);
        er = MsiRecordGetStringW(record, 2, guid, &size);
        if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
        if (!*guid) return E_FAIL;
        INSTALLSTATE installed;
        er = MsiQueryComponentStateW(product, NULL, MSIINSTALLCONTEXT_MACHINE, guid, &installed);
        if (er == ERROR_UNKNOWN_COMPONENT) continue;
        if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
        if (installed != INSTALLSTATE_LOCAL && installed != INSTALLSTATE_SOURCE) continue;
        bool permanent = (MsiRecordGetInteger(record, 3) & msidbComponentAttributesPermanent) != 0;
        bool retainedOwner = false;
        for (DWORD index = 0;; ++index)
        {
            wchar_t owner[39], sid[256];
            DWORD sidSize = _countof(sid);
            MSIINSTALLCONTEXT context;
            er = MsiEnumClientsExW(guid, NULL, MSIINSTALLCONTEXT_MACHINE, index,
                owner, &context, sid, &sidSize);
            if (er == ERROR_NO_MORE_ITEMS) break;
            if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
            bool retiring = false;
            for (const auto& candidate : retiringProducts)
                if (_wcsicmp(owner, candidate.c_str()) == 0) retiring = true;
            if (!retiring) retainedOwner = true;
        }

        PMSIHANDLE selectedView, parameter = MsiCreateRecord(1), selected;
        er = MsiRecordSetStringW(parameter, 1, guid);
        if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
        er = MsiDatabaseOpenViewW(current,
            L"SELECT `Component` FROM `Component` WHERE `ComponentId` = ?", &selectedView);
        if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
        er = MsiViewExecute(selectedView, parameter);
        if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
        bool successorOwner = false;
        while ((er = MsiViewFetch(selectedView, &selected)) == ERROR_SUCCESS)
        {
            wchar_t name[73];
            size = _countof(name);
            er = MsiRecordGetStringW(selected, 1, name, &size);
            if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
            INSTALLSTATE before, action;
            er = MsiGetComponentStateW(hInstall, name, &before, &action);
            if (er != ERROR_SUCCESS) return HRESULT_FROM_WIN32(er);
            if (action == INSTALLSTATE_LOCAL || action == INSTALLSTATE_SOURCE)
                successorOwner = true;
        }
        if (er != ERROR_NO_MORE_ITEMS) return HRESULT_FROM_WIN32(er);
        if (!permanent && !retainedOwner && !successorOwner)
        {
            WcaLog(LOGMSG_STANDARD, "Unsafe legacy last-owner removal: product %ws component %ws (%ws).",
                product, component, guid);
            PMSIHANDLE message = MsiCreateRecord(0);
            MsiRecordSetStringW(message, 0,
                L"This upgrade would remove an installed predecessor driver without a supported rollback inverse. "
                L"Upgrade retaining installed drivers, then change driver features using the successor installer.");
            MsiProcessMessage(hInstall, INSTALLMESSAGE_ERROR, message);
            return HRESULT_FROM_WIN32(ERROR_INSTALL_FAILURE);
        }
    }
    return er == ERROR_NO_MORE_ITEMS ? S_OK : HRESULT_FROM_WIN32(er);
}

UINT __stdcall CheckLegacyDriverOwnership(MSIHANDLE hInstall)
{
    HRESULT hr = WcaInitialize(hInstall, "CheckLegacyDriverOwnership");
    LPWSTR products = NULL;
    if (SUCCEEDED(hr)) hr = WcaSetProperty(L"DriverTransactionKeys", L"");
    if (SUCCEEDED(hr)) hr = RefuseNestedDriverEffects(hInstall);
    if (SUCCEEDED(hr)) hr = WcaGetProperty(L"WIX_UPGRADE_DETECTED", &products);
    if (SUCCEEDED(hr) && products && *products)
    {
        std::vector<std::wstring> retiring;
        std::wstring value(products);
        size_t start = 0, end;
        do
        {
            end = value.find(L';', start);
            std::wstring product = value.substr(start, end == std::wstring::npos ? end : end - start);
            if (!product.empty()) retiring.push_back(product);
            start = end == std::wstring::npos ? end : end + 1;
        } while (end != std::wstring::npos);
        for (const auto& product : retiring)
        {
            hr = CheckLegacyProduct(hInstall, product.c_str(), retiring, L"MsiDriverPackages");
            if (SUCCEEDED(hr)) hr = CheckLegacyProduct(hInstall, product.c_str(), retiring, L"DriverInstallTable");
            if (FAILED(hr)) break;
        }
    }
    ReleaseStr(products);
    if (FAILED(hr)) WcaLog(LOGMSG_STANDARD, "Legacy ownership admission failed: 0x%x.", hr);
    return WcaFinalize(SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE);
}

static HRESULT OtherMachineOwner(PCWSTR component, PCWSTR product, bool& other)
{
    other = false;
    for (DWORD index = 0;; ++index)
    {
        wchar_t client[39], sid[256];
        DWORD size = _countof(sid);
        MSIINSTALLCONTEXT context;
        UINT error = MsiEnumClientsExW(component, NULL, MSIINSTALLCONTEXT_MACHINE, index,
            client, &context, sid, &size);
        if (error == ERROR_NO_MORE_ITEMS) return S_OK;
        if (error != ERROR_SUCCESS) return HRESULT_FROM_WIN32(error);
        if (_wcsicmp(client, product) != 0) other = true;
    }
}

static HRESULT RefuseNestedDriverEffects(MSIHANDLE install)
{
    LPWSTR upgrading = NULL, product = NULL, name = NULL, identity = NULL;
    HRESULT hr = WcaGetProperty(L"UPGRADINGPRODUCTCODE", &upgrading);
    PMSIHANDLE view, record;
    if (FAILED(hr) || !upgrading || !*upgrading) goto LExit;
    hr = WcaGetProperty(L"ProductCode", &product);
    if (FAILED(hr)) goto LExit;
    if (WcaTableExists(L"DriverInstallTable") == S_FALSE) goto LExit;
    hr = WcaOpenExecuteView(vcsInstallDriverQuery, &view);
    if (FAILED(hr)) goto LExit;
    while ((hr = WcaFetchRecord(view, &record)) == S_OK)
    {
        hr = WcaGetRecordString(record, feqComponent, &name);
        if (FAILED(hr)) goto LExit;
        hr = WcaGetRecordString(record, feqIdentity, &identity);
        if (FAILED(hr)) goto LExit;
        INSTALLSTATE before, action;
        UINT error = MsiGetComponentStateW(install, name, &before, &action);
        if (error != ERROR_SUCCESS) { hr = HRESULT_FROM_WIN32(error); goto LExit; }
        bool mayInstall = WcaIsInstalling(before, action) || WcaIsReInstalling(before, action);
        if (mayInstall || WcaIsUninstalling(before, action))
        {
            bool other = false;
            if (!mayInstall) hr = OtherMachineOwner(identity, product, other);
            if (FAILED(hr)) goto LExit;
            if (mayInstall || !other)
            {
                WcaLog(LOGMSG_STANDARD, "Nested major upgrade cannot execute native work for component %ws: retain installed drivers, then modify with successor.", name);
                PMSIHANDLE message = MsiCreateRecord(0);
                MsiRecordSetStringW(message, 0, L"This upgrade cannot change installed predecessor drivers inside a nested transaction. Upgrade retaining installed drivers, then modify or remove drivers using the successor installer.");
                MsiProcessMessage(install, INSTALLMESSAGE_ERROR, message);
                hr = HRESULT_FROM_WIN32(ERROR_INSTALL_FAILURE);
                goto LExit;
            }
        }
    }
    if (hr == E_NOMOREITEMS) hr = S_OK;
LExit:
    ReleaseStr(upgrading); ReleaseStr(product); ReleaseStr(name); ReleaseStr(identity);
    return hr;
}

static UINT ProcessPackagesForAction(MSIHANDLE hInstall, bool installing)
{
    HRESULT hr = WcaInitialize(hInstall, "ProcessPackages");
    PMSIHANDLE view, record;
    LPWSTR body = NULL, data = NULL, cleanup = NULL, product = NULL, nested = NULL;
    LPWSTR component = NULL, identity = NULL, file = NULL, directory = NULL, path = NULL;
    unsigned count = 0;
    GUID guid;
    wchar_t keyGuid[39];
    std::wstring key;
    if (FAILED(hr)) goto LExit;
    if (WcaTableExists(L"DriverInstallTable") == S_FALSE) goto LExit;
    hr = WcaGetProperty(L"ProductCode", &product);
    if (FAILED(hr)) goto LExit;
    hr = WcaOpenExecuteView(vcsInstallDriverQuery, &view);
    if (FAILED(hr)) goto LExit;
    while ((hr = WcaFetchRecord(view, &record)) == S_OK)
    {
        int flags;
        INSTALLSTATE before, action;
        hr = WcaGetRecordString(record, feqComponent, &component);
        if (FAILED(hr)) goto LExit;
        UINT error = MsiGetComponentStateW(hInstall, component, &before, &action);
        if (error != ERROR_SUCCESS) { hr = HRESULT_FROM_WIN32(error); goto LExit; }
        bool needed = installing ? (WcaIsInstalling(before, action) || WcaIsReInstalling(before, action)) :
            WcaIsUninstalling(before, action);
        if (!needed) continue;
        hr = WcaGetRecordString(record, feqIdentity, &identity);
        if (FAILED(hr)) goto LExit;
        bool other = false;
        if (!installing)
        {
            hr = OtherMachineOwner(identity, product, other);
            if (FAILED(hr)) goto LExit;
            if (other) continue;
        }
        hr = WcaGetRecordString(record, feqFileName, &file);
        if (FAILED(hr)) goto LExit;
        hr = WcaGetRecordString(record, feqDirectory, &directory);
        if (FAILED(hr)) goto LExit;
        hr = WcaGetRecordInteger(record, feqFlags, &flags);
        if (FAILED(hr)) goto LExit;
        hr = WcaGetTargetPath(directory, &path);
        if (FAILED(hr)) goto LExit;
        hr = StrAllocConcat(&path, file, 0);
        if (FAILED(hr)) goto LExit;
        hr = WcaWriteStringToCaData(path, &body);
        if (FAILED(hr)) goto LExit;
        hr = WcaWriteStringToCaData(identity, &body);
        if (FAILED(hr)) goto LExit;
        hr = WcaWriteIntegerToCaData(flags, &body);
        if (FAILED(hr)) goto LExit;
        hr = WcaWriteIntegerToCaData(installing ? 0 : 1, &body);
        if (FAILED(hr)) goto LExit;
        ++count;
    }
    if (hr == E_NOMOREITEMS) hr = S_OK;
    if (FAILED(hr) || !count) goto LExit;
    hr = WcaGetProperty(L"UPGRADINGPRODUCTCODE", &nested);
    if (FAILED(hr)) goto LExit;
    if (nested && *nested) { hr = HRESULT_FROM_WIN32(ERROR_INSTALL_FAILURE); goto LExit; }
    hr = CoCreateGuid(&guid);
    if (FAILED(hr)) goto LExit;
    if (!StringFromGUID2(guid, keyGuid, _countof(keyGuid))) { hr = E_FAIL; goto LExit; }
    key = L"DrvTx-" + std::wstring(keyGuid);
    hr = WcaWriteStringToCaData(key.c_str(), &data);
    if (FAILED(hr)) goto LExit;
    hr = WcaWriteIntegerToCaData(installing ? 1 : 0, &data);
    if (FAILED(hr)) goto LExit;
    hr = WcaWriteIntegerToCaData(count, &data);
    if (FAILED(hr)) goto LExit;
    hr = WcaWriteStringToCaData(body, &data);
    if (FAILED(hr)) goto LExit;
    hr = WcaGetProperty(L"DriverTransactionKeys", &cleanup);
    if (FAILED(hr)) goto LExit;
    hr = WcaWriteStringToCaData(key.c_str(), &cleanup);
    if (FAILED(hr)) goto LExit;
    hr = WcaSetProperty(L"DriverTransactionKeys", cleanup);
    if (FAILED(hr)) goto LExit;
    // Begin must succeed before its rollback is registered: missing inverse state then is never normal.
    hr = WcaDoDeferredAction(L"BeginDriverTransaction", data, 0);
    if (FAILED(hr)) goto LExit;
    hr = WcaDoDeferredAction(installing ? L"RollbackInstall" : L"RollbackUninstall", data, 0);
    if (FAILED(hr)) goto LExit;
    hr = WcaDoDeferredAction(installing ? L"InstallDriverPackages" : L"UninstallDriverPackages", data, count * COST_DRIVER_EXCEPTION);
LExit:
    ReleaseStr(body); ReleaseStr(data); ReleaseStr(cleanup); ReleaseStr(product); ReleaseStr(nested);
    ReleaseStr(component); ReleaseStr(identity); ReleaseStr(file); ReleaseStr(directory); ReleaseStr(path);
    return WcaFinalize(SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE);
}

UINT __stdcall ProcessPackages(MSIHANDLE install) { return ProcessPackagesForAction(install, true); }
UINT __stdcall ProcessUninstallPackages(MSIHANDLE install) { return ProcessPackagesForAction(install, false); }

static UINT RunDriverTransactionAction(MSIHANDLE install, unsigned operation)
{
    HRESULT hr = WcaInitialize(install, "DriverTransaction");
    LPWSTR data = NULL;
    bool reboot = false;
    if (SUCCEEDED(hr)) hr = WcaGetProperty(operation == 3 ? L"DriverTransactionKeys" : L"CustomActionData", &data);
    if (SUCCEEDED(hr))
    {
        try
        {
            if (operation == 0) hr = BeginDriverTransaction(data);
            else if (operation == 1) hr = ExecuteDriverTransaction(data, reboot);
            else if (operation == 2) hr = RollbackDriverTransaction(data, reboot);
            else hr = CleanupDriverTransactions(data);
        }
        catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
    }
    ReleaseStr(data);
    if (reboot) WcaDeferredActionRequiresReboot();
    if (FAILED(hr)) WcaLog(LOGMSG_STANDARD, "Driver transaction operation %u failed: 0x%x.", operation, hr);
    // After-finalize housekeeping is nonblocking; real forward/inverse errors stay checked.
    return WcaFinalize(operation == 3 || SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE);
}

UINT __stdcall BeginDriverPackages(MSIHANDLE install) { return RunDriverTransactionAction(install, 0); }
UINT __stdcall InstallPackages(MSIHANDLE install) { return RunDriverTransactionAction(install, 1); }
UINT __stdcall UninstallPackages(MSIHANDLE install) { return RunDriverTransactionAction(install, 1); }
UINT __stdcall RollbackDriverPackages(MSIHANDLE install) { return RunDriverTransactionAction(install, 2); }
UINT __stdcall CleanupDriverPackages(MSIHANDLE install) { return RunDriverTransactionAction(install, 3); }

extern "C" BOOL WINAPI DllMain(
    __in HINSTANCE hInst,
    __in ULONG ulReason,
    __in LPVOID
    )
{
    switch(ulReason)
    {
    case DLL_PROCESS_ATTACH:
        WcaGlobalInitialize(hInst);
        break;

    case DLL_PROCESS_DETACH:
        WcaGlobalFinalize();
        break;
    }

    return TRUE;
}

HRESULT Save(const PCWSTR filename)
{
    LogReport(S_OK, L"%ws filename = %ws", WFUNCTION, filename);
    ConfigWrite* cfg = new ConfigWrite(filename);
    if (!cfg->Init(false)) {
        delete cfg;
        LogReport(S_OK, L"cfg Init failed");
        return E_FAIL;
    }
    cfg->Run();
    delete cfg;
    return S_OK;
}

HRESULT Restore(const PCWSTR filename)
{
    ConfigRead* cfg = new ConfigRead(filename);
    if (!cfg->Init(true)) {
        delete cfg;
        return E_FAIL;
    }
    cfg->Run();
    delete cfg;
    return S_OK;
}
