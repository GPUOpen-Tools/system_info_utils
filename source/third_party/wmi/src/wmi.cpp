/**
  *
  * WMI
  * @author Thomas Sparber (2016)
  *
 **/

#include <stdio.h>
#include <comdef.h>
#include <functional>
#include <wbemcli.h>
#include <windows.h>

#include <tl/expected.hpp>
#include <wil/com.h>
#include <wil/resource.h>
#include <wmi.hpp>

using std::function;
using std::string;
using std::wstring;
using std::wstringstream;

using namespace Wmi;

wstring escape(wstring str)
{
	size_t start_pos = 0;
	const wstring from = L"\"";
	const wstring to = L"\\\"";

    while((start_pos = str.find(from, start_pos)) != string::npos)
	{
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }

    return str;
}

tl::expected<wil::com_ptr_nothrow<IWbemLocator>, WmiException> createWbemLocator()
{
	wil::com_ptr_nothrow<IWbemLocator> pLocator;
	HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pLocator));
	if(FAILED(hr))
	{
		switch(hr)
		{
			case REGDB_E_CLASSNOTREG:	return tl::unexpected(WmiException("Error initializing IWbemLocator: REGDB_E_CLASSNOTREG", hr));
			case CLASS_E_NOAGGREGATION:	return tl::unexpected(WmiException("Error initializing IWbemLocator: CLASS_E_NOAGGREGATION", hr));
			case E_NOINTERFACE:			return tl::unexpected(WmiException("Error initializing IWbemLocator: E_NOINTERFACE", hr));
			case E_POINTER:				return tl::unexpected(WmiException("Error initializing IWbemLocator: E_POINTER", hr));
			default:					return tl::unexpected(WmiException("Error initializing IWbemLocator: Unknown Error", hr));
		}
	}

	return pLocator;
}

tl::expected<wil::com_ptr_nothrow<IWbemServices>, WmiException> connect(IWbemLocator *pLocator, const std::string& path)
{
	wil::com_ptr_nothrow<IWbemServices> pServices;
	HRESULT hr = pLocator->ConnectServer(_bstr_t("\\\\.\\root\\") + _bstr_t(path.c_str()), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pServices);
	if(FAILED(hr))
	{
		switch(hr)
		{
			case (HRESULT)WBEM_E_ACCESS_DENIED:		return tl::unexpected(WmiException("Error initializing IWbemServices: WBEM_E_ACCESS_DENIED", hr));
			case (HRESULT)WBEM_E_FAILED:			return tl::unexpected(WmiException("Error initializing IWbemServices: WBEM_E_FAILED", hr));
			case (HRESULT)WBEM_E_INVALID_NAMESPACE:	return tl::unexpected(WmiException("Error initializing IWbemServices: WBEM_E_INVALID_NAMESPACE", hr));
			case (HRESULT)WBEM_E_INVALID_PARAMETER:	return tl::unexpected(WmiException("Error initializing IWbemServices: WBEM_E_INVALID_PARAMETER", hr));
			case (HRESULT)WBEM_E_OUT_OF_MEMORY:		return tl::unexpected(WmiException("Error initializing IWbemServices: WBEM_E_OUT_OF_MEMORY", hr));
			case (HRESULT)WBEM_E_TRANSPORT_FAILURE:	return tl::unexpected(WmiException("Error initializing IWbemServices: WBEM_E_TRANSPORT_FAILURE", hr));
			case (HRESULT)WBEM_E_LOCAL_CREDENTIALS:	return tl::unexpected(WmiException("Error initializing IWbemServices: WBEM_E_LOCAL_CREDENTIALS", hr));
			default:								return tl::unexpected(WmiException("Error initializing IWbemServices: Unknown Error", hr));
		}
	}

	//Set authentication proxy
	hr = CoSetProxyBlanket(pServices.get(), 
		RPC_C_AUTHN_DEFAULT, 
		RPC_C_AUTHZ_NONE, 
		COLE_DEFAULT_PRINCIPAL, 
		RPC_C_AUTHN_LEVEL_DEFAULT, 
		RPC_C_IMP_LEVEL_IMPERSONATE, 
		nullptr, 
		EOAC_NONE 
	);

	if(FAILED(hr))
	{
		switch(hr)
		{
			case E_INVALIDARG:	return tl::unexpected(WmiException("Coult not set proxy blanket: E_INVALIDARG", hr));
			default:			return tl::unexpected(WmiException("Coult not set proxy blanket: Unknown Error", hr));
		}
	}

	return pServices;
}

