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
#include "ConfigClass.h"

Config::Config(const PCWSTR filename)
{
    nac = nullptr;
    cf = nullptr;
    name = filename;
}

Config::~Config()
{
    Close();
}

bool Config::Init(bool read)
{
    HRESULT hr = S_OK;

    hr = ::CoInitializeEx(NULL,
        COINIT_MULTITHREADED);

    if (FAILED(hr))
    {
        LogReport(hr, L"Cannot initialize COM");
        return false;
    }

    nac = new ManagementClass(CLASS_NAME);
    cf = new ConfigFile(name.c_str());

    return cf->Open(read);
}

void Config::Close()
{
    if (cf)
    {
        delete cf;
        cf = nullptr;
    }

    if (nac)
    {
        delete nac;
        nac = nullptr;
    }

    ::CoUninitialize();
}

bool ConfigWrite::Run()
{
    if (nac->GetInstances())
    {
        while (nac->MoveNext())
        {
            std::wstring srvname = nac->GetStringProperty(SRVSNAME);
            if (nac->GetBoolProperty(IPEN) &&
                !nac->GetBoolProperty(DHCPEN) &&
                (srvname.compare(NETKVM)) == 0)
            {
                LogReport(S_OK, L"srvmane = %ws", srvname.c_str());

                cf->WriteLine(STRING_FORMAT, MACADDR, VT_BSTR, nac->GetStringProperty(MACADDR).c_str());
                cf->WriteLine(STRING_FORMAT, DESCR, VT_BSTR, nac->GetStringProperty(DESCR).c_str());
//                cf->WriteLine(INTEGER_FORMAT, DHCPEN, VT_BOOL, nac->GetBoolProperty(DHCPEN));
                cf->WriteLine(STRING_FORMAT, IPADDR, VT_ARRAY | VT_BSTR, nac->GetSafeArrayProperty(IPADDR).c_str());
                cf->WriteLine(STRING_FORMAT, IPSUBNET, VT_ARRAY | VT_BSTR, nac->GetSafeArrayProperty(IPSUBNET).c_str());
                cf->WriteLine(INTEGER_FORMAT, IPCONMET, VT_I4, nac->GetIntProperty(IPCONMET));
                cf->WriteLine(STRING_FORMAT, DNSDOM, VT_BSTR, nac->GetStringProperty(DNSDOM).c_str());
//                cf->WriteLine(INTEGER_FORMAT, IPEN, VT_BOOL, nac->GetBoolProperty(IPEN));
                cf->WriteLine(STRING_FORMAT, DEFIPGW, VT_ARRAY | VT_BSTR, nac->GetSafeArrayProperty(DEFIPGW).c_str());
                cf->WriteLine(INTEGER_FORMAT, IPFLTSECEN, VT_BOOL, nac->GetBoolProperty(IPFLTSECEN));
                cf->WriteLine(STRING_FORMAT, DNSDMSUFSRCHORD, VT_ARRAY | VT_BSTR, nac->GetSafeArrayProperty(DNSDMSUFSRCHORD).c_str());
                cf->WriteLine(STRING_FORMAT, DNSHOSTNAME, VT_BSTR, nac->GetStringProperty(DNSHOSTNAME).c_str());
                cf->WriteLine(INTEGER_FORMAT, DNSWINSRES, VT_BOOL, nac->GetBoolProperty(DNSWINSRES));
                cf->WriteLine(STRING_FORMAT, DNSSRCHORD, VT_ARRAY | VT_BSTR, nac->GetSafeArrayProperty(DNSSRCHORD).c_str());
                cf->WriteLine(INTEGER_FORMAT, INDX, VT_I4, nac->GetIntProperty(INDX));
                cf->WriteLine(STRING_FORMAT, SRVSNAME, VT_BSTR, srvname.c_str());
            }
        }
    }

    return true;
}

ConfigRead::ConfigRead(const PCWSTR filename) : Config(filename)
{
    dhcp_en = false;
    ip_en = false;
    ip_filt_sec_en = false;
    dns_for_wins_res_en = false;

    ip_conn_metr = 0;
    index = 0;
}

