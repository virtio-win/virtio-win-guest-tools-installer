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
#include "ManagementClass.h"

bool ManagementClass::m_bSecurity = false;

ManagementClass::ManagementClass(std::wstring const& adr)
{
    m_wmiClass = adr;
    m_pLoc = nullptr;
    m_pEnumerator = nullptr;
    m_pclsObj = nullptr;
}

ManagementClass::~ManagementClass()
{

}

bool ManagementClass::GetInstances()
{
    HRESULT hr;

    if (m_bSecurity == false)
    {
        hr = CoInitializeSecurity(
            NULL,
            -1,
            NULL,
            NULL,
            RPC_C_AUTHN_LEVEL_PKT_PRIVACY, //RPC_C_AUTHN_LEVEL_CONNECT, //RPC_C_AUTHN_LEVEL_NONE, //
            RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL,
            EOAC_NONE,
            NULL
        );

        if (FAILED(hr) && (hr != RPC_E_TOO_LATE))
        {
            LogReport(hr, L"CoInitializeSecurity failed\n");
            return false;
        }

        m_bSecurity = true;
    }

    hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&m_pLoc);

    if (FAILED(hr))
    {
        LogReport(hr, L"CoCreateInstance failed\n");
        return false;
    }

    hr = m_pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        NULL,
        WBEM_FLAG_CONNECT_USE_MAX_WAIT,
        NULL,
        NULL,
        &m_pSvc
    );

    if (FAILED(hr))
    {
        LogReport(hr, L"ConnectServer failed\n");
        return false;
    }

    hr = CoSetProxyBlanket(
        m_pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hr))
    {
        LogReport(hr, L"CoSetProxyBlanket failed\n");
        return false;
    }

    std::wstring m_wmiAdr;
    m_wmiAdr = bstr_t("SELECT * From ") + m_wmiClass.c_str();

    hr = m_pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t(m_wmiAdr.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &m_pEnumerator);

    if (m_pEnumerator == NULL)
    {
        LogReport(hr, L"ExecQuery failed\n");
        return false;
    }

    if (FAILED(hr))
    {
        LogReport(hr, L"%ws failed\n", WFUNCTION);
        return false;
    }

    LogReport(hr, L"CoInitializeSecurity succeedded\n");
    return true;
}

bool ManagementClass::MoveNext()
{
    if (m_pEnumerator)
    {
        CComPtr<IWbemClassObject> pclsObj = nullptr;
        ULONG uReturn = 0;
        HRESULT hr = m_pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn <= 0)
        {
            return false;
        }
        if (SUCCEEDED(hr))
        {
            m_pclsObj = pclsObj;
            return true;
        }
        LogReport(hr, L"Next failed\n");
        return false;
    }
    LogReport(S_OK, L"%ws m_pEnumerator = NULL\n", WFUNCTION);
    return false;
}

std::wstring ManagementClass::GetStringProperty(std::wstring const& propName)
{
    std::wstring data;
    if (m_pclsObj != NULL)
    {
        CComVariant var_val;
        HRESULT hr;

        hr = m_pclsObj->Get(propName.c_str(), 0, &var_val, 0, 0);
        if (FAILED(hr))
        {
            LogReport(hr, L"GetStringProperty failed\n");
        }
        else if (var_val.vt == VT_BSTR)
        {
            data = var_val.bstrVal;
        }
    }
    else
    {
        LogReport(S_OK, L"%ws m_pclsObj = NULL\n", WFUNCTION);
    }
    return data;
}

std::wstring ManagementClass::GetSafeArrayProperty(std::wstring const& propName)
{
    std::wstring data;

    if (m_pclsObj != NULL)
    {
        CComVariant var_val;
        HRESULT hr = m_pclsObj->Get(propName.c_str(), 0, &var_val, 0, 0);
        if (FAILED(hr))
        {
            LogReport(hr, L"GetSafeArrayProperty failed\n");
        }
        else if (var_val.vt == (VT_ARRAY | VT_BSTR))
        {
            BSTR* pVals;
            HRESULT hr = SafeArrayAccessData(var_val.parray, (void**)&pVals);
            if (SUCCEEDED(hr))
            {
                long loBnd, upBnd;
                SafeArrayGetLBound(var_val.parray, 1, &loBnd);
                SafeArrayGetUBound(var_val.parray, 1, &upBnd);

                long cnt = upBnd - loBnd;
                for (int i = 0; i <= cnt; ++i)
                {
                    BSTR lVal = (BSTR)pVals[i];
                    data += (lVal);
                    if (i != cnt) {
                        data += L",";
                    }
                }
                SafeArrayUnaccessData(var_val.parray);
            }
        }
    }
    else
    {
        LogReport(S_OK, L"%ws m_pclsObj = NULL\n", WFUNCTION);
    }
    return data;
}

