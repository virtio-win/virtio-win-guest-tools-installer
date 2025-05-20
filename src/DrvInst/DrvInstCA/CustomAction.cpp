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


LPCWSTR vcsInstallDriverQuery =
L"SELECT `DriverInstallTable`.`Component`, `DriverInstallTable`.`FileName`, `DriverInstallTable`.`Directory`, `DriverInstallTable`.`Flags`, `DriverInstallTable`.`Sequence` FROM `DriverInstallTable` ORDER BY `Sequence`";
enum eInstallDriverQuery { feqComponent = 1, feqFileName, feqDirectory, feqFlags, feqSequence };
const UINT COST_DRIVER_EXCEPTION = 200000;


static HRESULT Save(const PCWSTR filename);
static HRESULT Restore(const PCWSTR filename);
static HRESULT Install(const PCWSTR filename, bool* bReboorNeeded);
static HRESULT Uninstall(const PCWSTR filename, bool* bReboorNeeded);

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

UINT __stdcall ProcessPackages(
    MSIHANDLE hInstall
    )
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;
    PMSIHANDLE hView = NULL;
    PMSIHANDLE hRec = NULL;
    LPWSTR pwzInstallCustomActionData = NULL;
    LPWSTR pwzUnInstallCustomActionData = NULL;
    LPWSTR pwzComponent = NULL;
    LPWSTR pwzFileName = NULL;
    LPWSTR pwzDirectory = NULL;
    LPWSTR pwzShortcutPath = NULL;
    int iFlags = 0;
    int iSequence = 0;
    int cInstallDrivers = 0;
    int cUnInstallDrivers = 0;
    INSTALLSTATE isInstalled;
    INSTALLSTATE isAction;

    hr = WcaInitialize(hInstall, "ProcessPackages");
    ExitOnFailure(hr, "Failed to initialize ProcessPackages");

    WcaLog(LOGMSG_STANDARD, "ProcessPackages Initialized.");

    if (S_OK != WcaTableExists(L"DriverInstallTable"))
    {
        WcaLog(LOGMSG_STANDARD, "DriverInstallTable table doesn't exist, so there are no Driver exceptions to configure.");
        ExitFunction();
    }

    hr = WcaOpenExecuteView(vcsInstallDriverQuery, &hView);
    ExitOnFailure(hr, "failed to open view on DriverInstallTable table");

    while (S_OK == (hr = WcaFetchRecord(hView, &hRec)))
    {
        hr = WcaGetRecordFormattedString(hRec, feqComponent, &pwzComponent);
        ExitOnFailure(hr, "failed to get Install Driver Component");
        er = ::MsiGetComponentStateW(hInstall, pwzComponent, &isInstalled, &isAction);
        ExitOnWin32Error1(er, hr, "failed to get install state for Component: %ws", pwzComponent);

        hr = WcaGetRecordFormattedString(hRec, feqFileName, &pwzFileName);
        ExitOnFailure(hr, "failed to get Install Driver file name");

        hr = WcaGetRecordFormattedString(hRec, feqDirectory, &pwzDirectory);
        ExitOnFailure(hr, "failed to get Install Driver Directory");

        hr = WcaGetRecordInteger(hRec, feqFlags, &iFlags);
        ExitOnFailure(hr, "failed to get Install Driver Flag");

        hr = WcaGetRecordInteger(hRec, feqSequence, &iSequence);
        ExitOnFailure(hr, "failed to get Install Driver Sequence");

        hr = WcaGetTargetPath(pwzDirectory, &pwzShortcutPath);
        ExitOnFailure(hr, "failed to allocate string for shortcut directory");

        hr = StrAllocConcat(&pwzShortcutPath, pwzFileName, 0);
        ExitOnFailure(hr, "failed to allocate string for shortcut filename");

        if (WcaIsInstalling(isInstalled, isAction) ||
            WcaIsReInstalling(isInstalled, isAction))
        {
//            WcaLog(LOGMSG_STANDARD, "cInstallDriver = '%d'.", cInstallDrivers);
//            WcaLog(LOGMSG_STANDARD, "FileName '%ws'", pwzFileName);
            WcaLog(LOGMSG_STANDARD, "Flag = '%d'.", iFlags);
//            WcaLog(LOGMSG_STANDARD, "Sequence = '%d'.", iSequence);
//            WcaLog(LOGMSG_STANDARD, "Directory '%ws'", pwzDirectory);
            WcaLog(LOGMSG_STANDARD, "Shortcut Directory '%ws'", pwzShortcutPath);

            hr = WcaWriteStringToCaData(pwzShortcutPath, &pwzInstallCustomActionData);
            ExitOnFailure(hr, "failed to write shortcut path to custom install action data");


            hr = WcaWriteIntegerToCaData(iFlags, &pwzInstallCustomActionData);
            ExitOnFailure(hr, "failed to write flags to custom install action data.");

            ++cInstallDrivers;
        }
        else if (WcaIsUninstalling(isInstalled, isAction))
        {
//            WcaLog(LOGMSG_STANDARD, "cUninstallDriver = '%d'.", cUnInstallDrivers);
//            WcaLog(LOGMSG_STANDARD, "FileName '%ws'", pwzFileName);
            WcaLog(LOGMSG_STANDARD, "Flag = '%d'.", iFlags);
//            WcaLog(LOGMSG_STANDARD, "Sequence = '%d'.", iSequence);
//            WcaLog(LOGMSG_STANDARD, "Directory '%ws'", pwzDirectory);
            WcaLog(LOGMSG_STANDARD, "Shortcut Directory '%ws'", pwzShortcutPath);

            hr = WcaWriteStringToCaData(pwzShortcutPath, &pwzUnInstallCustomActionData);
            ExitOnFailure(hr, "failed to write shortcut path to custom uninstall action data");

            hr = WcaWriteIntegerToCaData(iFlags, &pwzUnInstallCustomActionData);
            ExitOnFailure(hr, "failed to write flags to custom uninstall action data.");

            ++cUnInstallDrivers;
        }
    }

    if (E_NOMOREITEMS == hr)
    {
        hr = S_OK;
    }
    ExitOnFailure(hr, "Failure occured while processing WixInternetShortcut table");

    if (pwzInstallCustomActionData && *pwzInstallCustomActionData)
    {
        hr = WcaDoDeferredAction(L"RollbackInstall", pwzInstallCustomActionData, cInstallDrivers * COST_DRIVER_EXCEPTION);
        ExitOnFailure(hr, "failed to schedule RollbackInstall action");
        hr = WcaDoDeferredAction(L"InstallDriverPackages", pwzInstallCustomActionData, cInstallDrivers * COST_DRIVER_EXCEPTION);
        ExitOnFailure(hr, "failed to schedule InstallDriverPackages action");
    }

    if (pwzUnInstallCustomActionData && *pwzUnInstallCustomActionData)
    {
        hr = WcaDoDeferredAction(L"RollbackUninstall", pwzUnInstallCustomActionData, cUnInstallDrivers * COST_DRIVER_EXCEPTION);
        ExitOnFailure(hr, "failed to schedule RollbackUninstall action");
        hr = WcaDoDeferredAction(L"UninstallDriverPackages", pwzUnInstallCustomActionData, cUnInstallDrivers * COST_DRIVER_EXCEPTION);
        ExitOnFailure(hr, "failed to schedule UninstallDriverPackages action");
    }

