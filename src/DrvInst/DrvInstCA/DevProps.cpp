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
#include "DevProps.h"

DevProps::DevProps(const PCWSTR inf_name) : m_InfName(inf_name)
{
};

bool DevProps::Init(bool bInstall)
{

    if (!bInstall && !FileExists())
    {
        GetComponentRegKey();
    }

    if (GetLongPathname())
    {
        ObtainClassGUIDandName();
        GetVersion();
        GetCatalog();
        GetProvider();
        GetSrcFiles();
        ObtainDevHWIDs();
        return true;
    }

    return false;
}

bool DevProps::FileExists()
{
    if (!m_InfName.empty())
    {
        DWORD attr = GetFileAttributes(m_InfName.c_str());
        if ((attr == INVALID_FILE_ATTRIBUTES) ||
            (attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            DWORD Err = GetLastError();
            LogReport(HRESULT_FROM_WIN32(Err), L"GetFileAttributes failed err = 0x%x: %ws", Err, GetErrorString(Err));
            return false;
        }
        return true;
    }
    return false;
}

bool DevProps::GetLongPathname()
{
    if (FileExists())
    {
        wchar_t      buf[MAX_PATH] = { '\0' };
        DWORD        len = 0;
        if (!PathIsURL(m_InfName.c_str()) && PathIsRelative(m_InfName.c_str()))
        {
            len = GetFullPathName(m_InfName.c_str(), 0, nullptr, nullptr);
            if (len)
            {
                if ((len = GetFullPathName(m_InfName.c_str(), len, buf, nullptr)) != 0)
                {
                    m_FullPath = std::wstring(buf, len);
                }
            }
        }
        else
        {
            m_FullPath = m_InfName;
        }
    }
    return (!m_FullPath.empty());
}

std::wstring DevProps::GetInfVersion(const PCWSTR pKey)
{
    std::wstring val;
    if (!m_FullPath.empty())
    {
        DWORD size = 0;
        if (SetupGetInfInformation(GetFullPath(), INFINFO_INF_NAME_IS_ABSOLUTE, NULL, 0, &size) && size > 0)
        {
            std::unique_ptr< wchar_t[] > buf(new wchar_t[size_t(size) + 1]);
            PSP_INF_INFORMATION inf = (PSP_INF_INFORMATION)buf.get();
            if (SetupGetInfInformation(GetFullPath(), INFINFO_INF_NAME_IS_ABSOLUTE, inf, size, &size))
            {
                size = 0;
                if (SetupQueryInfVersionInformation(inf, 0, pKey, NULL, 0, &size))
                {
                    std::unique_ptr< wchar_t[] > val_buf(new wchar_t[size_t(size) + 1]);
                    val.resize(size_t(size) + 1);
                    if (SetupQueryInfVersionInformation(inf, 0, pKey, val_buf.get(), size, &size) && size > 0)
                    {
                        val = val_buf.get();
                        val.resize(size);
                        return val;
                    }
                }
            }
        }
    }
    DWORD Err = GetLastError();
    LogReport(S_OK, L"GetInfVersion failed err = 0x%x: %ws", Err, GetErrorString(Err));
    return val;
}

void DevProps::ObtainClassGUIDandName()
{
    DWORD size = MAX_CLASS_NAME_LEN;
    if (!m_FullPath.empty())
    {
        std::unique_ptr< wchar_t[] > buf(new wchar_t[size_t(size) + 1]);
        if (SetupDiGetINFClass(GetFullPath(), &m_ClassGUID, buf.get(), size, &size) && size > 0)
        {
            m_ClassName = std::wstring(buf.get(), size);
        }
    }
}

void DevProps::GetClass()
{
    m_ClassName = GetInfVersion(CLASS_TYPE);
}

void DevProps::GetCatalog()
{
    m_CatalogFile = GetInfVersion(CATALOG_FILE);
}

void DevProps::GetProvider()
{
    m_Provider = GetInfVersion(PROVIDER);
}

void DevProps::GetVersion()
{
    UCHAR dd, mm;
    UINT yy, osver, rhelver, magic, build;
    std::wstring ver = ObtainDevVersion();
    if (!ver.empty() &&
        (swscanf_s(ver.c_str(), DRIVER_VERSION_FORMAT, &mm, &dd, &yy, &osver, &rhelver, &magic, &build) == 7))
    {
        m_DriverVersion.mm = mm;
        m_DriverVersion.dd = dd;
        m_DriverVersion.yy = yy;
        m_DriverVersion.osver = osver;
        m_DriverVersion.rhelver = rhelver;
        m_DriverVersion.magic = magic;
        m_DriverVersion.build = build / 100;
    }
}

std::wstring DevProps::ObtainDevVersion()
{
    HINF hInf = nullptr;
    INFCONTEXT InfContext = { 0 };
    std::wstring version;

    hInf = SetupOpenInfFile(GetFullPath(),
        NULL,
        INF_STYLE_WIN4,
        NULL
    );

    if (hInf == INVALID_HANDLE_VALUE)
    {
        return version;
    }

    DWORD size = 0;
    if (SetupFindFirstLine(hInf,
        VERSION,
        DRIVERVER,
        &InfContext))
    {
        SetupGetMultiSzField(&InfContext,
            1,
            NULL,
            0,
            &size);
        std::unique_ptr< wchar_t[] > ver_buf(new wchar_t[size_t(size) + 1]);
        SetupGetMultiSzField(&InfContext,
            1,
            ver_buf.get(),
            size,
            &size);
        {
            std::vector<std::wstring> ver;
            ver = MultiSzToVector(ver_buf.get());
            assert(ver.size() == 2);
            version = ver[0] + L':' + ver[1];
        }
    }
    SetupCloseInfFile(hInf);
    return version.c_str();
}

void DevProps::GetSrcFiles()
{
    HINF hInf;
    INFCONTEXT InfContext;
    m_SrcFiles.clear();
    DWORD size = 0;
    hInf = SetupOpenInfFile(GetFullPath(),
        NULL,
        INF_STYLE_WIN4,
        NULL
    );

    if (hInf == INVALID_HANDLE_VALUE)
    {
        return;
    }

    if (SetupFindFirstLine(hInf,
        SOURCEDISKSFILES,
        NULL,
        &InfContext))
    {
        do
        {
            SetupGetStringField(
                &InfContext,
                0,
                NULL,
                NULL,
                &size);
            std::unique_ptr< wchar_t[] > sourc_buf(new wchar_t[size_t(size) + 1]);
            SetupGetStringField(
                &InfContext,
                0,
                sourc_buf.get(),
                size,
                &size);
            {
                std::wstring source_file(sourc_buf.get(), size_t(size) + 1);
                source_file.resize(size);
                m_SrcFiles.push_back(source_file);
            }
        } while (SetupFindNextLine(&InfContext, &InfContext));
    }

    SetupCloseInfFile(hInf);
}

std::vector<std::wstring> DevProps::MultiSzToVector(const wchar_t* buf)
{
    std::vector<std::wstring> rdata;

    if (buf)
    {
        while (*buf != '\0')
        {
            rdata.push_back(buf);
            buf += ((int)_tcslen(buf) + 1);
        }
    }
    return rdata;
}

std::vector<std::wstring> DevProps::FindCommon(std::vector<std::wstring> lval, std::vector<std::wstring> rval)
{
    std::vector<std::wstring> res(10);
    if (!lval.empty())
    {
        std::sort(lval.begin(), lval.end());
        if (!rval.empty())
        {
            std::sort(rval.begin(), rval.end());
            auto it = std::set_intersection(lval.begin(), lval.end(), rval.begin(), rval.end(), res.begin());
            res.erase(it, res.end());
        }
    }
    return res;
}

void DevProps::ObtainDevHWIDs()
{
    HINF hInf = nullptr;
    INFCONTEXT InfContext = {0};
    m_HWID.clear();

    hInf = SetupOpenInfFile(GetFullPath(),
        NULL,
        INF_STYLE_WIN4,
        NULL
    );

    if (hInf == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD size = 0;
    if (SetupFindFirstLine(hInf,
        MANUFACTURER,
        NULL,
        &InfContext))
    {
        SetupGetMultiSzField(&InfContext,
            1,
            NULL,
            0,
            &size);
        std::unique_ptr< wchar_t[] > man_buf(new wchar_t[size_t(size) + 1]);
        std::wstring manufact;
        SetupGetMultiSzField(&InfContext,
            1,
            man_buf.get(),
            size,
            &size);
        {
            std::vector<std::wstring> manuf;
            manuf = MultiSzToVector(man_buf.get());
            manufact = manuf[0] + L'.' + manuf[1];
        }

        if (SetupFindFirstLine(hInf,
            manufact.c_str(),
            NULL,
            &InfContext))
        {
            do
            {
                SetupGetMultiSzField(&InfContext,
                    2,
                    NULL,
                    0,
                    &size);
                std::unique_ptr< wchar_t[] > hwid_buf(new wchar_t[size]);
                std::vector<std::wstring> hwids;
                SetupGetMultiSzField(&InfContext,
                    2,
                    hwid_buf.get(),
                    size,
                    &size);
                {
                    hwids = MultiSzToVector(hwid_buf.get());
                    m_HWID.insert(m_HWID.end(), hwids.begin(), hwids.end());
                }

            } while (SetupFindNextLine(&InfContext, &InfContext));
        }
    }
    SetupCloseInfFile(hInf);
}

const wchar_t* GetErrorString(DWORD Error)
{
    std::string err_msg = std::system_category().message(Error);
    std::wstring msg (std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(err_msg));
//    msg = L"Error code 0x%x, %ws", Error, std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(err_msg);
    return msg.c_str();
}

const wchar_t* DevProps::GetFullPath()
{
    if (!m_FullPath.empty())
    {
        return m_FullPath.c_str();
    }
    return m_FullPath.c_str();
}

LPCGUID DevProps::GetClassGUID()
{
    return &m_ClassGUID;
}

std::vector<std::wstring> DevProps::GetHWIDs()
{
    return m_HWID;
}

const wchar_t* DevProps::GetClassName()
{
    if (!m_ClassName.empty())
    {
        return m_ClassName.c_str();
    }
    return m_ClassName.c_str();
}

const wchar_t* DevProps::GetOemName()
{
    if (!m_OemName.empty())
    {
        return m_OemName.c_str();
    }
    return m_OemName.c_str();
}

bool DevProps::SetOemName(const wchar_t* name, size_t size)
{
    if (m_OemName.empty())
    {
        std::wstring oem_name(name, size);
        std::size_t pos = oem_name.find_last_of(L"\\");
        ++pos;
        m_OemName = oem_name.substr(pos);;
        m_OemName.resize(size);
    }
    return (!m_OemName.empty());
}

const wchar_t* DevProps::GetStrongName()
{
    if (!m_StrongName.empty())
    {
        return m_StrongName.c_str();
    }
    return m_StrongName.c_str();
}

bool DevProps::SetStrongName(const wchar_t* name, size_t size)
{
    if (m_StrongName.empty() && (name != NULL))
    {
        m_StrongName = name;
        if (size > 0)
        {
            m_StrongName.resize(size);
        }
    }
    return (!m_StrongName.empty());
}

PDRV_VERSION DevProps::GetDriverVersion()
{
    return &m_DriverVersion;
}

std::wstring DevProps::GetComponentName()
{
    std::wstring component_name = m_InfName;
    if (!component_name.empty())
    {
        std::size_t pos = component_name.find_last_of(L"\\") + 1;
        component_name = component_name.substr(pos);
        pos = component_name.find_last_of(L".");
        component_name = component_name.substr(0, pos);
    }
    return component_name;
}

const wchar_t* DevProps::GetProviderName()
{
    if (!m_Provider.empty())
    {
        return m_Provider.c_str();
    }
    return m_Provider.c_str();
}

const wchar_t* DevProps::GetCatalogFile()
{
    if (!m_CatalogFile.empty())
    {
        return m_CatalogFile.c_str();
    }
    return m_CatalogFile.c_str();
}

bool DevProps::FindStrongName()
{
    DWORD size = 0;
    DWORD Err = ERROR_SUCCESS;
    BOOL bRes = FALSE;
 
    std::wstring inffile = GetOemName();

    if (inffile.empty())
    {
        if (!FindOemFile())
        {
            return false;
        }
        inffile = GetOemName();
    }
    if (!inffile.empty())
    {
        std::vector<wchar_t> name_buf(MAX_PATH, '\0');

        bRes = SetupGetInfDriverStoreLocation(inffile.c_str(),
            NULL,
            NULL,
            name_buf.data(),
            (DWORD)name_buf.size(),
            &size);
        if (bRes == FALSE)
        {
            Err = GetLastError();
            if (Err == ERROR_INSUFFICIENT_BUFFER)
            {
                name_buf.resize(size);
                bRes = SetupGetInfDriverStoreLocation(inffile.c_str(),
                    NULL,
                    NULL,
                    name_buf.data(),
                    (DWORD)name_buf.size(),
                    &size);
            }
        }
        if (bRes == TRUE)
        {
            SetStrongName(name_buf.data(), size);
            return true;
        }
    }
    LogReport(HRESULT_FROM_WIN32(Err), L"Failed to get the strong name for %ws %ws  failed err = 0x%x: %ws", inffile.c_str(), inffile.c_str(), Err, GetErrorString(Err));
    return false;
}

bool DevProps::FindOemFile()
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;
    UINT size = MAX_PATH;
    std::unique_ptr< wchar_t[] > sys_buf(new wchar_t[size]);
    std::wstring sys_path;
    DWORD Err = ERROR_SUCCESS;
    bool bfound = false;

    size = GetSystemWindowsDirectory(sys_buf.get(), size);
    sys_path.resize(size + 1);
    if (size == 0)
    {
        Err = GetLastError();
        sys_path = L"C:\\Windows\\INF\\";
    }
    else
    {
        sys_path = sys_buf.get();
        sys_path.resize(size);
        sys_path += L"\\INF\\";
    }

    std::wstring oem_temp = sys_path + L"oem*.inf";

    hFind = FindFirstFile(oem_temp.c_str(), &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        Err = GetLastError();
        LogReport(HRESULT_FROM_WIN32(Err), L"FindFirstFile  failed err = 0x%x: %ws", Err, GetErrorString(Err));
        return false;
    }

    do
    {
        std::wstring oem_file = sys_path;
        std::wstring file = FindFileData.cFileName;
        oem_file += file;

        DevProps* tmp_dp = new DevProps(oem_file.c_str());
        tmp_dp->Init();

        if (IsEqualGUID(*tmp_dp->GetClassGUID(), *GetClassGUID()) &&
            (wcscmp(tmp_dp->GetProviderName(), GetProviderName()) == 0) &&
            (wcscmp(tmp_dp->GetCatalogFile(), GetCatalogFile()) == 0) &&
            tmp_dp->GetDriverVersion()->build == GetDriverVersion()->build)
        {
            SetOemName(oem_file.c_str(), oem_file.length());
            bfound = true;
            break;
        }
        delete tmp_dp;

    } while (FindNextFile(hFind, &FindFileData));

    FindClose(hFind);

    return bfound;
}

bool DevProps::SetComponentRegKey()
{
    HKEY hRootKey;
    std::wstring rootkey = TEXT("SOFTWARE\\RedHat\\Virtio-Win\\Components\\");
    std::wstring dirkey = GetComponentName();
    rootkey += dirkey;
    LONG nError = ERROR_SUCCESS;

    nError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, rootkey.c_str(), 0, KEY_ALL_ACCESS, &hRootKey);

    if (nError == ERROR_FILE_NOT_FOUND)
    {
        nError = RegCreateKeyEx(HKEY_LOCAL_MACHINE, rootkey.c_str(), NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRootKey, NULL);
    }

    if (nError != ERROR_SUCCESS)
    {
        LogReport(HRESULT_FROM_WIN32(nError), L"Error opening or creating a key err = 0x%x: %ws", nError, GetErrorString(nError));
        return false;
    }

    std::wstring oem_name = GetOemName();
    if (!oem_name.empty())
    {
        std::wstring  value = TEXT("oem\0");
        nError = RegSetValueEx(hRootKey, value.c_str(), 0, REG_SZ, (LPBYTE)oem_name.c_str(), (DWORD)((oem_name.length() + 1) * sizeof(wchar_t)));

        if (nError != ERROR_SUCCESS)
        {
            LogReport(HRESULT_FROM_WIN32(nError), L"RegSetValueEx for oem value failed err = 0x%x: %ws", nError, GetErrorString(nError));
        }
    }

    std::wstring strong_name = GetStrongName();
    if (!strong_name.empty())
    {
        std::wstring  value = TEXT("strongname\0");
        nError = RegSetValueEx(hRootKey, value.c_str(), 0, REG_SZ, (LPBYTE)strong_name.c_str(), (DWORD)((strong_name.length() + 1) * sizeof(wchar_t)));

        if (nError != ERROR_SUCCESS)
        {
            LogReport(HRESULT_FROM_WIN32(nError), L"RegSetValueEx for strongname value failed err = 0x%x: %ws", nError, GetErrorString(nError));
        }
    }

    nError = RegCloseKey(hRootKey);

    if (nError != ERROR_SUCCESS)
    {
        LogReport(HRESULT_FROM_WIN32(nError), L"RegCloseKey failed err = 0x%x: %ws", nError, GetErrorString(nError));
        return false;
    }
    return true;
}

