#include "interactive_session.h"
#include "common.h"

namespace mixer
{

interactive_session_internal::interactive_session_internal(bool manualStart)
	: callerContext(nullptr), manualStart(manualStart), state(interactive_state::disconnected), shutdownRequested(false), packetId(0), sequenceId(0), wsOpen(false),
	onInput(nullptr), onError(nullptr), onStateChanged(nullptr), onParticipantsChanged(nullptr), onUnhandledMethod(nullptr)
{
	scenesRoot.SetObject();
}

void interactive_session_internal::handle_ws_open(const websocket& socket, const std::string& message)
{
	(socket);
	DEBUG_INFO("Websocket opened: " + message);
	if (!this->wsOpen)
	{
		// First connection, unblock the connect thread.
		std::lock_guard<std::mutex> l(this->wsOpenMutex);
		this->wsOpen = true;
		this->wsOpenCV.notify_one();
	}

	
}

void interactive_session_internal::handle_ws_message(const websocket& socket, const std::string& message)
{
	(socket);
	DEBUG_TRACE("Websocket message received: " + message);
	if (this->shutdownRequested)
	{
		return;
	}

	// Parse the message to determine packet type.
	try
	{
		std::shared_ptr<rapidjson::Document> doc = std::make_shared<rapidjson::Document>();
		doc->Parse(message.c_str(), message.length());
		if (!doc->HasMember(RPC_TYPE))
		{
			// Message does not conform to protocol, ignore it.
			return;
		}

		std::string type = (*doc)[RPC_TYPE].GetString();
		if (0 == type.compare(RPC_METHOD))
		{
			std::lock_guard<std::mutex> l(this->methodsMutex);
			this->methods.emplace(doc);
		}
		else if (0 == type.compare(RPC_REPLY))
		{
			unsigned int id = (*doc)[RPC_ID].GetUint();
			std::lock_guard<std::mutex> l(this->repliesMutex);
			this->replies.emplace(id, doc);
			this->repliesCV.notify_all();
		}
	}
	catch (...)
	{
		// Assume any failed parsed messages do not conform to protocol.
	}
}

void interactive_session_internal::handle_ws_error(const websocket& socket, const unsigned short code, const std::string& message)
{
	(socket);
	DEBUG_ERROR("Websocket error: " + message + ". (" + std::to_string(code) + ")");
	if (this->shutdownRequested)
	{
		return;
	}

	std::lock_guard<std::mutex> l(this->errorsMutex);
	this->errors.emplace(std::pair<unsigned short, std::string>(code, message));
}

void interactive_session_internal::handle_ws_close(const websocket& socket, const unsigned short code, const std::string& message)
{
	(socket);
	DEBUG_INFO("Websocket closed: " + message + ". (" + std::to_string(code) + ")");
	if (this->shutdownRequested)
	{
		return;
	}

	std::lock_guard<std::mutex> l(this->errorsMutex);
	this->errors.emplace(std::pair<unsigned short, std::string>(code, message));
}

void interactive_session_internal::run_ws()
{
	// Attempt to connect to all hosts in the order they were returned by the service.
	for (auto hostItr = this->hosts.begin(); hostItr < this->hosts.end(); ++hostItr)
	{
		auto onWsOpen = std::bind(&interactive_session_internal::handle_ws_open, this, std::placeholders::_1, std::placeholders::_2);
		auto onWsMessage = std::bind(&interactive_session_internal::handle_ws_message, this, std::placeholders::_1, std::placeholders::_2);
		auto onWsError = std::bind(&interactive_session_internal::handle_ws_error, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		auto onWsClose = std::bind(&interactive_session_internal::handle_ws_close, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		DEBUG_INFO("Connecting to websocket: " + *hostItr);
		int errc = this->ws->open(*hostItr, onWsOpen, onWsMessage, onWsError, onWsClose);

		if (this->shutdownRequested)
		{
			break;
		}

		if (errc)
		{
			if (!this->wsOpen)
			{
				DEBUG_WARNING("Failed to open websocket: " + *hostItr);
			}
			else
			{
				DEBUG_WARNING("Lost connection to websocket: " + *hostItr);
			}
		}
	}

	if (!this->wsOpen)
	{
		// No connections were made, unblock the connect thread.
		this->wsOpenCV.notify_one();
	}
}

}