LExit:
    ReleaseStr(pwzInstallCustomActionData);
    ReleaseStr(pwzUnInstallCustomActionData);
    ReleaseStr(pwzComponent);
    ReleaseStr(pwzFileName);
    ReleaseStr(pwzDirectory);
    ReleaseStr(pwzShortcutPath);
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall InstallPackages(
    MSIHANDLE hInstall
)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;
    LPWSTR pwzCustomActionData = NULL;
    LPWSTR pwzData = NULL;
    LPWSTR pwz = NULL;
    int iFlags = 0;
    bool bRebootNeeded = false;

    hr = WcaInitialize(hInstall, "InstallPackages");
    ExitOnFailure(hr, "Failed to initialize InstallPackages");

    WcaLog(LOGMSG_STANDARD, "Initialized.");


    hr = WcaGetProperty(L"CustomActionData", &pwzCustomActionData);
    ExitOnFailure(hr, "Failed to get Custom Action Data.");
    WcaLog(LOGMSG_STANDARD, "Custom Action Data = '%ws'.", pwzCustomActionData);

    pwz = pwzCustomActionData;

    while (pwz && *pwz)
    {
        hr = WcaReadStringFromCaData(&pwz, &pwzData);
        ExitOnFailure(hr, "failed to read package name from install custom action data");
        hr = WcaReadIntegerFromCaData(&pwz, &iFlags);
        ExitOnFailure(hr, "Failed to read package flags from install custom action data");
        WcaLog(LOGMSG_STANDARD, "Install of '%ws' flag %d.", pwzData, iFlags);
        hr = Install(pwzData, &bRebootNeeded);
        WcaLog(LOGMSG_STANDARD, "Completed with status 0x%x.", hr);
        if (FAILED(hr))
        {
            LogReport(S_OK, L"failed to execute Install command (with error 0x%x): %ws, continuing anyway", hr, pwzData);
            hr = S_OK;
        }
        if (iFlags & REBOOT_AFTER_INSTALL)
        {
            bRebootNeeded = true;
        }
        hr = WcaProgressMessage(COST_DRIVER_EXCEPTION, FALSE);
        ExitOnFailure1(hr, "failed to tick progress bar for driver: %ws", pwzData);
    }
    if (bRebootNeeded )
    {
        WcaDeferredActionRequiresReboot();
    }
    DevDriver::ScanForHardwareChanges();