bool DevProps::GetComponentRegKey()
{
    HKEY hRootKey;
    std::wstring rootkey = TEXT("SOFTWARE\\RedHat\\Virtio-Win\\Components\\");
    std::wstring dirkey = GetComponentName();// TEXT("vioscsi\0");
    rootkey += dirkey;
    LONG nError = ERROR_SUCCESS;

    nError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, rootkey.c_str(), 0, KEY_ALL_ACCESS, &hRootKey);

    if (nError != ERROR_SUCCESS)
    {
        LogReport(HRESULT_FROM_WIN32(nError), L"RegOpenKeyEx failed err = 0x%x: %ws key %ws", nError, GetErrorString(nError), rootkey.c_str());
        return false;
    }

    DWORD SubKeysCount = 0;
    DWORD ValuesCount = 0;
    nError = RegQueryInfoKey(hRootKey, NULL, NULL, NULL, &SubKeysCount, NULL, NULL, &ValuesCount, NULL, NULL, NULL, NULL);
    if (nError == ERROR_SUCCESS)
    {
        std::wstring  strongname = TEXT("strongname");
        std::wstring  oemname = TEXT("oem");
        for (DWORD i = 0; i < ValuesCount; i++)
        {
            DWORD buffSize = 1024;
            wchar_t valueBuff[1024];
            DWORD subKeyCount = 0;
            HKEY hSubKey = hRootKey;
            RegEnumValue(hRootKey, i, valueBuff, &buffSize, NULL, NULL, NULL, NULL);

            std::wstring valueName = valueBuff;
            DWORD dwType = REG_SZ;
            wchar_t value[1024];
            DWORD value_length = 1024;
            nError = RegQueryValueEx(hRootKey, valueName.c_str(), NULL, &dwType, (LPBYTE)&value, &value_length);
            if (nError == ERROR_SUCCESS)
            {
                if ((wcscmp(valueName.c_str(), strongname.c_str()) == 0))
                {
                    m_InfName = value;
                    GetLongPathname();
                }
                else if (wcscmp(valueName.c_str(), oemname.c_str()) == 0)
                {
                    SetOemName(value, _tcslen(value));
                    FindStrongName();
                }
            }
            else
            {
                LogReport(HRESULT_FROM_WIN32(nError), L"RegQueryValueEx failed err = 0x%x: %ws", nError, GetErrorString(nError));
            }
        }
    }

    nError = RegDeleteKey(HKEY_LOCAL_MACHINE, rootkey.c_str());
    if (nError != ERROR_SUCCESS)
    {
        LogReport(HRESULT_FROM_WIN32(nError), L"RegDeleteKey failed err = 0x%x: %ws key = %ws", nError, GetErrorString(nError), rootkey.c_str());
    }

    nError = RegCloseKey(hRootKey);

    if (nError != ERROR_SUCCESS) {
        LogReport(HRESULT_FROM_WIN32(nError), L"RegCloseKey failed err = 0x%x: %ws", nError, GetErrorString(nError));
        return false;
    }

    return true;
}

