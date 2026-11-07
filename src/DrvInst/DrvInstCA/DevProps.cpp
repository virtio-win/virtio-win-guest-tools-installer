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
    m_ClassGUID = GUID_NULL;
    ZeroMemory(&m_DriverVersion, sizeof(m_DriverVersion));
};

bool DevProps::Init(bool bInstall)
{
    if (!bInstall && !FileExists())
    {
        if (!GetComponentRegKey()) return false;
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
    LogReport(S_OK, L"%ws FAILED", WFUNCTION);
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
    static thread_local wchar_t buf[ERR_BUF_SIZE];

    LPWSTR msgbuf = nullptr;
    DWORD len = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        Error,
        0,
        (LPWSTR)&msgbuf,
        0,
        nullptr);
    if (len > 0 && msgbuf != nullptr)
    {
        wcsncpy_s(buf, ERR_BUF_SIZE, msgbuf, _TRUNCATE);
        LocalFree(msgbuf);
    }
    else
    {
        if (msgbuf != nullptr)
            LocalFree(msgbuf);
        wcscpy_s(buf, ERR_BUF_SIZE, L"Unknown error");
    }

    return buf;
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
        std::wstring oem_name(name);
        m_OemName = oem_name.substr(oem_name.find_last_of(L"\\") + 1);
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
    Err = GetLastError();
    if (!Err) Err = ERROR_NOT_FOUND;
    LogReport(HRESULT_FROM_WIN32(Err), L"Failed to get the strong name for %ws %ws  failed err = 0x%x: %ws", inffile.c_str(), inffile.c_str(), Err, GetErrorString(Err));
    SetLastError(Err);
    return false;
}

static bool ReadInfBytes(PCWSTR path, std::vector<BYTE>& bytes)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    DWORD error = ERROR_SUCCESS, read = 0;
    if (!GetFileSizeEx(file, &size)) error = GetLastError();
    else if (size.QuadPart <= 0 || size.QuadPart > MAXDWORD) error = ERROR_INVALID_DATA;
    else
    {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        if (!ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, NULL))
            error = GetLastError();
        else if (read != bytes.size()) error = ERROR_HANDLE_EOF;
    }
    CloseHandle(file);
    SetLastError(error);
    return error == ERROR_SUCCESS;
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
    std::vector<BYTE> source;
    if (!ReadInfBytes(GetFullPath(), source)) return false;

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

        std::vector<BYTE> candidate;
        if (!ReadInfBytes(oem_file.c_str(), candidate))
        {
            Err = GetLastError();
            break;
        }
        if (candidate == source)
        {
            if (bfound)
            {
                Err = ERROR_DUP_NAME;
                break;
            }
            SetOemName(oem_file.c_str(), oem_file.length());
            bfound = true;
        }

    } while (FindNextFile(hFind, &FindFileData));

    if (Err == ERROR_SUCCESS && GetLastError() != ERROR_NO_MORE_FILES)
        Err = GetLastError();
    FindClose(hFind);
    if (Err == ERROR_SUCCESS && !bfound) Err = ERROR_NOT_FOUND;
    SetLastError(Err);
    return bfound && Err == ERROR_SUCCESS;
}

bool DevProps::SetComponentRegKey()
{
    HKEY key;
    std::wstring path = L"SOFTWARE\\RedHat\\Virtio-Win\\Components\\" + GetComponentName();
    LONG error = RegCreateKeyEx(HKEY_LOCAL_MACHINE, path.c_str(), 0, NULL, 0,
        KEY_SET_VALUE, NULL, &key, NULL);
    if (error == ERROR_SUCCESS)
    {
        std::wstring oem = GetOemName(), strong = GetStrongName();
        error = RegSetValueEx(key, L"oem", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(oem.c_str()), static_cast<DWORD>((oem.size() + 1) * sizeof(wchar_t)));
        if (error == ERROR_SUCCESS)
            error = RegSetValueEx(key, L"strongname", 0, REG_SZ,
                reinterpret_cast<const BYTE*>(strong.c_str()), static_cast<DWORD>((strong.size() + 1) * sizeof(wchar_t)));
        LONG closed = RegCloseKey(key);
        if (error == ERROR_SUCCESS) error = closed;
    }
    SetLastError(error);
    return error == ERROR_SUCCESS;
}

bool DevProps::GetComponentRegKey()
{
    std::wstring key = L"SOFTWARE\\RedHat\\Virtio-Win\\Components\\" + GetComponentName();
    wchar_t strong[MAX_PATH], oem[MAX_PATH];
    DWORD size = sizeof(strong);
    LONG error = RegGetValueW(HKEY_LOCAL_MACHINE, key.c_str(), L"strongname",
        RRF_RT_REG_SZ, NULL, strong, &size);
    if (error == ERROR_SUCCESS)
    {
        size = sizeof(oem);
        error = RegGetValueW(HKEY_LOCAL_MACHINE, key.c_str(), L"oem",
            RRF_RT_REG_SZ, NULL, oem, &size);
    }
    if (error == ERROR_SUCCESS)
    {
        m_InfName = strong;
        SetOemName(oem, wcslen(oem));
    }
    SetLastError(error);
    return error == ERROR_SUCCESS;
}

DWORD DevProps::DeleteComponentRegKey()
{
    std::wstring key = L"SOFTWARE\\RedHat\\Virtio-Win\\Components\\" + GetComponentName();
    LONG error = RegDeleteKey(HKEY_LOCAL_MACHINE, key.c_str());
    return error == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : error;
}