bool ConfigRead::IsValidIPv4Address(std::wstring const& address)
{
    unsigned char a, b, c, d;
    return (swscanf_s(address.c_str(), IPV4_FORMAT, &a, &b, &c, &d) == 4);
}

std::vector<std::wstring> ConfigRead::SplitStringAsVector(std::wstring& line, const PCWSTR delim, SplitType type)
{
    std::vector<std::wstring> rdata;
    size_t end = line.find(delim);
    while (end != -1)
    {
        if ((type == SplitType::None) ||
            ((type == SplitType::Ipv4) && IsValidIPv4Address(line.substr(0, end).c_str())) ||
            ((type == SplitType::Ipv6) && !IsValidIPv4Address(line.substr(0, end).c_str())))
        {
            rdata.push_back(line.substr(0, end).c_str());
        }
        line.erase(line.begin(), line.begin() + end + 1);
        end = line.find(delim);
    }

    if ((type == SplitType::None) ||
        ((type == SplitType::Ipv4) && IsValidIPv4Address(line.substr(0, end).c_str())) ||
        ((type == SplitType::Ipv6) && !IsValidIPv4Address(line.substr(0, end).c_str())))
    {
        rdata.push_back(line.substr(0, end).c_str());
    }

    return rdata;
}

bool ConfigRead::ProcessInteger(std::vector<std::wstring>& vector)
{
    std::wstring method(vector[0].c_str());

    if (method == IPCONMET)
    {
        ip_conn_metr = _wtoi(vector[2].c_str());
    }
    else if (method == INDX)
    {
        index = _wtoi(vector[2].c_str());
    }
    else
    {
        return false;
    }

    return true;
}

bool ConfigRead::ProcessString(std::vector<std::wstring>& vector)
{
    std::wstring method(vector[0].c_str());

    if (method == MACADDR)
    {
        mac_addr = vector[2].c_str();
    }
    else if (method == DESCR)
    {
        descr = vector[2].c_str();
    }
    else if (method == DNSDOM)
    {
        dns_dom = vector[2].c_str();
    }
    else if (method == SRVSNAME)
    {
        srvc_name = vector[2].c_str();
    }
    else if (method == DNSHOSTNAME)
    {
        dns_host_name = vector[2].c_str();
    }
    else
    {
        return false;
    }

    return true;
}

bool ConfigRead::ProcessBoolean(std::vector<std::wstring>& vector)
{
    std::wstring method(vector[0].c_str());

    if (method == DHCPEN)
    {
        dhcp_en = !!(_wtoi(vector[2].c_str()));
    }
    else if (method == IPEN)
    {
        ip_en = !!(_wtoi(vector[2].c_str()));
    }
    else if (method == IPFLTSECEN)
    {
        ip_filt_sec_en = !!(_wtoi(vector[2].c_str()));
    }
    else if (method == DNSWINSRES)
    {
        dns_for_wins_res_en = !!(_wtoi(vector[2].c_str()));
    }
    else
    {
        return false;
    }

    return true;
}

bool ConfigRead::ProcessArrayOfStrings(std::vector<std::wstring>& vector)
{
    std::wstring method(vector[0].c_str());

    if (method == IPADDR)
    {
        ipv4_addr = SplitStringAsVector(vector[2], STRING_DELIM, SplitType::Ipv4);
        ipv6_addr = SplitStringAsVector(vector[2], STRING_DELIM, SplitType::Ipv6);
    }
    else if (method == IPSUBNET)
    {
        ipv4_subnet = SplitStringAsVector(vector[2], STRING_DELIM, SplitType::Ipv4);
        ipv6_subnet = SplitStringAsVector(vector[2], STRING_DELIM, SplitType::Ipv6);
    }
    else if (method == DEFIPGW)
    {
        ipv4_def_gw = SplitStringAsVector(vector[2], STRING_DELIM, SplitType::Ipv4);
        ipv6_def_gw = SplitStringAsVector(vector[2], STRING_DELIM, SplitType::Ipv6);
    }
    else if (method == DNSDMSUFSRCHORD)
    {
        dns_dom_suff_srch_ord = SplitStringAsVector(vector[2], STRING_DELIM);
    }
    else if (method == DNSSRCHORD)
    {
        dns_serv_srch_ord = SplitStringAsVector(vector[2], STRING_DELIM);
    }
    else
    {
        return false;
    }

    return false;
}