DevDriver::DevDriver(const PCWSTR inf_name) : DevProps(inf_name), m_bRebootNeeded(false), m_pDiUninstallDriver(NULL)
{
    FindFunctions();
}

bool DevDriver::UnInstall()
{
    RemoveDevices();
    UnInstallOEMInf();
    return true;
}

bool DevDriver::Install()
{
    DWORD Err = UpdateDriver();

    if (Err == ERROR_NO_SUCH_DEVINST)
    {
        Err = InstallOEMInf();
    }

    if ((Err == NO_ERROR) || (Err == ERROR_NO_SUCH_DEVINST))
    {
        HDEVINFO NonPresentDevs = INVALID_HANDLE_VALUE;
        NonPresentDevs = FindNonPresentDevices();
        if (NonPresentDevs != INVALID_HANDLE_VALUE)
        {
            MarkDevicesAsNeedReinstall(NonPresentDevs);
            SetupDiDestroyDeviceInfoList(NonPresentDevs);
        }
        FindStrongName();
        SetComponentRegKey();
        return true;
    }

    LogReport(HRESULT_FROM_WIN32(Err), L"Install failed err = 0x%x: %ws", Err, GetErrorString(Err));

    return false;
}

bool DevDriver::ScanForHardwareChanges(void)
{
    DEVINST     devInst;
    CONFIGRET   status = CR_SUCCESS;
    DWORD Err = S_OK;
    status = CM_Locate_DevNode(&devInst, NULL, CM_LOCATE_DEVNODE_NORMAL);

    if (status != CR_SUCCESS)
    {
        Err = CM_MapCrToWin32Err(status, ERROR_UNIDENTIFIED_ERROR);
        LogReport(HRESULT_FROM_WIN32(Err), L"CM_Locate_DevNode failed err = 0x%x: %ws", Err, GetErrorString(Err));
        return false;
    }

    status = CM_Reenumerate_DevNode(devInst, 0);

    if (status != CR_SUCCESS)
    {
        Err = CM_MapCrToWin32Err(status, ERROR_UNIDENTIFIED_ERROR);
        LogReport(HRESULT_FROM_WIN32(Err), L"CM_Reenumerate_DevNode failed wit err = 0x%x: %ws", Err, GetErrorString(Err));
        return false;
    }

    return true;
}