tl::expected<wil::com_ptr_nothrow<IEnumWbemClassObject>, WmiException> execute(IWbemServices *pServices, const string &q)
{
	wil::com_ptr_nothrow<IEnumWbemClassObject> pClassObject;
	HRESULT hr = pServices->ExecQuery(_bstr_t("WQL"), _bstr_t(q.c_str()), WBEM_FLAG_RETURN_IMMEDIATELY|WBEM_FLAG_FORWARD_ONLY, nullptr, &pClassObject);
	if(FAILED(hr))
	{
		switch(hr)
		{
			case (HRESULT)WBEM_E_ACCESS_DENIED:		return tl::unexpected(WmiException("Error executing query: WBEM_E_ACCESS_DENIED", hr));
			case (HRESULT)WBEM_E_FAILED:			return tl::unexpected(WmiException("Error executing query: WBEM_E_FAILED", hr));
			case (HRESULT)WBEM_E_INVALID_CLASS:		return tl::unexpected(WmiException("Error executing query: WBEM_E_INVALID_CLASS", hr));
			case (HRESULT)WBEM_E_INVALID_PARAMETER:	return tl::unexpected(WmiException("Error executing query: WBEM_E_INVALID_PARAMETER", hr));
			case (HRESULT)WBEM_E_OUT_OF_MEMORY:		return tl::unexpected(WmiException("Error executing query: WBEM_E_OUT_OF_MEMORY", hr));
			case (HRESULT)WBEM_E_SHUTTING_DOWN:		return tl::unexpected(WmiException("Error executing query: WBEM_E_SHUTTING_DOWN", hr));
			case (HRESULT)WBEM_E_TRANSPORT_FAILURE:	return tl::unexpected(WmiException("Error executing query: WBEM_E_TRANSPORT_FAILURE", hr));
			default:								return tl::unexpected(WmiException("Error executing query: Unknown Error", hr));
		}
	}

	return pClassObject;
}

tl::expected<void, WmiException> foreachObject(IEnumWbemClassObject *pClassObject, function<tl::expected<bool, WmiException>(IWbemClassObject*)> fn)
{
	bool cont = true;
	HRESULT hr = WBEM_S_NO_ERROR;

	//The final Next will return WBEM_S_FALSE
	while(cont && hr == WBEM_S_NO_ERROR)
	{
		ULONG uReturned = 0;
		wil::com_ptr_nothrow<IWbemClassObject> apObj;
		hr = pClassObject->Next(WBEM_INFINITE, 1, &apObj, &uReturned);

		if(FAILED(hr))
		{
			switch(hr)
			{
				case WBEM_S_FALSE: break;
				case (HRESULT)WBEM_E_INVALID_PARAMETER:	return tl::unexpected(WmiException("Error getting next element: WBEM_E_INVALID_PARAMETER", hr));
				case (HRESULT)WBEM_E_OUT_OF_MEMORY:		return tl::unexpected(WmiException("Error getting next element: WBEM_E_OUT_OF_MEMORY", hr));
				case (HRESULT)WBEM_E_UNEXPECTED:		return tl::unexpected(WmiException("Error getting next element: WBEM_E_UNEXPECTED", hr));
				case (HRESULT)WBEM_E_TRANSPORT_FAILURE:	return tl::unexpected(WmiException("Error getting next element: WBEM_E_TRANSPORT_FAILURE", hr));
				case WBEM_S_TIMEDOUT:					return tl::unexpected(WmiException("Error getting next element: WBEM_S_TIMEDOUT", hr));
				default:								return tl::unexpected(WmiException("Error getting next element: Unknown Error", hr));
			}
		}

		if(uReturned == 1)
		{
			auto result = fn(apObj.get());
			if(!result) return tl::unexpected(result.error());
			cont = *result;
		}
	}

	return {};
}