bool ConfigRead::ProcessParametesAsVector(std::vector<std::wstring>& vector)
{
    assert(vector.size() == 3);
    bool res = false;
    int type = _wtoi(vector[1].c_str());

    switch (type)
    {
    case VT_I4:
        res = ProcessInteger(vector);
        break;
    case VT_BSTR:
        res = ProcessString(vector);
        break;
    case VT_BOOL:
        res = ProcessBoolean(vector);
        break;
    case (VT_ARRAY | VT_BSTR):
        res = ProcessArrayOfStrings(vector);
        break;
    default:
        break;
    }

    return res;
}

bool ConfigRead::BuildSafeArrayFromVector(SAFEARRAY** sa, std::vector<std::wstring> const& arr)
{
    HRESULT hr;
    UINT count = (UINT)arr.size();
    if (count > 0)
    {
        SAFEARRAY* param1_list = SafeArrayCreateVector(VT_BSTR, 0, count);
        long idx[] = { 0 };

        for (UINT i = 0; i < count; i++)
        {
            idx[0] = (LONG)i;
            hr = SafeArrayPutElement(param1_list, idx, (BSTR)bstr_t(arr[i].c_str()));
            if (FAILED(hr))
            {
                return (false);
            }
        }

        *sa = param1_list;
        return true;
    }
    return false;
}

