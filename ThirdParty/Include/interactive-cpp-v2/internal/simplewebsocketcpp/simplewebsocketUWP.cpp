#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "simplewebsocket.h"


#include <ppltasks.h>
#include <sstream>
#include <codecvt>
#include <queue>
#include <condition_variable>

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::Networking::Sockets;

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

class semaphore
{
private:
	std::mutex mutex_;
	std::condition_variable condition_;
	unsigned long count_ = 0; // Initialized as locked.

public:
	void notify()
	{
		std::unique_lock<decltype(mutex_)> lock(mutex_);
		++count_;
		condition_.notify_one();
	}

	void wait()
	{
		std::unique_lock<decltype(mutex_)> lock(mutex_);
		while (!count_) // Handle spurious wake-ups.
		{
			condition_.wait(lock);
		}
		--count_;
	}

	bool try_wait()
	{
		std::unique_lock<decltype(mutex_)> lock(mutex_);
		if (count_)
		{
			--count_;
			return true;
		}
		return false;
	}
};

class ws_client : public mixer::websocket
{
public:
	ws_client() : m_closed(false)
	{
		m_ws = ref new MessageWebSocket();
		m_ws->Control->MessageType = SocketMessageType::Utf8;
	}

	int add_header(const std::string& key, const std::string& value)
	{
		std::wstring keyWS = utf8_to_wstring(key);
		std::wstring valueWS = utf8_to_wstring(value);
		m_ws->SetRequestHeader(StringReference(keyWS.c_str()), StringReference(valueWS.c_str()));
		return 0;
	}

	int open(const std::string& uri, const mixer::on_ws_connect onConnect, const mixer::on_ws_message onMessage, const mixer::on_ws_error onError, const mixer::on_ws_close onClose)
	{
		int result = 0;
		std::wstring uriWS = utf8_to_wstring(uri);
		Uri^ uriRef = ref new Uri(StringReference(uriWS.c_str()));

		bool connected = false;
		unsigned short closeCode = 0;
		std::string closeReason;

		m_ws->Closed += ref new TypedEventHandler<IWebSocket^, WebSocketClosedEventArgs^>([&](IWebSocket^ sender, WebSocketClosedEventArgs^ args)
		{
			closeCode = args->Code;
			closeReason = wstring_to_utf8(args->Reason->Data());
			std::lock_guard<std::mutex> autoLock(m_messagesMutex);
			m_closed = true;
			m_messagesSemaphore.notify();
		});

		m_ws->MessageReceived += ref new TypedEventHandler<MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^>([&](MessageWebSocket^ sender, MessageWebSocketMessageReceivedEventArgs^ args)
		{
			if (args->MessageType == SocketMessageType::Utf8)
			{	
				DataReader^ reader = args->GetDataReader();
				reader->UnicodeEncoding = UnicodeEncoding::Utf8;
				String^ message = reader->ReadString(reader->UnconsumedBufferLength);

				std::lock_guard<std::mutex> lock(m_messagesMutex);
				m_messages.push(wstring_to_utf8(message->Data()));
				m_messagesSemaphore.notify();
			}
		});
		
		create_task(m_ws->ConnectAsync(uriRef)).then([&](task<void> prevTask)
		{
			std::lock_guard<std::mutex> autoLock(m_messagesMutex);
			try
			{
				prevTask.get();
				connected = true;
			}
			catch (Exception^ ex)
			{
				result = ex->HResult;
			}

			m_messagesSemaphore.notify();
		});
		
		m_messagesSemaphore.wait();
		if (0 != result)
		{
			if (onError)
			{
				std::string message = "Connection failed.";
				onError(*this, 4000, message);
			}

			return result;
		}

		if (m_closed)
		{
			if (closeCode)
			{
				if (onError)
				{
					onError(*this, closeCode, closeReason);
				}
			}
			return result;
		}

		if (onConnect)
		{
			std::string connectMessage = "Connected to: " + uri;
			onConnect(*this, connectMessage);
		}

		// Process messages until the socket is closed;
		for(;;)
		{
			// Wait for a message
			m_messagesSemaphore.wait();

			if (m_closed)
			{	
				if (onError)
				{
					onError(*this, closeCode, closeReason);
				}

				if (onClose)
				{
					onClose(*this, closeCode, closeReason);
				}

				break;
			}

			std::string message;
			{
				std::lock_guard<std::mutex> autoLock(m_messagesMutex);
				message = m_messages.front();
				m_messages.pop();
			}

			if (onMessage)
			{
				onMessage(*this, message);
			}
		}

		return result;
	}

	int send(const std::string& message)
	{
		if (m_closed)
		{
			return E_ABORT;
		}

		DataWriter writer(m_ws->OutputStream);
		writer.WriteString(StringReference(utf8_to_wstring(message).c_str()));
		try
		{
			create_task(writer.StoreAsync()).wait();
			writer.DetachStream();
		}
		catch (Exception^ ex)
		{
			return ex->HResult;
		}

		return 0;
	}

	int read(std::string& message)
	{
		if (m_closed)
		{
			return E_ABORT;
		}

		m_messagesSemaphore.wait();
		if (m_closed)
		{
			// Incorrectly consumed close notification, pass it on.
			m_messagesSemaphore.notify();
			return E_ABORT;
		}

		std::lock_guard<std::mutex> autoLock(m_messagesMutex);
		message = m_messages.front();
		m_messages.pop();

		return 0;
	}

	void close()
	{
		if (!m_closed)
		{	
			m_ws->Close(1000, nullptr);
		}
	}

private:
	MessageWebSocket ^ m_ws;
	bool m_closed;
	std::mutex m_messagesMutex;
	semaphore m_messagesSemaphore;
	std::queue<std::string> m_messages;
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
				onConnect((void*)&socket, connectMessage.c_str(), (unsigned int)connectMessage.length());
			}
		};

		auto messageHandler = [&](const mixer::websocket& socket, const std::string& message)
		{
			if (onMessage)
			{
				onMessage((void*)&socket, message.c_str(), (unsigned int)message.length());
			}
		};

		auto errorHandler = [&](const mixer::websocket& socket, unsigned short code, const std::string& error)
		{
			if (onError)
			{
				onError((void*)&socket, code, error.c_str(), (unsigned int)error.length());
			}
		};

		auto closeHandler = [&](const mixer::websocket& socket, unsigned short code, const std::string& reason)
		{
			if (onClose)
			{
				onClose((void*)&socket, code, reason.c_str(), (unsigned int)reason.length());
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