tl::expected<wstring, WmiException> convertVariant(const VARIANT &value)
{
	bool handled = true;
	std::wstringstream ss;

	switch(value.vt)
	{
		case VT_EMPTY:
			break;
		case VT_NULL:
			ss<<"NULL";
			break;
		case VT_I2:
			ss<<value.iVal;
			break;
		case VT_I4:
			ss<<value.lVal;
			break;
		case VT_R4:
			ss<<value.fltVal;
			break;
		case VT_R8:
			ss<<value.dblVal;
			break;
		case VT_BSTR:
			ss<<value.bstrVal;
			break;
		case VT_BOOL:
			ss<<(value.boolVal ? "true" : "false");
			break;
		case VT_DECIMAL:
			ss<<value.pdecVal;
			break;
		case VT_I1:
			ss<<value.cVal;
			break;
		case VT_UI1:
			ss<<value.bVal;
			break;
		case VT_UI2:
			ss<<value.uiVal;
			break;
		case VT_UI4:
			ss<<value.ulVal;
			break;
		case VT_I8:
			ss<<value.llVal;
			break;
		case VT_UI8:
			ss<<value.ullVal;
			break;
		case VT_INT:
			ss<<value.intVal;
			break;
		case VT_UINT:
			ss<<value.uintVal;
			break;
		case VT_VOID:
			break;
		case VT_ARRAY|VT_BSTR:
		{
			long lLower, lUpper;
			SafeArrayGetLBound(value.parray, 1, &lLower);
			SafeArrayGetUBound(value.parray, 1, &lUpper);

			ss<<"[";
			for(long i = lLower; i <= lUpper; ++i)
			{
				BSTR inner;
				HRESULT hr = SafeArrayGetElement(value.parray, &i, &inner);

				if(!FAILED(hr))
				{
					ss<<"\""<<escape(inner)<<"\"";
				}

				if(i != lUpper)ss<<",";
			}
			ss<<"]";

			break;
		}
		case VT_ARRAY|VT_I4:
		{
			long lLower, lUpper;
			SafeArrayGetLBound(value.parray, 1, &lLower);
			SafeArrayGetUBound(value.parray, 1, &lUpper);

			ss<<"[";
			for(long i = lLower; i <= lUpper; ++i)
			{
				ULONG inner;
				HRESULT hr = SafeArrayGetElement(value.parray, &i, &inner);

				if(!FAILED(hr))
				{
					ss<<inner;
				}

				if(i != lUpper)ss<<",";
			}
			ss<<"]";

			break;
		}
		
		
		case VT_CY:				return tl::unexpected(WmiException("Data type not yet supported: VT_CY", value.vt));
		case VT_DATE:			return tl::unexpected(WmiException("Data type not yet supported: VT_DATE", value.vt));
		case VT_DISPATCH:		return tl::unexpected(WmiException("Data type not yet supported: VT_DISPATCH", value.vt));
		case VT_ERROR:			return tl::unexpected(WmiException("Data type not yet supported: VT_ERROR", value.vt));
		case VT_VARIANT:		return tl::unexpected(WmiException("Data type not yet supported: VT_VARIANT", value.vt));
		case VT_UNKNOWN:		return tl::unexpected(WmiException("Data type not yet supported: VT_UNKNOWN", value.vt));
		case VT_HRESULT:		return tl::unexpected(WmiException("Data type not yet supported: VT_HRESULT", value.vt));
		case VT_PTR:			return tl::unexpected(WmiException("Data type not yet supported: VT_PTR", value.vt));
		case VT_SAFEARRAY:		return tl::unexpected(WmiException("Data type not yet supported: VT_SAFEARRAY", value.vt));
		case VT_CARRAY:			return tl::unexpected(WmiException("Data type not yet supported: VT_CARRAY", value.vt));
		case VT_USERDEFINED:	return tl::unexpected(WmiException("Data type not yet supported: VT_USERDEFINED", value.vt));
		case VT_RECORD:			return tl::unexpected(WmiException("Data type not yet supported: VT_RECORD", value.vt));
		case VT_INT_PTR:		return tl::unexpected(WmiException("Data type not yet supported: VT_INT_PTR", value.vt));
		case VT_UINT_PTR:		return tl::unexpected(WmiException("Data type not yet supported: VT_UINT_PTR", value.vt));
		case VT_LPSTR:			return tl::unexpected(WmiException("Data type not yet supported: VT_LPSTR", value.vt));
		case VT_LPWSTR:			return tl::unexpected(WmiException("Data type not yet supported: VT_LPWSTR", value.vt));
		case VT_BYREF:
			//for local use only
			break;
		default:
			handled = false;
			break;
	}
	
	if(!handled)
	{
		return tl::unexpected(WmiException("Unknown data type", value.vt));
	}

	return ss.str();
}

