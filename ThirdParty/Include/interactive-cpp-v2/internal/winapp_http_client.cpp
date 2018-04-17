#include "winapp_http_client.h"
#include "common.h"
#include <mutex>
#include <condition_variable>
#include <sstream>

#if _DURANGO
#define CALLING_CONVENTION
#include <ixmlhttprequest2.h>
#else
#define CALLING_CONVENTION __stdcall
#include <MsXml6.h>
#endif

#include <wrl.h>
#include <shcore.h>
#include <ppltasks.h>

#define RETURN_HR_IF_FAILED(x) hr = x; if(0 != hr) { return hr; }

namespace mixer_internal
{

using namespace Platform;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Windows::Storage::Streams;
using namespace concurrency;

// Buffer with the required ISequentialStream interface to send data with and IXHR2 request. The only method
// required for use is Read.  IXHR2 will not write to this buffer nor will it use anything from the IDispatch
// interface.
class HttpRequestStream : public RuntimeClass<RuntimeClassFlags<ClassicCom>, ISequentialStream, IDispatch>
{
public:
	HttpRequestStream() : m_buffer(nullptr), m_seekLocation(0), m_bufferSize(0)
	{
	}

	~HttpRequestStream()
	{
		FreeInternalBuffer();
	}

	// ISequentialStream interface
	HRESULT CALLING_CONVENTION Read(void *buffer, unsigned long bufferSize, unsigned long *bytesRead)
	{
		if (buffer == nullptr)
		{
			return E_INVALIDARG;
		}

		long result = S_OK;

		if (bufferSize + m_seekLocation > m_bufferSize)
		{
			result = S_FALSE;

			// Calculate how many bytes are remaining 
			bufferSize = static_cast<unsigned long>(m_bufferSize - m_seekLocation);
		}

		memcpy(buffer, m_buffer + m_seekLocation, bufferSize);

		*bytesRead = bufferSize;
		m_seekLocation += bufferSize;

		return result;
	}
	HRESULT CALLING_CONVENTION Write(const void *, unsigned long, unsigned long *) { return E_NOTIMPL; }

	// IDispatch interface is required but not used in this context.  The methods are empty.
	HRESULT CALLING_CONVENTION GetTypeInfoCount(unsigned int FAR*) { return E_NOTIMPL; }
	HRESULT CALLING_CONVENTION GetTypeInfo(unsigned int, LCID, ITypeInfo FAR* FAR*) { return E_NOTIMPL; }
	HRESULT CALLING_CONVENTION GetIDsOfNames(REFIID, OLECHAR FAR* FAR*, unsigned int, LCID, DISPID FAR*) { return DISP_E_UNKNOWNNAME; }
	HRESULT CALLING_CONVENTION Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS FAR*, VARIANT FAR*, EXCEPINFO FAR*, unsigned int FAR*) { return S_OK; }

	// Methods created for simplicity when creating and passing along the buffer
	HRESULT CALLING_CONVENTION Open(const void *buffer, size_t bufferSize)
	{
		AllocateInternalBuffer(bufferSize);

		memcpy(m_buffer, buffer, bufferSize);

		return S_OK;
	}
	size_t CALLING_CONVENTION Size() const { return m_bufferSize; }
private:
	void CALLING_CONVENTION AllocateInternalBuffer(size_t size)
	{
		if (m_buffer != nullptr)
		{
			FreeInternalBuffer();
		}

		m_bufferSize = size;
		m_buffer = new unsigned char[size];
	}
	void CALLING_CONVENTION FreeInternalBuffer()
	{
		delete[] m_buffer;
		m_buffer = nullptr;
	}

	unsigned char    *m_buffer;
	size_t            m_seekLocation;
	size_t            m_bufferSize;
};

