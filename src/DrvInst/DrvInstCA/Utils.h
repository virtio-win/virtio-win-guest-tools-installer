#pragma once

#include <string>
bool ReadStaticIPOrder(std::wstring& ipaddr, std::wstring& subnet, const std::wstring& guid);
std::wstring ReadConnectionName(const std::wstring& guid);
bool WriteConnectionName(const std::wstring& guid, const std::wstring& name);
