#include "stdafx.h"
#include "ConfigClass.h"
#include <string>
#include <vector>
#include <algorithm>

static std::vector<std::wstring> ReadRegStrings(HKEY rootKey, const std::wstring& subKey, const wchar_t* valueName)
{
    std::vector<std::wstring> result;
    HKEY hKey = NULL;
    if (RegOpenKeyExW(rootKey, subKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return result;

    DWORD type = 0;
    DWORD cbSize = 0;
    LONG rc = RegQueryValueExW(hKey, valueName, nullptr, &type, nullptr, &cbSize);
    if (rc != ERROR_SUCCESS || cbSize == 0)
    {
        RegCloseKey(hKey);
        return result;
    }

    // cbSize is bytes. Allocate wchar buffer with room for extra terminator.
    size_t wcharCount = (cbSize + sizeof(wchar_t) - 1) / sizeof(wchar_t);
    std::vector<wchar_t> buf;
    buf.resize(wcharCount + 1);
    cbSize = static_cast<DWORD>(buf.size() * sizeof(wchar_t));
    rc = RegQueryValueExW(hKey, valueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(buf.data()), &cbSize);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS)
        return result;

    // Ensure null termination
    buf[buf.size() - 1] = L'\0';

    if (type == REG_MULTI_SZ)
    {
        const wchar_t* p = buf.data();
        while (*p)
        {
            result.emplace_back(p);
            p += wcslen(p) + 1;
        }
    }
    else // REG_SZ or REG_EXPAND_SZ
    {
        result.emplace_back(buf.data());
    }

    return result;
}

static const wchar_t NETWORK_CONNECTIONS_KEY[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Network"
    L"\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\";

std::wstring ReadConnectionName(const std::wstring& guid)
{
    if (guid.empty())
        return std::wstring();

    std::wstring path = NETWORK_CONNECTIONS_KEY + guid + L"\\Connection";
    std::vector<std::wstring> vals = ReadRegStrings(HKEY_LOCAL_MACHINE, path, L"Name");
    if (!vals.empty())
        return vals[0];
    return std::wstring();
}

bool WriteConnectionName(const std::wstring& guid, const std::wstring& name)
{
    if (guid.empty() || name.empty())
        return false;

    std::wstring path = NETWORK_CONNECTIONS_KEY + guid + L"\\Connection";
    HKEY hKey = NULL;
    LONG rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_SET_VALUE, &hKey);
    if (rc != ERROR_SUCCESS)
        return false;

    DWORD cbData = static_cast<DWORD>((name.size() + 1) * sizeof(wchar_t));
    rc = RegSetValueExW(hKey, L"Name", 0, REG_SZ,
        reinterpret_cast<const BYTE*>(name.c_str()), cbData);
    RegCloseKey(hKey);
    return (rc == ERROR_SUCCESS);
}

std::wstring ReadManualDNS(const std::wstring& guid)
{
    if (guid.empty())
        return std::wstring();

    std::wstring path = L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\" + guid;
    std::vector<std::wstring> vals = ReadRegStrings(HKEY_LOCAL_MACHINE, path, L"NameServer");
    if (!vals.empty() && !vals[0].empty())
        return vals[0];
    return std::wstring();
}

static bool ReadRegistryIPs(std::vector<std::wstring>& ips, std::vector<std::wstring>& masks, const std::wstring& guid)
{
    ips.clear();
    masks.clear();

    if (guid.empty())
        return false;

    HKEY hKey = NULL;
    DWORD current = 0;
    DWORD size = sizeof(current);

    LONG rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\Select", 0, KEY_READ, &hKey);
    if (rc != ERROR_SUCCESS || hKey == NULL)
        return false;

    DWORD type = 0;
    rc = RegQueryValueExW(hKey, L"Current", nullptr, &type, reinterpret_cast<LPBYTE>(&current), &size);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS || type != REG_DWORD)
        return false;

    wchar_t controlSetBuf[64] = {};
    if (swprintf_s(controlSetBuf, _countof(controlSetBuf), L"ControlSet%03u", static_cast<unsigned int>(current)) < 0)
        return false;

    std::wstring controlSet(controlSetBuf);

    std::wstring path = L"SYSTEM\\" + controlSet + L"\\Services\\Tcpip\\Parameters\\Interfaces\\" + guid;

    ips = ReadRegStrings(HKEY_LOCAL_MACHINE, path, L"IPAddress");
    masks = ReadRegStrings(HKEY_LOCAL_MACHINE, path, L"SubnetMask");
    if (masks.empty())
        masks = ReadRegStrings(HKEY_LOCAL_MACHINE, path, L"IPMask");

    return !ips.empty();
}

bool ReadStaticIPOrder(std::wstring& ipaddr, std::wstring& subnet, const std::wstring& guid)
{
    ipaddr.clear();
    subnet.clear();

    std::vector<std::wstring> ips, masks;
    if (!ReadRegistryIPs(ips, masks, guid))
        return false;

    ipaddr = ips[0];
    if (!masks.empty())
        subnet = masks[0];

    return true;
}

bool ReadAllStaticIPs(std::wstring& ipaddrs, std::wstring& subnets, const std::wstring& guid)
{
    ipaddrs.clear();
    subnets.clear();

    std::vector<std::wstring> ips, masks;
    if (!ReadRegistryIPs(ips, masks, guid))
        return false;

    for (size_t i = 0; i < ips.size(); ++i)
    {
        if (i > 0) ipaddrs += STRING_DELIM;
        ipaddrs += ips[i];
    }
    for (size_t i = 0; i < masks.size(); ++i)
    {
        if (i > 0) subnets += STRING_DELIM;
        subnets += masks[i];
    }

    return true;
}