// Implementation of IXMLHTTPRequest2Callback used when only the complete response is needed.
// When processing chunks of response data as they are received, use HttpRequestBuffersCallback instead.
class HttpRequestStringCallback
	: public RuntimeClass<RuntimeClassFlags<ClassicCom>, IXMLHTTPRequest2Callback, FtmBase>
{
public:

	HttpRequestStringCallback(IXMLHTTPRequest2* httpRequest,
		cancellation_token ct = concurrency::cancellation_token::none()) :
		request(httpRequest), cancellationToken(ct)
	{
		// Register a callback function that aborts the HTTP operation when 
		// the cancellation token is canceled.
		if (cancellationToken != cancellation_token::none())
		{
			registrationToken = cancellationToken.register_callback([this]()
			{
				if (request != nullptr)
				{
					request->Abort();
				}
			});
		}
	}

	// Called when the HTTP request is being redirected to a new URL.
	IFACEMETHODIMP OnRedirect(IXMLHTTPRequest2*, PCWSTR)
	{
		return S_OK;
	}

	// Called when HTTP headers have been received and processed.
	IFACEMETHODIMP OnHeadersAvailable(IXMLHTTPRequest2*, DWORD statusCode, PCWSTR reasonPhrase)
	{
		HRESULT hr = S_OK;

		// We must not propagate exceptions back to IXHR2.
		try
		{
			this->m_statusCode = statusCode;
			this->m_reasonPhrase = reasonPhrase;
		}
		catch (std::bad_alloc&)
		{
			hr = E_OUTOFMEMORY;
		}

		return hr;
	}

	// Called when a portion of the entity body has been received.
	IFACEMETHODIMP OnDataAvailable(IXMLHTTPRequest2*, ISequentialStream*)
	{
		return S_OK;
	}

	// Called when the entire entity response has been received.
	IFACEMETHODIMP OnResponseReceived(IXMLHTTPRequest2*, ISequentialStream* responseStream)
	{
		std::wstring wstr;
		HRESULT hr = ReadUtf8StringFromSequentialStream(responseStream, wstr);

		// We must not propagate exceptions back to IXHR2.
		try
		{
			completionEvent.set(std::make_tuple<HRESULT, std::wstring>(std::move(hr), move(wstr)));
		}
		catch (std::bad_alloc&)
		{
			hr = E_OUTOFMEMORY;
		}

		return hr;
	}

	// Simulate the functionality of DataReader.ReadString().
	// This is needed because DataReader requires IRandomAccessStream and this
	// code has an ISequentialStream that does not have a conversion to IRandomAccessStream like IStream does.
	HRESULT ReadUtf8StringFromSequentialStream(ISequentialStream* readStream, std::wstring& str)
	{
		// Convert the response to Unicode wstring.
		HRESULT hr;

		// Holds the response as a Unicode string.
		std::wstringstream ss;

		while (true)
		{
			ULONG cb;
			char buffer[4096];

			// Read the response as a UTF-8 string.  Since UTF-8 characters are 1-4 bytes long,
			// we need to make sure we only read an integral number of characters.  So we'll
			// start with 4093 bytes.
			hr = readStream->Read(buffer, sizeof(buffer) - 3, &cb);
			if (FAILED(hr) || (cb == 0))
			{
				break; // Error or no more data to process, exit loop.
			}

			if (cb == sizeof(buffer) - 3)
			{
				ULONG subsequentBytesRead;
				unsigned int i, cl;

				// Find the first byte of the last UTF-8 character in the buffer.
				for (i = cb - 1; (i >= 0) && ((buffer[i] & 0xC0) == 0x80); i--);

				// Calculate the number of subsequent bytes in the UTF-8 character.
				if (((unsigned char)buffer[i]) < 0x80)
				{
					cl = 1;
				}
				else if (((unsigned char)buffer[i]) < 0xE0)
				{
					cl = 2;
				}
				else if (((unsigned char)buffer[i]) < 0xF0)
				{
					cl = 3;
				}
				else
				{
					cl = 4;
				}

				// Read any remaining bytes.
				if (cb < i + cl)
				{
					hr = readStream->Read(buffer + cb, i + cl - cb, &subsequentBytesRead);
					if (FAILED(hr))
					{
						break; // Error, exit loop.
					}
					cb += subsequentBytesRead;
				}
			}

			// First determine the size required to store the Unicode string.
			int const sizeRequired = MultiByteToWideChar(CP_UTF8, 0, buffer, cb, nullptr, 0);
			if (sizeRequired == 0)
			{
				// Invalid UTF-8.
				hr = HRESULT_FROM_WIN32(GetLastError());
				break;
			}
			std::unique_ptr<char16[]> wstr(new(std::nothrow) char16[sizeRequired + 1]);
			if (wstr.get() == nullptr)
			{
				hr = E_OUTOFMEMORY;
				break;
			}

			// Convert the string from UTF-8 to UTF-16LE.  This can never fail, since
			// the previous call above succeeded.
			MultiByteToWideChar(CP_UTF8, 0, buffer, cb, wstr.get(), sizeRequired);
			wstr[sizeRequired] = L'\0'; // Terminate the string.
			ss << wstr.get(); // Write the string to the stream.
		}

		str = SUCCEEDED(hr) ? ss.str() : std::wstring();
		return (SUCCEEDED(hr)) ? S_OK : hr; // Don't return S_FALSE.
	}

	// Called when an error occurs during the HTTP request.
	IFACEMETHODIMP OnError(IXMLHTTPRequest2*, HRESULT hrError)
	{
		HRESULT hr = S_OK;

		// We must not propagate exceptions back to IXHR2.
		try
		{
			completionEvent.set(std::make_tuple<HRESULT, std::wstring>(std::move(hrError), std::wstring()));
		}
		catch (std::bad_alloc&)
		{
			hr = E_OUTOFMEMORY;
		}

		return hr;
	}

	// Retrieves the completion event for the HTTP operation.
	task_completion_event<std::tuple<HRESULT, std::wstring>> const& GetCompletionEvent() const
	{
		return completionEvent;
	}

	int GetStatusCode() const
	{
		return m_statusCode;
	}

	std::wstring GetReasonPhrase() const
	{
		return m_reasonPhrase;
	}

private:
	~HttpRequestStringCallback()
	{
		// Unregister the callback.
		if (cancellationToken != cancellation_token::none())
		{
			cancellationToken.deregister_callback(registrationToken);
		}
	}

	// Signals that the download operation was canceled.
	cancellation_token cancellationToken;

	// Used to unregister the cancellation token callback.
	cancellation_token_registration registrationToken;

	// The IXMLHTTPRequest2 that processes the HTTP request.
	ComPtr<IXMLHTTPRequest2> request;

	// Task completion event that is set when the 
	// download operation completes.
	task_completion_event<std::tuple<HRESULT, std::wstring>> completionEvent;

	int m_statusCode;
	std::wstring m_reasonPhrase;
};

