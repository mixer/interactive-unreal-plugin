#pragma once
#include <functional>
#include <memory>

namespace mixer
{

class websocket;

typedef std::function<void(const websocket& socket, const std::string& connectMessage)> on_ws_connect;
typedef std::function<void(const websocket& socket, const std::string& message)> on_ws_message;
typedef std::function<void(const websocket& socket, const unsigned short code, const std::string& error)> on_ws_error;
typedef std::function<void(const websocket& socket, const unsigned short code, const std::string& reason)> on_ws_close;

class websocket
{
public:
	virtual ~websocket() = 0 {};
	virtual int add_header(const std::string& key, const std::string& value) = 0;
	virtual int open(const std::string& uri, const on_ws_connect onConnect, const on_ws_message onMessage, const on_ws_error onError, const on_ws_close onClose) = 0;
	virtual int send(const std::string& message) = 0;
	virtual int read(std::string& message) = 0;
	virtual void close() = 0;
};

class websocket_factory
{
public:
	static std::unique_ptr<websocket> make_websocket();
};

}

extern "C" {

	typedef void* websocket_handle;

	typedef void(*c_on_ws_connect)(const websocket_handle handle, const char* connectMessage, const unsigned int connectMessageSize);
	typedef void(*c_on_ws_message)(const websocket_handle handle, const char* message, const unsigned int messageSize);
	typedef void(*c_on_ws_error)(const websocket_handle handle, const unsigned short code, const char* message, const unsigned int messageSize);
	typedef void(*c_on_ws_close)(const websocket_handle handle, const unsigned short code, const char* reason, const unsigned int reasonSize);

	int create_websocket(websocket_handle* handlePtr);
	int add_header(websocket_handle, const char* key, const char* value);
	int open_websocket(websocket_handle, const char* uri, const c_on_ws_connect, const c_on_ws_message, const c_on_ws_error, const c_on_ws_close);
	int write_websocket(websocket_handle, const char* message);
	int read_websocket(websocket_handle, c_on_ws_message onMessage);
	int close_websocket(websocket_handle);

};