bool ManagementClass::GetBoolProperty(std::wstring const& propName)
{
    if (m_pclsObj != NULL)
    {
        CComVariant var_val;
        HRESULT hr;

        hr = m_pclsObj->Get(propName.c_str(), 0, &var_val, 0, 0);
        if (SUCCEEDED(hr) && (var_val.vt == VT_BOOL))
        {
            return (var_val.boolVal == VARIANT_TRUE);
        }

    }
    LogReport(S_OK, L"%ws m_pclsObj = NULL\n", WFUNCTION);
    return false;
}

int ManagementClass::GetIntProperty(std::wstring const& propName)
{
    if (m_pclsObj != NULL)
    {
        CComVariant var_val;
        HRESULT hr;

        hr = m_pclsObj->Get(propName.c_str(), 0, &var_val, 0, 0);
        if (SUCCEEDED(hr) && (var_val.vt == VT_I4))
        {
            return var_val.iVal;;
        }
    }
    LogReport(S_OK, L"%ws m_pclsObj = NULL\n", WFUNCTION);
    return 0;
}

long ManagementClass::GetLongProperty(std::wstring const& propName)
{
    if (m_pclsObj != NULL)
    {
        CComVariant var_val;
        HRESULT hr;

        hr = m_pclsObj->Get(propName.c_str(), 0, &var_val, 0, 0);
        if (SUCCEEDED(hr) && (var_val.vt == VT_I8))
        {
            return var_val.lVal;
        }
    }
    LogReport(S_OK, L"%ws m_pclsObj = NULL\n", WFUNCTION);
    return 0;
}

bool ManagementClass::GetMethod(std::wstring const& methodName, CComPtr<IWbemClassObject>& ptr)
{
    CComPtr<IWbemClassObject> pInClass = nullptr;
    CComPtr<IWbemClassObject> pInInstClass = nullptr;
    CComPtr<IWbemClassObject> pClass = nullptr;
    HRESULT hr;

    if (m_pSvc != NULL)
    {
        hr = m_pSvc->GetObject(bstr_t(m_wmiClass.c_str()), 0, NULL, &pClass, NULL);
        if (FAILED(hr))
        {
            LogReport(hr, L"GetObject failed\n");
            return false;
        }

        hr = pClass->GetMethod(methodName.c_str(), 0, &pInClass, NULL);
        if (FAILED(hr))
        {
            LogReport(hr, L"GetMethod failed\n");
            return false;
        }

        hr = pInClass->SpawnInstance(0, &pInInstClass);
        if (FAILED(hr))
        {
            LogReport(hr, L"SpawnInstance failed\n");
            return false;
        }

        ptr = pInInstClass;
        return true;
    }
    return false;
}

bool ManagementClass::Put(CComPtr<IWbemClassObject> pInInstClass, std::wstring const& propName, CComVariant* var)
{
    if (pInInstClass != NULL)
    {
        HRESULT hr;
        hr = pInInstClass->Put(propName.c_str(), 0,
            var, 0);

        if (FAILED(hr))
        {
            LogReport(hr, L"Put failed\n");
            return false;
        }
        return true;
    }
    return false;
}


bool ManagementClass::ExecMethod(CComPtr<IWbemClassObject> pInInstClass, std::wstring const& methodName, int* ret, bool adapter)
{
    if (m_pclsObj != NULL)
    {
        HRESULT hr;
        CComPtr<IWbemClassObject> pOutParams = nullptr;

        if (adapter)
        {
            CComVariant var_path;
            hr = m_pclsObj->Get(L"__PATH", 0, &var_path, NULL, NULL);
            if (FAILED(hr))
            {
                LogReport(hr, L"Get failed\n");
                return false;
            }
            hr = m_pSvc->ExecMethod(var_path.bstrVal, bstr_t(methodName.c_str()), 0,
                NULL, pInInstClass, &pOutParams, NULL);
        }
        else
        {
            hr = m_pSvc->ExecMethod(bstr_t(m_wmiClass.c_str()), bstr_t(methodName.c_str()), 0,
                NULL, pInInstClass, &pOutParams, NULL);
        }

        if (FAILED(hr))
        {
            LogReport(hr, L"ExecMethod failed\n");
            return false;
        }

        CComVariant ret_val;;
        hr = pOutParams->Get(_bstr_t(RETVAL), 0,
            &ret_val, NULL, 0);
        *ret = ret_val.intVal;
        return true;
    }
    return false;
}

