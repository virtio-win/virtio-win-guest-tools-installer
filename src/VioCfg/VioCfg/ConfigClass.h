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
#pragma once

#include "ConfigFile.h"
#include "ManagementClass.h"


class Config
{
public:
    Config(const PCWSTR filename);
    ~Config();
    virtual bool Run() = 0;
    bool Init(bool read = false);
    void Close();
protected:
    ManagementClass* nac;
    ConfigFile* cf;
    std::wstring name;
};

class ConfigWrite : public Config
{
public:
    ConfigWrite(const PCWSTR filename) : Config(filename) { ; }
    bool Run();
};

class ConfigRead : public Config
{
public:
    ConfigRead(const PCWSTR filename);
    bool Run();
protected:
    enum SplitType
    {
        None,
        Ipv4,
        Ipv6
    };
    std::vector<std::wstring> SplitStringAsVector(std::wstring& line, const PCWSTR delim, SplitType type = SplitType::None);
    bool ProcessInteger(std::vector<std::wstring>& vector);
    bool ProcessString(std::vector<std::wstring>& vector);
    bool ProcessBoolean(std::vector<std::wstring>& vector);
    bool ProcessArrayOfStrings(std::vector<std::wstring>& vector);
    bool ProcessParametesAsVector(std::vector<std::wstring>& vector);
    bool RestoreConfig();
private:
    bool EnableStatic(bool ipv4 = true);
    bool SetDNSServer(void);
    bool SetGateWays(bool ipv4 = true);
    bool EnableDNS(void);
    bool BuildSafeArrayFromVector(SAFEARRAY** sa, std::vector<std::wstring> const& arr);
    bool IsValidIPv4Address(std::wstring const& address);
protected:
    std::vector<std::wstring> ipv4_addr;
    std::vector<std::wstring> ipv4_subnet;
    std::vector<std::wstring> ipv4_def_gw;
    std::vector<std::wstring> ipv6_addr;
    std::vector<std::wstring> ipv6_subnet;
    std::vector<std::wstring> ipv6_def_gw;
    std::vector<std::wstring> dns_dom_suff_srch_ord;
    std::vector<std::wstring> dns_serv_srch_ord;

    std::wstring mac_addr;
    std::wstring descr;
    std::wstring dns_dom;
    std::wstring srvc_name;
    std::wstring dns_host_name;

    bool dhcp_en;
    bool ip_en;
    bool ip_filt_sec_en;
    bool dns_for_wins_res_en;

    unsigned int ip_conn_metr;
    unsigned int index;
};
