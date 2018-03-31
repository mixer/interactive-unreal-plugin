#include "simplewebsocket.h"

#include <Windows.h>
#include <winhttp.h>

#include <map>
#include <sstream>
#include <codecvt>
#include <queue>
#include <condition_variable>
#include <regex>

#pragma comment(lib, "winhttp.lib")


// String conversion functions
std::wstring utf8_to_wstring(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	return conv.from_bytes(str);
}

std::string wstring_to_utf8(const std::wstring& wstr)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	return conv.to_bytes(wstr);
}

struct hinternet_deleter
{
	void operator()(void* internet)
	{
		WinHttpCloseHandle(internet);
	}
};
typedef std::unique_ptr<void, hinternet_deleter> hinternet_ptr;

class ws_client : public mixer::websocket
{
public:
	ws_client() : m_closed(false), m_open(false), m_opening(false)
	{	
	}

	~ws_client()
	{
	}

	int add_header(const std::string& key, const std::string& value)
	{
		m_headers[key] = value;
		return 0;
	}

	int open(const std::string& uri, const mixer::on_ws_connect onConnect, const mixer::on_ws_message onMessage, const mixer::on_ws_error onError, const mixer::on_ws_close onClose)
	{
		{
			std::lock_guard<std::mutex> lock(m_openMutex);
			m_opening = true;

			// Crack the URI.
			// Parse the url with regex in accordance with RFC 3986.
			std::regex url_regex(R"(^(([^:/?#]+):)?(//([^:/?#]*):?([0-9]*)?)?([^?#]*)(\?([^#]*))?(#(.*))?)", std::regex::ECMAScript);
			std::smatch url_match_result;

			if (!std::regex_match(uri, url_match_result, url_regex))
			{
				return E_INVALIDARG;
			}

			std::string protocol = url_match_result[2];
			std::string host = url_match_result[4];
			std::string port = url_match_result[5];
			std::string path = url_match_result[6];

			if (port.empty())
			{
				if (0 == protocol.compare("wss"))
				{
					port = "443";
				}
				else if (0 == protocol.compare("ws"))
				{
					port = "80";
				}
				else
				{
					return E_INVALIDARG;
				}
			}

			// Open an http connection to the server.
			hinternet_ptr sessionHandle(WinHttpOpen(L"Simplewebsocket - Windows", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
			if (nullptr == sessionHandle)
			{
				return GetLastError();
			}

			hinternet_ptr connectionHandle(WinHttpConnect(sessionHandle.get(), utf8_to_wstring(host).c_str(), (INTERNET_PORT)atoi(port.c_str()), 0));
			if (nullptr == connectionHandle)
			{
				return GetLastError();
			}

			hinternet_ptr requestHandle(WinHttpOpenRequest(connectionHandle.get(), L"GET", utf8_to_wstring(path).c_str(), nullptr, nullptr, nullptr, (0 == protocol.compare("wss") ? WINHTTP_FLAG_SECURE : 0)));
			if (nullptr == requestHandle)
			{
				return GetLastError();
			}

			// Request protocol upgrade from http to websocket. 
#pragma prefast(suppress:6387, "WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET does not take any arguments.") 
			BOOL status = WinHttpSetOption(requestHandle.get(), WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0);
			if (!status)
			{
				return GetLastError();
			}

			// Add any request headers.
			std::stringstream headerStream;
			for (std::pair<std::string, std::string> header : m_headers)
			{
				headerStream << header.first << ": " << header.second << "\r\n";
			}

			std::string headers = headerStream.str();
			// Perform websocket handshake by sending a request and receiving server's response. 
			// Application may specify additional headers if needed. 
			status = WinHttpSendRequest(requestHandle.get(), utf8_to_wstring(headers).c_str(), (DWORD)headers.length(), nullptr, 0, 0, 0);
			if (!status)
			{
				return GetLastError();
			}

			status = WinHttpReceiveResponse(requestHandle.get(), 0);
			if (!status)
			{
				return GetLastError();
			}

			// Application should check what is the HTTP status code returned by the server and behave accordingly. 
			// WinHttpWebSocketCompleteUpgrade will fail if the HTTP status code is different than 101. 
			hinternet_ptr websocketHandle(WinHttpWebSocketCompleteUpgrade(requestHandle.get(), NULL));
			if (nullptr == websocketHandle.get())
			{
				return GetLastError();
			}

			m_websocketHandle.swap(websocketHandle);

			m_opening = false;
			m_open = true;
			if (nullptr != onConnect)
			{
				std::string msg = "Connected to " + uri;
				onConnect(*this, msg);
			}
		}

		BYTE buffer[1024];
		memset(buffer, 0, sizeof(buffer));
		DWORD bytesRead = 0;
		WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType;

		std::stringstream message;

		while (!m_closed)
		{
			int err = WinHttpWebSocketReceive(m_websocketHandle.get(), buffer, sizeof(buffer) - 1, &bytesRead, &bufferType);
			if (err)
			{
				if (ERROR_WINHTTP_OPERATION_CANCELLED == err && nullptr != onClose)
				{
					BYTE reasonBuffer[123]; // Guaranteed max size for reason.
					DWORD reasonLengthConsumed = 0;
					USHORT status = 0;
					WinHttpWebSocketQueryCloseStatus(m_websocketHandle.get(), &status, reasonBuffer, sizeof(reasonBuffer), &reasonLengthConsumed);
					onClose(*this, status, m_closeReason);
					return 0;
				}
				return err;
			}

			if (WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE == bufferType)
			{
				BYTE reasonBuffer[123]; // Guaranteed max size for reason.
				DWORD reasonLengthConsumed = 0;
				USHORT status = 0;
				WinHttpWebSocketQueryCloseStatus(m_websocketHandle.get(), &status, reasonBuffer, sizeof(reasonBuffer), &reasonLengthConsumed);
				if (nullptr != onClose)
				{
					onClose(*this, status, std::string(reinterpret_cast<char*>(reasonBuffer), reasonLengthConsumed));
				}
				return 0;
			}

			if (WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE == bufferType ||
				WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE == bufferType)
			{
				// Binary messages are not supported.
				memset(buffer, 0, bytesRead);
				bytesRead = 0;
				continue;
			}

			buffer[bytesRead] = 0;
			message << buffer;
			memset(buffer, 0, bytesRead);
			bytesRead = 0;
			
			if (WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE != bufferType)
			{
				if (nullptr != onMessage)
				{
					onMessage(*this, message.str());
				}
				message = std::stringstream();
			}
		}

		return 0;
	}

	int send(const std::string& message)
	{
		if (m_closed)
		{
			return E_ABORT;
		}

		if (!m_open)
		{
			while (!m_opening)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			std::lock_guard<std::mutex> openLock(m_openMutex);

			if (!m_open)
			{
				return E_ABORT;
			}
		}

		return WinHttpWebSocketSend(m_websocketHandle.get(), WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (void*)message.c_str(), (DWORD)message.length());
	}

	int read(std::string& message)
	{
		if (m_closed)
		{
			return E_ABORT;
		}

		BYTE buffer[1024];
		memset(buffer, 0, sizeof(buffer));
		DWORD bytesRead = 0;
		WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType = WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE;
		std::stringstream messageStream;

		while (WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE == bufferType)
		{
			int err = WinHttpWebSocketReceive(m_websocketHandle.get(), buffer, sizeof(buffer), &bytesRead, &bufferType);
			if (err)
			{	
				return err;
			}

			if (WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE == bufferType)
			{
				return E_ABORT;
			}

			if (WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE == bufferType ||
				WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE == bufferType)
			{
				// Binary messages are not supported.
				return E_INVALID_PROTOCOL_FORMAT;
			}

			messageStream << buffer;
			memset(buffer, 0, bytesRead);
		}

		message = messageStream.str();
		return 0;
	}

	void close()
	{
		if (!m_closed)
		{
			m_closed = true;
			m_closeReason = "Close requested";
			WinHttpWebSocketClose(m_websocketHandle.get(), WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, (void*)m_closeReason.c_str(), (DWORD)m_closeReason.length());
		}
	}

private:
	hinternet_ptr m_websocketHandle;
	std::map<std::string, std::string> m_headers;
	bool m_open;
	bool m_opening;
	std::mutex m_openMutex;
	std::condition_variable m_openCV;
	bool m_closed;
	std::string m_closeReason;
};


std::unique_ptr<mixer::websocket>
mixer::websocket_factory::make_websocket()
{
	return std::unique_ptr<mixer::websocket>(new ws_client());
}

extern "C" {

	int create_websocket(websocket_handle* handlePtr)
	{
		if (nullptr == handlePtr)
		{
			return 1;
		}

		std::unique_ptr<mixer::websocket> handle = mixer::websocket_factory::make_websocket();
		*handlePtr = handle.release();
		return 0;
	}

	int add_header(websocket_handle handle, const char* key, const char* value)
	{
		mixer::websocket* client = reinterpret_cast<mixer::websocket*>(handle);
		return client->add_header(key, value);
	}

	int open_websocket(websocket_handle handle, const char* uri, const c_on_ws_connect onConnect, const c_on_ws_message onMessage, const c_on_ws_error onError, const c_on_ws_close onClose)
	{
		mixer::websocket* client = reinterpret_cast<mixer::websocket*>(handle);


		auto connectHandler = [&](const mixer::websocket& socket, const std::string& connectMessage)
		{
			if (onConnect)
			{
				onConnect((void*)&socket, connectMessage.c_str(), connectMessage.length());
			}
		};

		auto messageHandler = [&](const mixer::websocket& socket, const std::string& message)
		{
			if (onMessage)
			{
				onMessage((void*)&socket, message.c_str(), message.length());
			}
		};

		auto errorHandler = [&](const mixer::websocket& socket, unsigned short code, const std::string& error)
		{
			if (onError)
			{
				onError((void*)&socket, code, error.c_str(), error.length());
			}
		};

		auto closeHandler = [&](const mixer::websocket& socket, unsigned short code, const std::string& reason)
		{
			if (onClose)
			{
				onClose((void*)&socket, code, reason.c_str(), reason.length());
			}
		};

		return client->open(uri, connectHandler, messageHandler, errorHandler, closeHandler);
	}

	int write_websocket(websocket_handle handle, const char* message)
	{
		mixer::websocket* client = reinterpret_cast<mixer::websocket*>(handle);
		return client->send(message);
	}

	int read_websocket(websocket_handle handle, c_on_ws_message onMessage)
	{
		if (nullptr == handle || nullptr == onMessage)
		{
			return 1;
		}

		mixer::websocket* client = reinterpret_cast<mixer::websocket*>(handle);
		std::string message;
		int error = client->read(message);
		if (error)
		{
			return error;
		}

		return 0;
	}

	int close_websocket(websocket_handle handle)
	{
		mixer::websocket* client = reinterpret_cast<mixer::websocket*>(handle);
		client->close();
		delete client;
		return 0;
	}
}