winapp_http_client::winapp_http_client()
{
};

winapp_http_client::~winapp_http_client()
{
}

int
winapp_http_client::make_request(const std::string& uri, const std::string& verb, const std::map<std::string, std::string>* headers, const std::string& body, http_response& response, unsigned long timeoutMs) const
{
	HRESULT hr = S_OK;
	ComPtr<IXMLHTTPRequest2> request;

#if _DURANGO
	DWORD context = CLSCTX_SERVER;
#else
	DWORD context = CLSCTX_INPROC;
#endif
	RETURN_HR_IF_FAILED(CoCreateInstance(__uuidof(FreeThreadedXMLHTTP60), nullptr, context, __uuidof(IXMLHTTPRequest2), reinterpret_cast<void**>(request.GetAddressOf())));
	ComPtr<HttpRequestStringCallback> callback = Make<HttpRequestStringCallback>(request.Get());

	if (nullptr != headers)
	{
		for (auto header : *headers)
		{	
			RETURN_HR_IF_FAILED(request->SetRequestHeader(utf8_to_wstring(header.first).c_str(), utf8_to_wstring(header.second).c_str()));
		}
	}

	ComPtr<IXMLHTTPRequest2Callback> xhrRequestCallback;
	RETURN_HR_IF_FAILED(callback.As<IXMLHTTPRequest2Callback>(&xhrRequestCallback));
	RETURN_HR_IF_FAILED(request->Open(utf8_to_wstring(verb).c_str(), utf8_to_wstring(uri).c_str(), xhrRequestCallback.Get(), nullptr, nullptr, nullptr, nullptr));

	RETURN_HR_IF_FAILED(request->SetProperty(XHR_PROP_TIMEOUT, timeoutMs));

	if (body.empty())
	{
		RETURN_HR_IF_FAILED(request->Send(nullptr, 0));
	}
	else
	{
		ComPtr<HttpRequestStream> requestStream = Make<HttpRequestStream>();
		RETURN_HR_IF_FAILED(requestStream->Open(body.c_str(), body.length()));
		RETURN_HR_IF_FAILED(request->Send(requestStream.Get(), body.length()));
	}

	auto sendTask = create_task(callback->GetCompletionEvent());
	auto receiveTask = sendTask.then([&](std::tuple<HRESULT, std::wstring> resultTuple)
	{
		// If the GET operation failed, throw an Exception.
		if (S_OK == std::get<0>(resultTuple))
		{
			response.statusCode = callback->GetStatusCode();
			response.body = wstring_to_utf8(std::get<1>(resultTuple));
		}
	});

	receiveTask.wait();
	
	return S_OK;
}

}