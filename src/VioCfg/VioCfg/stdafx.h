#pragma once

#include "targetver.h"

//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <string>
#include <vector>

#include <windows.h>
//#include <strsafe.h>
#include <msiquery.h>


#include <Wbemidl.h>
#include <comdef.h>
#include <assert.h>
#include "Wbemidl.h"
#include <atlbase.h>
#include <atlsafe.h>
#include "comutil.h"
#include <errno.h>

// WiX Header Files:
#include <wcautil.h>
#include <strutil.h>


// TODO: reference additional headers your program requires here

const PCWSTR CLASS_NAME = _TEXT("Win32_NetworkAdapterConfiguration");


const PCWSTR FILE_NAME = _TEXT("C:\\virtio.cfg");

const PCWSTR STRING_FORMAT = _TEXT("%s\t%d\t%s\r\n");
const PCWSTR INTEGER_FORMAT = _TEXT("%s\t%d\t%d\r\n");
const PCWSTR PARAMS_DELIM = _TEXT("\t");;
const PCWSTR STRING_DELIM = _TEXT(",");;
const PCWSTR IPV4_FORMAT = _TEXT("%hhd.%hhd.%hhd.%hhd");


const PCWSTR MACADDR = _TEXT("MACAddress");
const PCWSTR DESCR = _TEXT("Description");
const PCWSTR DHCPEN = _TEXT("DHCPEnabled");
const PCWSTR IPADDR = _TEXT("IPAddress");
const PCWSTR IPSUBNET = _TEXT("IPSubnet");
const PCWSTR IPCONMET = _TEXT("IPConnectionMetric");
const PCWSTR DNSDOM = _TEXT("DNSDomain");
const PCWSTR IPEN = _TEXT("IPEnabled");
const PCWSTR DEFIPGW = _TEXT("DefaultIPGateway");
const PCWSTR IPFLTSECEN = _TEXT("IPFilterSecurityEnabled");
const PCWSTR DNSDMSUFSRCHORD = _TEXT("DNSDomainSuffixSearchOrder");
const PCWSTR DNSHOSTNAME = _TEXT("DNSHostName");
const PCWSTR DNSWINSRES = _TEXT("DNSEnabledForWINSResolution");
const PCWSTR DNSSRCHORD = _TEXT("DNSServerSearchOrder");
const PCWSTR INDX = _TEXT("Index");
const PCWSTR SRVSNAME = _TEXT("ServiceName");

const PCWSTR RETVAL = _TEXT("ReturnValue");
const PCWSTR NETKVM = _TEXT("netkvm");


const PCWSTR ENIPFLTSEC_METHOD = _TEXT("EnableIPFilterSec");
const PCWSTR ENSTATIC_METHOD = _TEXT("EnableStatic");
const PCWSTR SUBNETMSK = _TEXT("SubnetMask");
const PCWSTR ENDNS_METHOD = _TEXT("EnableDNS");
const PCWSTR SETGW_METHOD = _TEXT("SetGateways");

const PCWSTR SETDNSSRCORD_METHOD = _TEXT("SetDNSServerSearchOrder");


void LogReport(
    __in HRESULT hr,
    __in_z __format_string PCWSTR fmt, ...
);


#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)
#define WFUNCTION WIDE1(__FUNCTION__)
