
#include "Pch.h"

#include "RHEVCredProv.h"
#include "RHEVProvider.h"
#include "RHEVCredentials.h"
#include "Helpers.h"

RHEVProvider::RHEVProvider() :
	_cRef(1),
	_pCredentialsChannel(NULL),
	_pRHEVCredentials(NULL),
	_pCredentialProviderEvents(NULL)
{
	DllAddRef();
}

RHEVProvider::~RHEVProvider()
{
	if (_pCredentialsChannel != NULL)
	{
		delete _pCredentialsChannel;
		_pCredentialsChannel = NULL;
	}

	if (_pRHEVCredentials != NULL)
	{
		_pRHEVCredentials->Release();
		_pRHEVCredentials = NULL;
	}

	if (_pCredentialProviderEvents != NULL)
	{
		_pCredentialProviderEvents->Release();
		_pCredentialProviderEvents = NULL;
	}

	DllRelease();
}

	// IUnknown

IFACEMETHODIMP_(ULONG) RHEVProvider::AddRef()
{
	return ++_cRef;
}

IFACEMETHODIMP_(ULONG) RHEVProvider::Release()
{
	LONG cRef = --_cRef;
	if (!cRef)
	{
		delete this;
	}
	return cRef;
}

IFACEMETHODIMP RHEVProvider::QueryInterface(__in REFIID riid, __deref_out void** ppv)
{
	HRESULT hr;
	if (ppv)
	{
		if ((IID_IUnknown == riid) || (IID_ICredentialProvider == riid))
		{
			*ppv = static_cast<IUnknown*>(this);
			reinterpret_cast<IUnknown*>(*ppv)->AddRef();
			hr = S_OK;
		}
		else
		{
			*ppv = NULL;
			hr = E_NOINTERFACE;
		}
	}
	else
	{
		hr = E_INVALIDARG;
	}
	return hr;
}

	// ICredentialProvider

HRESULT STDMETHODCALLTYPE RHEVProvider::SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
														 DWORD dwFlags)
{
	UNREFERENCED_PARAMETER(dwFlags);

	HRESULT hr = E_INVALIDARG;
	
	switch (cpus)
	{
		case CPUS_LOGON:
		case CPUS_UNLOCK_WORKSTATION:
		{
			if ((_pRHEVCredentials == NULL) && (_pCredentialsChannel == NULL))
			{
				_pRHEVCredentials = new RHEVCredentials();
				if (_pRHEVCredentials != NULL)
				{
					_pCredentialsChannel = new CredentialsChannel();
					if (_pCredentialsChannel == NULL)
					{
						_pRHEVCredentials->Release();
						_pRHEVCredentials = NULL;
					
						hr = E_OUTOFMEMORY;
					}
				}
				else
				{
					hr = E_OUTOFMEMORY;
				}
			}

			if (_pRHEVCredentials != NULL)
			{
				_pRHEVCredentials->SetUsageScenario(cpus);
			}

			if (_pCredentialsChannel != NULL)
			{
				hr = _pCredentialsChannel->CreateChannel(this);
			}			

			break;
		}

		case CPUS_CHANGE_PASSWORD:
		case CPUS_CREDUI:
			hr = E_NOTIMPL;
			break;

		default:
			hr = E_INVALIDARG;
			break;
	}

    return hr;
}

HRESULT STDMETHODCALLTYPE RHEVProvider::SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION *pcpcs)
{
	UNREFERENCED_PARAMETER(pcpcs);
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE RHEVProvider::Advise(ICredentialProviderEvents *pcpe,
											   UINT_PTR upAdviseContext)
{
	if (_pCredentialProviderEvents != NULL)
	{
		_pCredentialProviderEvents->Release();
	}

	_pCredentialProviderEvents = pcpe;
	_pCredentialProviderEvents->AddRef();
	_upAdviseContext = upAdviseContext;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE RHEVProvider::UnAdvise()
{
	if (_pCredentialProviderEvents != NULL)
	{
		_pCredentialProviderEvents->Release();
		_pCredentialProviderEvents = NULL;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE RHEVProvider::GetFieldDescriptorCount(DWORD *pdwCount)
{
	ASSERT(pdwCount != NULL);
	*pdwCount = 0;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE RHEVProvider::GetFieldDescriptorAt(DWORD dwIndex,
															 CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR **ppcpfd)
{
	UNREFERENCED_PARAMETER(dwIndex);
	UNREFERENCED_PARAMETER(ppcpfd);
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE RHEVProvider::GetCredentialCount(DWORD *pdwCount,
														   DWORD *pdwDefault,
														   BOOL *pbAutoLogonWithDefault)
{
	ASSERT(pdwCount != NULL);
	ASSERT(pdwDefault != NULL);
	ASSERT(pbAutoLogonWithDefault != NULL);

	*pdwCount = (_pRHEVCredentials->GotCredentials() ? 1 : 0);
	*pdwDefault = 0;
	*pbAutoLogonWithDefault = TRUE;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE RHEVProvider::GetCredentialAt(DWORD dwIndex,
														ICredentialProviderCredential **ppcpc)
{
	ASSERT(dwIndex < 1);
	ASSERT(ppcpc != NULL);
	ASSERT(_pRHEVCredentials != NULL);

	HRESULT hr;

	if ((dwIndex < 1) && _pRHEVCredentials && ppcpc)
	{
		hr = _pRHEVCredentials->QueryInterface(
			IID_ICredentialProviderCredential, reinterpret_cast<void**>(ppcpc));
	}
	else
	{
		hr = E_INVALIDARG;
	}

	return hr;
}

void RHEVProvider::OnCredentialsArrivial(LPCWSTR wzUserName, LPCWSTR wzPassword, LPCWSTR wzDomain)
{
	ASSERT(_pCredentialProviderEvents != NULL);
	ASSERT(_pRHEVCredentials != NULL);

	_pRHEVCredentials->SetCredentials(wzUserName, wzPassword, wzDomain);

	if (_pCredentialProviderEvents != NULL)
	{
		_pCredentialProviderEvents->CredentialsChanged(_upAdviseContext);
	}
}

HRESULT RHEVCredProv_CreateInstance(REFIID riid, void** ppv)
{
	HRESULT hr;

	RHEVProvider *pProvider = new RHEVProvider();
	if (pProvider != NULL)
	{
		hr = pProvider->QueryInterface(riid, ppv);
		pProvider->Release();
	}
	else
	{
		hr = E_OUTOFMEMORY;
	}

	return hr;
}