bool DevDriver::RemoveDevices(void)
{
    HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
    DWORD i = 0;
    DWORD Err = S_OK;

    SP_DEVINFO_DATA DeviceInfoData;
    DWORD RegDataType;
    bool bRet = false;

    DeviceInfoSet = SetupDiGetClassDevs(GetClassGUID(),
        NULL,
        NULL,
        DIGCF_ALLCLASSES
    );

    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        Err = GetLastError();
        LogReport(HRESULT_FROM_WIN32(Err), L"SetupDiGetClassDevs failed err = 0x%x: %ws", Err, GetErrorString(Err));
        return false;
    }

    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    DWORD size = 0;

    while (SetupDiEnumDeviceInfo(DeviceInfoSet, i++, &DeviceInfoData))
    {
        if (!SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
            &DeviceInfoData,
            SPDRP_HARDWAREID,
            &RegDataType,
            NULL,
            0,
            &size))
        {
            Err = GetLastError();
            if (Err == ERROR_INSUFFICIENT_BUFFER && size > 0)
            {
                std::unique_ptr< wchar_t[] > hwid_buf(new wchar_t[size]);
                if (SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
                    &DeviceInfoData,
                    SPDRP_HARDWAREID,
                    &RegDataType,
                    (PBYTE)hwid_buf.get(),
                    size,
                    &size))
                {
                    if ((RegDataType == REG_MULTI_SZ) || size > 0)
                    {
                        std::vector<std::wstring> hwids;
                        std::vector<std::wstring> res;
                        hwids = MultiSzToVector(hwid_buf.get());
                        res = FindCommon(GetHWIDs(), hwids);
                        if (!res.empty())
                        {
                            for (auto it = begin(res); it != end(res); ++it)
                            {
                                bRet = true;
                                SP_REMOVEDEVICE_PARAMS removeParameters{0};
                                SP_DRVINFO_DATA DriverInfoData{0};
                                HKEY hKey = NULL;
                                DWORD RegDataLength;
                                DWORD RegDataType;
                                DWORD RegErr;

                                removeParameters.ClassInstallHeader.cbSize =
                                    sizeof(removeParameters.ClassInstallHeader);
                                removeParameters.ClassInstallHeader.InstallFunction =
                                    DIF_REMOVE;
                                removeParameters.Scope = DI_REMOVEDEVICE_GLOBAL;
                                removeParameters.HwProfile = 0;

                                SP_DEVINSTALL_PARAMS deviceInstallParams;
                                deviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);

                                if (!SetupDiGetDeviceInstallParams(DeviceInfoSet, &DeviceInfoData, &deviceInstallParams))
                                {
                                    Err = GetLastError();
                                    LogReport(HRESULT_FROM_WIN32(Err), L"SetupDiGetDeviceInstallParams failed err = 0x%x: %ws", Err, GetErrorString(Err));
                                    return false;
                                }
                                deviceInstallParams.FlagsEx |= (DI_FLAGSEX_INSTALLEDDRIVER | DI_FLAGSEX_ALLOWEXCLUDEDDRVS);

                                if (SetupDiSetDeviceInstallParams(DeviceInfoSet, &DeviceInfoData, &deviceInstallParams))
                                {
                                    if (!SetupDiBuildDriverInfoList(DeviceInfoSet, &DeviceInfoData, SPDIT_CLASSDRIVER))
                                    {
                                        Err = GetLastError();
                                        LogReport(HRESULT_FROM_WIN32(Err), L"SetupDiBuildDriverInfoList failed err = 0x%x: %ws", Err, GetErrorString(Err));
                                        return FALSE;
                                    }
                                    DriverInfoData.cbSize = sizeof(DriverInfoData);
                                    if (!SetupDiEnumDriverInfo(DeviceInfoSet, &DeviceInfoData, SPDIT_CLASSDRIVER,
                                        0, &DriverInfoData)) 
                                    {
                                        Err = GetLastError();
                                        LogReport(HRESULT_FROM_WIN32(Err), L"SetupDiEnumDriverInfo failed err = 0x%x: %ws", Err, GetErrorString(Err));
                                        return false;
                                    }
                                }
                                deviceInstallParams.FlagsEx &= ~(DI_FLAGSEX_INSTALLEDDRIVER | DI_FLAGSEX_ALLOWEXCLUDEDDRVS);
                                hKey = SetupDiOpenDevRegKey(DeviceInfoSet,
                                    &DeviceInfoData,
                                    DICS_FLAG_GLOBAL,
                                    0,
                                    DIREG_DRV,
                                    KEY_READ
                                );

                                if (hKey == INVALID_HANDLE_VALUE) {
                                    Err = GetLastError();
                                    LogReport(HRESULT_FROM_WIN32(Err), L"SetupDiOpenDevRegKey failed err = 0x%x: %ws", Err, GetErrorString(Err));
                                    RegCloseKey(hKey);
                                    return false;
                                }

                                RegDataLength = sizeof(deviceInstallParams.DriverPath);
                                RegErr = RegQueryValueEx(hKey,
                                    REGSTR_VAL_INFPATH,
                                    NULL,
                                    &RegDataType,
                                    (PBYTE)deviceInstallParams.DriverPath,
                                    &RegDataLength
                                );

                                if ((RegErr != ERROR_SUCCESS) || (RegDataType != REG_SZ))
                                {
                                    Err = GetLastError();
                                    LogReport(HRESULT_FROM_WIN32(Err), L"RegQueryValueEx failed err = 0x%x: %ws", RegErr, GetErrorString(RegErr));
                                    RegCloseKey(hKey);
                                    return false;
                                }

                                SetOemName(deviceInstallParams.DriverPath, RegDataLength);

                                SetupDiSetClassInstallParams(DeviceInfoSet, &DeviceInfoData,
                                    &removeParameters.ClassInstallHeader,
                                    sizeof(removeParameters));

                                if (!SetupDiCallClassInstaller(DIF_REMOVE, DeviceInfoSet, &DeviceInfoData))
                                {
                                    Err = GetLastError();
                                    LogReport(HRESULT_FROM_WIN32(Err), L"SetupDiCallClassInstaller failed err = 0x%x: %ws", Err, GetErrorString(Err));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(DeviceInfoSet);

    return bRet;
}


HDEVINFO DevDriver::FindNonPresentDevices()
{
    HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
    HDEVINFO ExistingNonPresentDevices = INVALID_HANDLE_VALUE;
    DWORD i, Err;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD RegDataType;
    BOOL bRet;
    ULONG Status, Problem;
    TCHAR DeviceInstanceId[MAX_DEVNODE_ID_LEN];

    DeviceInfoSet = SetupDiGetClassDevs(GetClassGUID(),
        NULL,
        NULL,
        DIGCF_ALLCLASSES
    );

    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        Err = GetLastError();
        LogReport(HRESULT_FROM_WIN32(Err), L"SetupDiGetClassDevs failed err = 0x%x: %ws", Err, GetErrorString(Err));
        return INVALID_HANDLE_VALUE;
    }

    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    Err = NO_ERROR;
    bRet = FALSE;

    i = 0;

    DWORD size = 0;
    while (SetupDiEnumDeviceInfo(DeviceInfoSet, i++, &DeviceInfoData))
    {

        if (!SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
            &DeviceInfoData,
            SPDRP_HARDWAREID,
            &RegDataType,
            NULL,
            0,
            &size))
        {
            Err = GetLastError();
            if (Err == ERROR_INSUFFICIENT_BUFFER && size > 0)
            {
                std::unique_ptr< wchar_t[] > hwid_buf(new wchar_t[size]);
                if (SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
                    &DeviceInfoData,
                    SPDRP_HARDWAREID,
                    &RegDataType,
                    (PBYTE)hwid_buf.get(),
                    size,
                    &size)) 
                {
                    if ((RegDataType == REG_MULTI_SZ) || size > 0) 
                    {
                        std::vector<std::wstring> hwids;
                        std::vector<std::wstring> res;
                        hwids = MultiSzToVector(hwid_buf.get());
                        res = FindCommon(GetHWIDs(), hwids);
                        if (!res.empty())
                        {
                            for (auto it = begin(res); it != end(res); ++it)
                            {
                                bRet = TRUE;

                                if (CR_SUCCESS != CM_Get_DevNode_Status(&Status,
                                    &Problem,
                                    (DEVNODE)DeviceInfoData.DevInst,
                                    0))
                                {
                                    if (ExistingNonPresentDevices == INVALID_HANDLE_VALUE)
                                    {
                                        ExistingNonPresentDevices =
                                            SetupDiCreateDeviceInfoList(NULL, NULL);

                                        if (ExistingNonPresentDevices == INVALID_HANDLE_VALUE)
                                        {
                                            Err = GetLastError();
                                            LogReport(HRESULT_FROM_WIN32(Err), L"SetupDiCreateDeviceInfoList failed err = 0x%x: %ws", Err, GetErrorString(Err));

                                            bRet = FALSE;
                                            break;
                                        }
                                    }
                                    std::unique_ptr< wchar_t[] > dev_id(new wchar_t[MAX_DEVNODE_ID_LEN]);

                                    if (!SetupDiGetDeviceInstanceId(DeviceInfoSet,
                                        &DeviceInfoData,
                                        DeviceInstanceId,
                                        sizeof(DeviceInstanceId) / sizeof(TCHAR),
                                        NULL))
                                    {
                                        Err = GetLastError();
                                        LogReport(HRESULT_FROM_WIN32(Err), L"SetupDiGetDeviceInstanceId failed err = 0x%x: %ws", Err, GetErrorString(Err));
                                        bRet = FALSE;
                                        break;
                                    }

                                    if (!SetupDiOpenDeviceInfo(ExistingNonPresentDevices,
                                        DeviceInstanceId,
                                        NULL,
                                        0,
                                        NULL))
                                    {
                                        Err = GetLastError();
                                        LogReport(HRESULT_FROM_WIN32(Err), L"SetupDiOpenDeviceInfo failed err = 0x%x: %ws", Err, GetErrorString(Err));
                                        bRet = FALSE;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(DeviceInfoSet);

    if ((Err != NO_ERROR) &&
        (ExistingNonPresentDevices != INVALID_HANDLE_VALUE))
    {
        SetupDiDestroyDeviceInfoList(ExistingNonPresentDevices);
        ExistingNonPresentDevices = INVALID_HANDLE_VALUE;
    }

    return ExistingNonPresentDevices;
}

VOID DevDriver::MarkDevicesAsNeedReinstall(
    _In_ HDEVINFO DeviceInfoSet
)
{
    SP_DEVINFO_DATA DeviceInfoData{0};

    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0;
        SetupDiEnumDeviceInfo(DeviceInfoSet, i, &DeviceInfoData);
        i++)
    {
        DWORD ConfigFlags, RegDataType;

        if (SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
            &DeviceInfoData,
            SPDRP_CONFIGFLAGS,
            &RegDataType,
            (PBYTE)&ConfigFlags,
            sizeof(ConfigFlags),
            NULL)
            && (RegDataType == REG_DWORD))
        {
            ConfigFlags |= CONFIGFLAG_REINSTALL;
            SetupDiSetDeviceRegistryProperty(DeviceInfoSet,
                &DeviceInfoData,
                SPDRP_CONFIGFLAGS,
                (PBYTE)&ConfigFlags,
                sizeof(ConfigFlags)
            );
        }
    }
}

DWORD DevDriver::UpdateDriver()
{
    const wchar_t* pInf = GetFullPath();
    DWORD Err = NO_ERROR;
    if (pInf)
    {
        const std::vector<std::wstring> hwids = GetHWIDs();

        if (!hwids.empty())
        {
            for (const auto& hwid : hwids)
            {
                if (!hwid.empty())
                {
                    if (UpdateDriverForPlugAndPlayDevices(NULL, hwid.data(), pInf, INSTALLFLAG_FORCE, &m_bRebootNeeded))
                    {
                        return NO_ERROR;
                    }
                    Err = GetLastError();
                    LogReport(HRESULT_FROM_WIN32(Err), L"UpdateDriverForPlugAndPlayDevices for %ws failed err = 0x%x: %ws", hwid.data(), Err, GetErrorString(Err));
                }
            }
        }
    }
    return ERROR_NO_SUCH_DEVINST;
}

DWORD DevDriver::InstallOEMInf()
{
    const wchar_t* pInf = GetFullPath();
    DWORD Err = ERROR_FILE_NOT_FOUND;
    if (pInf)
    {
        DWORD size = MAX_PATH;
        std::unique_ptr< wchar_t[] > val_buf(new wchar_t[size]);
        PWSTR oem_name = nullptr;
        if (!SetupCopyOEMInf(pInf, NULL, SPOST_PATH, 0, val_buf.get(), size, &size, &oem_name))
        {
            Err = GetLastError();
            LogReport(HRESULT_FROM_WIN32(Err), L"SetupCopyOEMInf failed err = 0x%x: %ws", Err, GetErrorString(Err));
        }
        else
        {
            SetOemName(oem_name, _tcslen(oem_name));
            return NO_ERROR;
        }
    }
    return Err;
}

DWORD DevDriver::UnInstallOEMInf()
{
    const wchar_t* pOem = nullptr;

    if (m_pDiUninstallDriver == NULL)
    {
        pOem = GetOemName();
        if (pOem && SetupUninstallOEMInf(pOem, 0, NULL))
        {
            return NO_ERROR;
        }
    }
    else
    {
        pOem = GetStrongName();
        if (pOem && m_pDiUninstallDriver(NULL, pOem, 0/*DIURFLAG_NO_REMOVE_INF*/, &m_bRebootNeeded))
        {
            return NO_ERROR;
        }
    }

    return ERROR_FILE_NOT_FOUND;
}

void DevDriver::FindFunctions()
{
    HINSTANCE hModDll = LoadLibrary(L"newdev.dll");

    if (hModDll != NULL)
    {
        m_pDiUninstallDriver = (DiUninstallDriverProto)GetProcAddress(hModDll, "DiUninstallDriverW");
        FreeLibrary(hModDll);
    }
}