bool ConfigRead::EnableStatic(bool ipv4)
{
    CComPtr<IWbemClassObject> pInInstClass = nullptr;

    if (nac->GetMethod(ENSTATIC_METHOD, pInInstClass))
    {
        SAFEARRAY* sa1 = nullptr;
        SAFEARRAY* sa2 = nullptr;
        if (BuildSafeArrayFromVector(&sa1, ipv4 ? ipv4_addr : ipv6_addr) &&
            BuildSafeArrayFromVector(&sa2, ipv4 ? ipv4_subnet : ipv4_subnet))
        {
            CComVariant var1;
            var1.vt = VT_ARRAY | VT_BSTR;
            var1.parray = sa1;
            CComVariant var2;
            var2.vt = VT_ARRAY | VT_BSTR;
            var2.parray = sa2;
            if (nac->Put(pInInstClass, IPADDR, &var1) &&
                nac->Put(pInInstClass, SUBNETMSK, &var2))
            {
                int ret = 0;
                if (nac->ExecMethod(pInInstClass, ENSTATIC_METHOD, &ret, true))
                {
                    if (ret > 0)
                    {
                        LogReport(S_OK, L"%ws method completed with the error code %d", ENSTATIC_METHOD, ret);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

bool ConfigRead::SetDNSServer(void)
{
    CComPtr<IWbemClassObject> pInInstClass = nullptr;

    if (nac->GetMethod(SETDNSSRCORD_METHOD, pInInstClass))
    {
        SAFEARRAY* sa1 = nullptr;
        if (BuildSafeArrayFromVector(&sa1, dns_serv_srch_ord))
        {
            CComVariant var1;
            var1.vt = VT_ARRAY | VT_BSTR;
            var1.parray = sa1;
            if (nac->Put(pInInstClass, DNSSRCHORD, &var1))
            {
                int ret = 0;
                if (nac->ExecMethod(pInInstClass, SETDNSSRCORD_METHOD, &ret, true))
                {
                    if (ret > 0)
                    {
                        LogReport(S_OK, L"%ws method completed with the error code %d", SETDNSSRCORD_METHOD, ret);
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

bool ConfigRead::EnableDNS(void)
{
    CComPtr<IWbemClassObject> pInInstClass = nullptr;

    if (nac->GetMethod(ENDNS_METHOD, pInInstClass))
    {
        SAFEARRAY* sa1 = nullptr;
        SAFEARRAY* sa2 = nullptr;
        if (BuildSafeArrayFromVector(&sa1, dns_serv_srch_ord) &&
            BuildSafeArrayFromVector(&sa2, dns_dom_suff_srch_ord))
        {
            CComVariant var1;
            var1.vt = VT_BSTR;
            var1.bstrVal = (BSTR)bstr_t(dns_host_name.c_str());

            CComVariant var2;
            var2.vt = VT_BSTR;
            var2.bstrVal = (BSTR)bstr_t(dns_dom.c_str());

            CComVariant var3;
            var3.vt = VT_ARRAY | VT_BSTR;
            var3.parray = sa1;
            CComVariant var4;
            var4.vt = VT_ARRAY | VT_BSTR;
            var4.parray = sa2;
            if (nac->Put(pInInstClass, DNSHOSTNAME, &var1) &&
                nac->Put(pInInstClass, DNSDOM, &var2) &&
                nac->Put(pInInstClass, DNSSRCHORD, &var3) &&
                nac->Put(pInInstClass, DNSDMSUFSRCHORD, &var4))
            {
                int ret = 0;
                if (nac->ExecMethod(pInInstClass, ENDNS_METHOD, &ret, false))
                {
                    if (ret > 0)
                    {
                        LogReport(S_OK, L"%ws method completed with the error code %d", ENDNS_METHOD, ret);
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

bool ConfigRead::SetGateWays(bool ipv4)
{
    CComPtr<IWbemClassObject> pInInstClass = nullptr;

    if (nac->GetMethod(SETGW_METHOD, pInInstClass))
    {
        SAFEARRAY* sa1 = nullptr;
        if (BuildSafeArrayFromVector(&sa1, ipv4 ? ipv4_def_gw : ipv6_def_gw))
        {
            CComVariant var1;
            var1.vt = VT_ARRAY | VT_BSTR;
            var1.parray = sa1;
            if (nac->Put(pInInstClass, DEFIPGW, &var1))
            {
                int ret = 0;
                if (nac->ExecMethod(pInInstClass, SETGW_METHOD, &ret, true))
                {
                    if (ret > 0)
                    {
                        LogReport(S_OK, L"%ws method completed with the error code %d", SETGW_METHOD, ret);
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

bool ConfigRead::RestoreConfig()
{
    if (nac->GetInstances())
    {
        while (nac->MoveNext())
        {
            std::wstring mac = nac->GetStringProperty(MACADDR);
            LogReport(S_OK, L"macs %ws <--> %ws.", mac.c_str(), mac_addr.c_str());
            if (nac->GetStringProperty(MACADDR) == mac_addr &&
                nac->GetStringProperty(SRVSNAME) == srvc_name)
            {
                if (!EnableStatic(true))
                {
                    LogReport(S_OK, L"EnableStatic IPv4 failed");
                }
                if (!EnableStatic(false))
                {
                    LogReport(S_OK, L"EnableStatic IPv6 failed");
                }
                if (!SetDNSServer())
                {
                    LogReport(S_OK, L"SetDNSServer failed");
                }
                if (!SetGateWays(true))
                {
                    LogReport(S_OK, L"SetGateWays IPv4 failed");
                }
                if (!SetGateWays(false))
                {
                    LogReport(S_OK, L"SetGateWays IPv6 failed");
                }
                if (!EnableDNS())
                {
                    LogReport(S_OK, L"EnableDNS failed");
                }
                return true;
            }
        }
    }

    return false;
}

bool ConfigRead::Run()
{
    std::wstring line;

    while (cf->ReadLine(line))
    {
        std::vector<std::wstring> params;
        params = SplitStringAsVector(line, PARAMS_DELIM);
        ProcessParametesAsVector(params);
    }
    RestoreConfig();

    return true;
}

