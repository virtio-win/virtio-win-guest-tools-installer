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
#pragma once
#include "stdafx.h"

const PCWSTR CLASS_GUID = _TEXT("ClassGUID");
const PCWSTR CLASS_TYPE = _TEXT("Class");
const PCWSTR CATALOG_FILE = _TEXT("CatalogFile");
const PCWSTR DRIVER_VERSION = _TEXT("DriverVer");
const PCWSTR DRIVER_HWID = _TEXT("hardware-id");
const PCWSTR MANUFACTURER = _TEXT("Manufacturer");
const PCWSTR PROVIDER = _TEXT("Provider");
const PCWSTR VERSION = _TEXT("Version");
const PCWSTR DRIVERVER = _TEXT("DriverVer");
const PCWSTR SOURCEDISKSFILES = _TEXT("SourceDisksFiles");

const PCWSTR DRIVER_VERSION_FORMAT = _TEXT("%hhu/%hhu/%u:%u.%u.%u.%u"); //07/04/2023,100.94.104.24700

const wchar_t* GetErrorString(DWORD Err);

typedef struct _DRV_VERSION
{
    UCHAR mm;
    UCHAR dd;
    UINT yy;
    UINT osver;
    UINT rhelver;
    UINT magic;
    UINT build;
}DRV_VERSION, * PDRV_VERSION;

class DevProps
{
public:
    DevProps(const PCWSTR inf_name);
    bool Init(bool bInstall = true );
    ~DevProps() {};
    const wchar_t* GetFullPath();
    LPCGUID GetClassGUID();
    std::vector<std::wstring> GetHWIDs();
    const wchar_t* GetClassName();
    const wchar_t* GetOemName();
    bool SetOemName(const wchar_t* name, size_t size);
    const wchar_t* GetStrongName();
    bool SetStrongName(const wchar_t* name, size_t size);
    const wchar_t* GetProviderName();
    const wchar_t* GetCatalogFile();
    PDRV_VERSION GetDriverVersion();
    std::wstring GetComponentName();

    std::vector<std::wstring> MultiSzToVector(const wchar_t* buf);
    std::vector<std::wstring> FindCommon(std::vector<std::wstring> lval, std::vector<std::wstring> rval);
private:
    bool GetLongPathname();
    bool FileExists();
    std::wstring GetInfVersion(const PCWSTR pKey);
    void ObtainClassGUIDandName();
    std::wstring ObtainDevVersion();
    void GetClass();
    void GetVersion();
    void GetCatalog();
    void GetProvider();
    void ObtainDevHWIDs();
    void GetSrcFiles();
protected:
    bool SetComponentRegKey(void);
    bool GetComponentRegKey(void);
    bool FindStrongName(void);
    bool FindOemFile();

private:
    std::wstring m_InfName;
    std::wstring m_OemName;
    std::wstring m_FullPath;
    std::wstring m_StrongName;
    GUID m_ClassGUID;
    std::wstring m_ClassName;
    std::wstring m_CatalogFile;
    std::wstring m_Provider;
    std::vector<std::wstring> m_HWID;
    std::vector<std::wstring> m_SrcFiles;
    DRV_VERSION m_DriverVersion;
};

typedef BOOL(WINAPI* DiUninstallDriverProto)(
    _In_opt_  HWND hwndParent,
    _In_      LPCWSTR InfPath,
    _In_      DWORD Flags,
    _Out_opt_ PBOOL NeedReboot);

class DevDriver : public DevProps
{
public:
    DevDriver(const PCWSTR inf_name);
    bool Install();
    bool UnInstall();
    ~DevDriver() {};
    bool IsRebootNeeded(void) { return m_bRebootNeeded; }
    static bool ScanForHardwareChanges(void);
private:
    DWORD InstallOEMInf();
    DWORD UnInstallOEMInf();
    DWORD UpdateDriver();
    HDEVINFO FindNonPresentDevices();
    VOID MarkDevicesAsNeedReinstall(
        _In_ HDEVINFO DeviceInfoSet
    );

    bool RemoveDevices(void);
    void FindFunctions(void);
private:
    BOOL m_bRebootNeeded;
    DiUninstallDriverProto m_pDiUninstallDriver;
};
