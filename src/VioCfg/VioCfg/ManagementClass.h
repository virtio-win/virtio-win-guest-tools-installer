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
#include "stdafx.h"

class ManagementClass
{
public:
    ManagementClass(std::wstring const& adr);
    ~ManagementClass();

    bool GetInstances();
    bool MoveNext();
    std::wstring GetStringProperty(std::wstring const& propName);
    bool GetBoolProperty(std::wstring const& propName);
    int GetIntProperty(std::wstring const& propName);
    long GetLongProperty(std::wstring const& propName);
    std::wstring GetSafeArrayProperty(std::wstring const& propName);

    //private:
    bool GetMethod(std::wstring const& methodName, CComPtr<IWbemClassObject>& pInInstClass);
    bool Put(CComPtr<IWbemClassObject> pInInstClass, std::wstring const& propName, CComVariant* var);
    bool ExecMethod(CComPtr<IWbemClassObject> pInInstClass, std::wstring const& methodName, int* ret, bool adapter = true);

protected:
    std::wstring m_wmiClass;
    CComPtr<IWbemLocator> m_pLoc;
    CComPtr<IEnumWbemClassObject> m_pEnumerator;
    CComPtr<IWbemServices> m_pSvc;
    CComPtr<IWbemClassObject> m_pclsObj;
    static bool m_bSecurity;
};