LExit:
    ReleaseStr(pwz);
    ReleaseStr(pwzData);
    ReleaseStr(pwzCustomActionData);
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall UninstallPackages(
    MSIHANDLE hInstall
)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;
    LPWSTR pwzCustomActionData = NULL;
    LPWSTR pwzData = NULL;
    LPWSTR pwz = NULL;
    int iFlags = 0;
    bool bRebootNeeded = false;

    hr = WcaInitialize(hInstall, "UninstallPackages");
    ExitOnFailure(hr, "Failed to initialize UninstallPackages");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    hr = WcaGetProperty(L"CustomActionData", &pwzCustomActionData);
    ExitOnFailure(hr, "Failed to get Custom Action Data.");
    WcaLog(LOGMSG_STANDARD, "Custom Action Data = '%ws'.", pwzCustomActionData);

    pwz = pwzCustomActionData;

    while (pwz && *pwz)
    {
        hr = WcaReadStringFromCaData(&pwz, &pwzData);
        ExitOnFailure(hr, "failed to read command line from custom action data");
        hr = WcaReadIntegerFromCaData(&pwz, &iFlags);
        ExitOnFailure(hr, "Failed to read package flags from install custom action data");

        hr = Uninstall(pwzData, &bRebootNeeded);
        if (FAILED(hr))
        {
            WcaLog(LOGMSG_STANDARD, "failed to execute Uninstall command (with error 0x%x): %ws, continuing anyway", hr, pwzData);
            hr = S_OK;
        }
        if (iFlags & REBOOT_AFTER_UNINSTALL)
        {
            bRebootNeeded = true;
        }
    }
    if (bRebootNeeded)
    {
        WcaDeferredActionRequiresReboot();
    }

    DevDriver::ScanForHardwareChanges();

LExit:
    ReleaseStr(pwzCustomActionData);
    ReleaseStr(pwzData);
    ReleaseStr(pwz);
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

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

HRESULT Install(const PCWSTR filename, bool* bReboorNeeded)
{
    DevDriver* dp = new DevDriver(filename);
    if (dp)
    {
        if (dp->Init())
        {
            dp->Install();
            if (bReboorNeeded != NULL)
            {
                *bReboorNeeded = dp->IsRebootNeeded();
            }
        }
        delete dp;
        return S_OK;
    }
    LogReport(S_OK, L"%ws filename = %ws FAILED", WFUNCTION, filename);
    return E_OUTOFMEMORY;
}

HRESULT Uninstall(const PCWSTR filename, bool* bReboorNeeded)
{
    DevDriver* dp = new DevDriver(filename);
    if (dp)
    {
        if (dp->Init(false))
        {
            dp->UnInstall();
            if (bReboorNeeded != NULL)
            {
                *bReboorNeeded = dp->IsRebootNeeded();
            }
        }
        delete dp;
        return S_OK;
    }
    return E_OUTOFMEMORY;
}