tl::expected<void, WmiException> foreachProperty(IWbemClassObject *object, function<bool(const wstring&, const std::wstring&)> fn)
{
	SAFEARRAY *psaNames = nullptr;
    HRESULT hr = object->GetNames(nullptr, WBEM_FLAG_ALWAYS | WBEM_FLAG_NONSYSTEM_ONLY, nullptr, &psaNames);

    if(FAILED(hr))
    {
		switch(hr)
		{
			case (HRESULT)WBEM_E_FAILED:			return tl::unexpected(WmiException("Could not get properties: WBEM_E_FAILED", hr));
			case (HRESULT)WBEM_E_INVALID_PARAMETER:	return tl::unexpected(WmiException("Could not get properties: WBEM_E_INVALID_PARAMETER", hr));
			case (HRESULT)WBEM_E_OUT_OF_MEMORY:		return tl::unexpected(WmiException("Could not get properties: WBEM_E_OUT_OF_MEMORY", hr));
			default:								return tl::unexpected(WmiException("Could not get properties: WBEM_E_FAILED", hr));
		}
    }

	long lLower, lUpper; 
	BSTR propName = nullptr;
	SafeArrayGetLBound(psaNames, 1, &lLower);
	SafeArrayGetUBound(psaNames, 1, &lUpper);

	for(long i = lLower; i <= lUpper; ++i) 
	{
		hr = SafeArrayGetElement(psaNames, &i, &propName);

		if(FAILED(hr))
		{
			SafeArrayDestroy(psaNames);
			switch(hr)
			{
				case DISP_E_BADINDEX:	return tl::unexpected(WmiException("Could not get name from SafeArray: DISP_E_BADINDEX", hr));
				case E_INVALIDARG:		return tl::unexpected(WmiException("Could not get name from SafeArray: E_INVALIDARG", hr));
				case E_OUTOFMEMORY:		return tl::unexpected(WmiException("Could not get name from SafeArray: E_OUTOFMEMORY", hr));
				default:				return tl::unexpected(WmiException("Could not get name from SafeArray: Unknown Error", hr));
			}
		}

		VARIANT value;
		VariantInit(&value);
		hr = object->Get(propName, 0, &value, nullptr, nullptr);
		
		if(FAILED(hr))
		{
			SysFreeString(propName);
			SafeArrayDestroy(psaNames);
			switch(hr)
			{
				case (HRESULT)WBEM_E_FAILED:			return tl::unexpected(WmiException("Could not get property: WBEM_E_FAILED", hr));
				case (HRESULT)WBEM_E_INVALID_PARAMETER:	return tl::unexpected(WmiException("Could not get property: WBEM_E_INVALID_PARAMETER", hr));
				case (HRESULT)WBEM_E_NOT_FOUND:			return tl::unexpected(WmiException("Could not get property: WBEM_E_NOT_FOUND", hr));
				case (HRESULT)WBEM_E_OUT_OF_MEMORY:		return tl::unexpected(WmiException("Could not get property: WBEM_E_OUT_OF_MEMORY", hr));
				default:								return tl::unexpected(WmiException("Could not get property: Unknown Error", hr));
			}
		}
		
		auto converted = convertVariant(value);
		VariantClear(&value);

		if(!converted)
		{
			wstring temp(propName);
			SysFreeString(propName);
			SafeArrayDestroy(psaNames);
			return tl::unexpected(WmiException(
				string("Can't convert parameter: ") + string(temp.begin(), temp.end()) + ": " + converted.error().errorMessage,
				converted.error().errorCode));
		}

		bool cont = fn(propName, *converted);
		SysFreeString(propName);
		if(!cont)break;
	}

    SafeArrayDestroy(psaNames);
	return {};
}

tl::expected<void, WmiException> Wmi::query(const string& q, const string& p, WmiResult &out)
{
	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr))
	{
		return tl::unexpected(WmiException("The COM library is already initialized on this thread", hr));
	}
	wil::unique_couninitialize_call coinit_guard;

	auto locator = createWbemLocator();
	if (!locator) return tl::unexpected(locator.error());

	auto services = connect(locator->get(), p);
	if (!services) return tl::unexpected(services.error());

	auto classObject = execute(services->get(), q);
	if (!classObject) return tl::unexpected(classObject.error());

	std::size_t index = 0;
	auto result = foreachObject(classObject->get(), [&out,&index](IWbemClassObject *object)
		-> tl::expected<bool, WmiException>
	{
		auto prop_result = foreachProperty(object, [&out,index](const wstring &name, const std::wstring &value)
		{
			out.set(index, name, value);
			return true;
		});
		if (!prop_result) return tl::unexpected(prop_result.error());
		index++;
		return true;
	});

	if (!result) return tl::unexpected(result.error());
	return {};
}
