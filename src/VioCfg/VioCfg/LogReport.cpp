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
        WcaLogError(hr, "xru %s\n", ws2s(buffer).c_str());
        LogError(hr);
    }
    else
    {
        WcaLog(LOGMSG_STANDARD, "xru %s\n", ws2s(buffer).c_str());
    }

    free(buffer);
}
