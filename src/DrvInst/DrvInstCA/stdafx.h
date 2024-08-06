#pragma once

#include <string>
#include <vector>

#include <windows.h>
#include <newdev.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <RegStr.h>

#include <msiquery.h>


#include <Wbemidl.h>
#include <comdef.h>
#include <assert.h>
#include "Wbemidl.h"
#include <atlbase.h>
#include <atlsafe.h>
#include "comutil.h"
#include <errno.h>



#include <codecvt>
#include<algorithm>
#include <system_error>
#include <strsafe.h>


// WiX Header Files:
#include <wcautil.h>
#include <strutil.h>

const int ERR_BUF_SIZE = 1024;

void LogReport(
    __in HRESULT hr,
    __in_z __format_string PCWSTR fmt, ...
);


#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)
#define WFUNCTION WIDE1(__FUNCTION__)
