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
#include <codecvt>
#include <string>
#include "stdafx.h"

#include <lmerr.h>

#define ERRMSGBUFFERSIZE 1024

std::string ws2s(const std::wstring& wstr)
{
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

void LogError(DWORD dwErrorMsgId)
{
    HLOCAL pBuffer;
    DWORD ret = 0;
    HINSTANCE hInst = NULL;
    BOOL doLookup = TRUE;
    DWORD dwMessageId = dwErrorMsgId;
    LPCSTR pMessage = "Error %d";
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;

    if (HRESULT_FACILITY(dwErrorMsgId) == FACILITY_MSMQ) {
        hInst = LoadLibrary(TEXT("MQUTIL.DLL"));
        flags |= FORMAT_MESSAGE_FROM_HMODULE;
        doLookup = (NULL != hInst);
    }
    else if (dwErrorMsgId >= NERR_BASE && dwErrorMsgId <= MAX_NERR) {
        hInst = LoadLibrary(TEXT("NETMSG.DLL"));
        flags |= FORMAT_MESSAGE_FROM_HMODULE;
        doLookup = (NULL != hInst);
    }
    else if (HRESULT_FACILITY(dwErrorMsgId) == FACILITY_WIN32) {
        dwMessageId &= 0x0000FFFF;
        flags |= FORMAT_MESSAGE_FROM_SYSTEM;
    }

    if (doLookup) {
        ret = FormatMessageA(
            flags,
            hInst,
            dwMessageId,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&pBuffer,
            ERRMSGBUFFERSIZE,
            NULL
        );
    }

    if (0 < ret && NULL != pBuffer) {
        pMessage = (LPSTR)pBuffer;
    }

    WcaLogError(dwErrorMsgId, pMessage, dwMessageId);
    LocalFree(pBuffer);
}

void LogReport(
    __in HRESULT hr,
    __in_z __format_string PCWSTR fmt, ...
)
{
    va_list args;
    int len;
    wchar_t* buffer;

    va_start(args, fmt);
#pragma warning( push )
#pragma warning( disable : 4996 )
    len = vswprintf(NULL, 0, fmt, args) + 1;
#pragma warning( pop )
    buffer = (wchar_t*)malloc(len * sizeof(wchar_t));

#ifdef _DEBUG
    ::ZeroMemory(buffer, len);
#endif
    vswprintf(buffer, len, fmt, args);

    if (FAILED(hr))
    {
        WcaLogError(hr, "Virtio-Win Installer %s\n", ws2s(buffer).c_str());
        LogError(hr);
    }
    else
    {
        WcaLog(LOGMSG_STANDARD, "Virtio-Win Installer %s\n", ws2s(buffer).c_str());
    }

    free(buffer);
}
