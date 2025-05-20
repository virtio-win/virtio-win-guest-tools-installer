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
#include "configfile.h"

ConfigFile::ConfigFile(std::wstring const& name)
{
    fileHandle = nullptr;
    fileName = name;
    readMode = false;
}

ConfigFile::~ConfigFile()
{
    Close();
}

bool ConfigFile::Open(bool read)
{
    readMode = read;
    if (_wfopen_s(&fileHandle, fileName.c_str(), readMode ? L"r" : L"w") < 0)
    {
        DWORD Err = GetLastError();
        LogReport(S_OK, L"_wfopen failed err = %d :  %s", Err, GetErrorString(Err));
        return false;
    }
    return true;
}

bool ConfigFile::Close()
{
    if (fileHandle)
    {
        if(fclose(fileHandle) < 0)
        {
            DWORD Err = GetLastError();
            LogReport(S_OK, L"fclose failed err = %d :  %s", Err, GetErrorString(Err));
            return false;
        }
        fileHandle = nullptr;
    }
    return true;
}

bool ConfigFile::ReadLine(std::wstring& line)
{
    if (fileHandle && readMode)
    {
        line.resize(1024);
        if (fgetws(const_cast<wchar_t*>(line.data()), (int)line.size(), fileHandle) != NULL) {
            line.erase(line.begin() + line.rfind(_TEXT("\r")), line.end());
            return true;
        }
    }
    return false;
}

bool ConfigFile::WriteLine(wchar_t const* const fmt, ...)
{
    bool res = true;
    if (fileHandle && !readMode)
    {
        wchar_t wcstr[1024];
        va_list list;
        va_start(list, fmt);
        vswprintf(wcstr, 1024, fmt, list);
        va_end(list);

        if (_ftprintf(fileHandle, wcstr) < 0)
        {
            DWORD Err = GetLastError();
            LogReport(S_OK, L"_ftprintf failed err = %d :  %s", Err, GetErrorString(Err));
            res = false;
        }
    }
    return res;
}
