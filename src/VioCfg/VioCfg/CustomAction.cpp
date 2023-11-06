/*
 * Copyright (C) 2023 Red Hat, Inc.
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

static HRESULT Save(const PCWSTR filename);
static HRESULT Restore(const PCWSTR filename);

UINT __stdcall SaveSettings(
    MSIHANDLE hInstall
    )
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "SaveSettings");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "SaveSettings Initialized.");

    LPWSTR pszCustomActionData = NULL;
    hr = WcaGetProperty(L"CustomActionData", &pszCustomActionData);
    ExitOnFailure(hr, "Failed to get Custom Action Data.");
    WcaLog(LOGMSG_STANDARD, "Custom Action Data = '%ls'.", pszCustomActionData);

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

    hr = WcaInitialize(hInstall, "RestoreSettings");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "RestoreSettings Initialized.");

    LPWSTR pszCustomActionData = NULL;
    hr = WcaGetProperty(L"CustomActionData", &pszCustomActionData);
    ExitOnFailure(hr, "Failed to get Custom Action Data.");
    WcaLog(LOGMSG_STANDARD, "Custom Action Data = '%ls'.", pszCustomActionData);

    hr = Restore(pszCustomActionData);
    ExitOnFailure(hr, "Custom action failed");

LExit:
    ReleaseStr(pszCustomActionData);
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}


// DllMain - Initialize and cleanup WiX custom action utils.
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


HRESULT RsSettings(LPWSTR filename)
{
    HRESULT hr = E_FAIL;
    return hr;
}

HRESULT SvSettings(LPWSTR filename)
{
    HRESULT hr = E_FAIL;
    return hr;
}

HRESULT Save(const PCWSTR filename)
{
    LogReport(S_OK, L"%ws filename = %ws\n", WFUNCTION, filename);
    ConfigWrite* cfg = new ConfigWrite(filename);
    if (!cfg->Init(false)) {
        delete cfg;
        LogReport(S_OK, L"%cfg Init failed\n");
        return E_FAIL;
    }
    cfg->Run();
    delete cfg;
    return NOERROR;
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
    return NOERROR;
    ;
}
