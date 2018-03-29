//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "interactive_session.h"
#include "common.h"

#include <functional>
#include <iostream>

namespace mixer
{

typedef std::function<void(rapidjson::Document::AllocatorType& allocator, rapidjson::Value& value)> on_get_params;

int send_method(interactive_session_internal& session, const std::string& method, on_get_params getParams, bool discard, unsigned int* id)
{
	if (session.shutdownRequested)
	{
		return MIXER_OK;
	}

	try
	{
		rapidjson::Document doc;
		doc.SetObject();
		rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
		unsigned int packetID = session.packetId++;
		doc.AddMember(RPC_ID, packetID, allocator);
		doc.AddMember(RPC_METHOD, rapidjson::StringRef(method.c_str(), method.length()), allocator);
		doc.AddMember(RPC_DISCARD, discard, allocator);
		doc.AddMember(RPC_SEQUENCE, session.sequenceId, allocator);

		// Get the parameters from the caller.
		rapidjson::Value params(rapidjson::kObjectType);
		if (getParams)
		{
			getParams(allocator, params);
		}
		doc.AddMember(RPC_PARAMS, params, allocator);

		std::string packet = jsonStringify(doc);
		DEBUG_TRACE("Sending websocket message: " + packet);
		int errc = session.ws->send(packet);
		if (errc)
		{
			return MIXER_ERROR_WS_SEND_FAILED;
		}

		if (nullptr != id)
		{
			*id = packetID;
		}
	}
	catch (std::exception e)
	{
		return MIXER_ERROR_WS_SEND_FAILED;
	}

	return MIXER_OK;
}

int receive_reply(interactive_session_internal& session, unsigned int id, std::shared_ptr<rapidjson::Document>& replyPtr, unsigned int timeoutMs)
{
	if (session.shutdownRequested)
	{
		return MIXER_OK;
	}

	// Wait for a reply
	std::shared_ptr<rapidjson::Document> spReply;
	auto replyItr = session.replies.end();
	{
		std::unique_lock<std::mutex> l(session.repliesMutex);
		replyItr = session.replies.find(id);
		while (session.replies.end() == replyItr)
		{
			session.repliesCV.wait_for(l, std::chrono::milliseconds(timeoutMs));
			if (session.shutdownRequested)
			{
				return MIXER_ERROR_CANCELLED;
			}
			replyItr = session.replies.find(id);
		}

		if (session.replies.end() == replyItr)
		{
			return MIXER_ERROR_RECEIVE_TIMED_OUT;
		}

		spReply = (*replyItr).second;
		session.replies.erase(id);
	}

	// Check for errors.
	rapidjson::Document& reply = *spReply;
	try
	{
		if (reply.HasMember(RPC_SEQUENCE) && !reply[RPC_SEQUENCE].IsNull())
		{
			session.sequenceId = reply[RPC_SEQUENCE].GetInt();
		}

		if (reply.HasMember(RPC_ERROR) && !reply[RPC_ERROR].IsNull())
		{
			int errCode = reply[RPC_ERROR][RPC_ERROR_CODE].GetInt();
			if (session.onError)
			{
				std::string errMessage = reply[RPC_ERROR][RPC_ERROR_MESSAGE].GetString();
				session.onError(session.callerContext, &session, errCode, errMessage.c_str(), errMessage.length());
			}

			return errCode;
		}
	}
	catch (std::exception e)
	{
		return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
	}

	replyPtr = spReply;
	return MIXER_OK;
}

int send_ready_message(interactive_session_internal& session, bool ready = true)
{
	return send_method(session, RPC_METHOD_READY, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_PARAM_IS_READY, ready, allocator);
	}, true, nullptr);
}

int handle_hello(interactive_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	interactive_state previousState = session.state;
	session.state = interactive_state::not_ready;
	if (session.onStateChanged)
	{
		session.onStateChanged(session.callerContext, &session, previousState, session.state);
	}

	if (!session.manualStart)
	{
		return send_ready_message(session);
	}

	return MIXER_OK;
}

int handle_input(interactive_session_internal& session, rapidjson::Document& doc)
{
	if (!session.onInput)
	{
		// No input handler, return.
		return MIXER_OK;
	}

	try
	{	
		interactive_input inputData;
		memset(&inputData, 0, sizeof(inputData));
		std::string inputJson = jsonStringify(doc[RPC_PARAMS]);
		inputData.jsonData = inputJson.c_str();
		inputData.jsonDataLength = inputJson.length();
		rapidjson::Value& input = doc[RPC_PARAMS][RPC_PARAM_INPUT];
		inputData.control.id = input[RPC_CONTROL_ID].GetString();
		inputData.control.idLength = input[RPC_CONTROL_ID].GetStringLength();
		
		if (doc[RPC_PARAMS].HasMember(RPC_PARTICIPANT_ID))
		{
			inputData.participantId = doc[RPC_PARAMS][RPC_PARTICIPANT_ID].GetString();
			inputData.participantIdLength = doc[RPC_PARAMS][RPC_PARTICIPANT_ID].GetStringLength();
		}

		// Locate the cached control data.
		auto itr = session.controls.find(inputData.control.id);
		if (itr == session.controls.end())
		{
			int errCode = MIXER_ERROR_OBJECT_NOT_FOUND;
			if (session.onError)
			{
				std::string errMessage = "Input received for unknown control.";
				session.onError(session.callerContext, &session, errCode, errMessage.c_str(), errMessage.length());
			}

			return errCode;
		}

		rapidjson::Value* control = rapidjson::Pointer(itr->second.c_str()).Get(session.scenesRoot);
		if (nullptr == control)
		{
			int errCode = MIXER_ERROR_OBJECT_NOT_FOUND;
			if (session.onError)
			{
				std::string errMessage = "Internal failure: Failed to find control in cached json data.";
				session.onError(session.callerContext, &session, errCode, errMessage.c_str(), errMessage.length());
			}

			return errCode;
		}

		inputData.control.kind = control->GetObject()[RPC_CONTROL_KIND].GetString();
		inputData.control.kindLength = control->GetObject()[RPC_CONTROL_KIND].GetStringLength();
		if (doc[RPC_PARAMS].HasMember(RPC_PARAM_TRANSACTION_ID))
		{
			inputData.transactionId = doc[RPC_PARAMS][RPC_PARAM_TRANSACTION_ID].GetString();
			inputData.transactionIdLength = doc[RPC_PARAMS][RPC_PARAM_TRANSACTION_ID].GetStringLength();
		}

		std::string inputEvent = input[RPC_PARAM_INPUT_EVENT].GetString();
		if (0 == inputEvent.compare(RPC_INPUT_EVENT_MOVE))
		{
			inputData.type = input_type_coordinate;
			inputData.coordinateData.x = input[RPC_INPUT_EVENT_MOVE_X].GetFloat();
			inputData.coordinateData.y = input[RPC_INPUT_EVENT_MOVE_Y].GetFloat();
		}
		else if (0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_DOWN) ||
			0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_UP) ||
			0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_DOWN) ||
			0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_UP))
		{
			inputData.type = input_type_button;
			bool keyDown = false;
			if (0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_DOWN) || 0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_DOWN))
			{
				keyDown = true;
			}
			inputData.buttonData.action = keyDown ? button_action::down : button_action::up;
		}
		else
		{
			inputData.type = input_type_custom;
		}

		session.onInput(session.callerContext, &session, &inputData);
	}
	catch (std::exception e)
	{
		int errCode = MIXER_ERROR_JSON_PARSE;
		if (session.onError)
		{
			std::string errMessage = "Exception while parsing interactive input data: " + std::string(e.what());
			session.onError(session.callerContext, &session, errCode, errMessage.c_str(), errMessage.length());
		}
		return errCode;
	}

	return MIXER_OK;
}


int handle_participants_change(interactive_session_internal& session, rapidjson::Document& doc, participant_action action)
{
	if (!session.onParticipantsChanged)
	{
		// No registered handler, ignore.
		return MIXER_OK;
	}

	if (!doc.HasMember(RPC_PARAMS) || !doc[RPC_PARAMS].HasMember(RPC_PARAM_PARTICIPANTS))
	{
		return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
	}

	rapidjson::Value& participants = doc[RPC_PARAMS][RPC_PARAM_PARTICIPANTS];
	for (auto itr = participants.Begin(); itr != participants.End(); ++itr)
	{
		interactive_participant participant;
		parse_participant(*itr, participant);

		switch (action)
		{
		case participant_join:
		case participant_update:
		{
			std::shared_ptr<rapidjson::Document> participantDoc(std::make_shared<rapidjson::Document>());
			participantDoc->CopyFrom(*itr, participantDoc->GetAllocator());
			session.participants[participant.id] = participantDoc;
			break;
		}
		case participant_leave:
		default:
		{
			session.participants.erase(participant.id);
			break;
		}
		}

		session.onParticipantsChanged(session.callerContext, &session, action, &participant);
	}

	return MIXER_OK;
}

int handle_participants_join(interactive_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_join);
}

int handle_participants_leave(interactive_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_leave);
}

int handle_participants_update(interactive_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_update);
}

int handle_ready(interactive_session_internal& session, rapidjson::Document& doc)
{
	if (!doc.HasMember(RPC_PARAMS) || !doc[RPC_PARAMS].HasMember(RPC_PARAM_IS_READY))
	{
		return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
	}

	bool isReady = doc[RPC_PARAMS][RPC_PARAM_IS_READY].GetBool();
	// Only change state and notify if the ready state is different.
	if (isReady && ready != session.state || !isReady && not_ready != session.state)
	{
		interactive_state previousState = session.state;
		session.state = isReady ? ready : not_ready;
		if (session.onStateChanged)
		{
			session.onStateChanged(session.callerContext, &session, previousState, session.state);
		}
	}

	return MIXER_OK;
}

int handle_group_changed(interactive_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	return cache_groups(session);
}

int handle_scene_changed(interactive_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	return cache_scenes(session);
}

int route_method(interactive_session_internal& session, rapidjson::Document& doc)
{
	std::string method = doc[RPC_METHOD].GetString();
	auto itr = session.methodHandlers.find(method);
	if (itr != session.methodHandlers.end())
	{
		return itr->second(session, doc);
	}
	else
	{
		DEBUG_WARNING("Unhandled method type: " + method);
		if (session.onUnhandledMethod)
		{
			std::string methodJson = jsonStringify(doc);
			session.onUnhandledMethod(session.callerContext, &session, methodJson.c_str(), methodJson.length());
		}
	}

	return MIXER_OK;
}

int get_hosts(interactive_session_internal& session)
{
	DEBUG_INFO("Retrieving hosts.");
	http_response response;
	static std::string hosts = "https://mixer.com/api/v1/interactive/hosts";
	RETURN_IF_FAILED(session.http->make_request(hosts, "GET", nullptr, "", response));

	if (200 != response.statusCode)
	{
		return MIXER_ERROR_NO_HOST;
	}

	try
	{
		rapidjson::Document doc;
		doc.Parse(response.body.c_str());
		if (!doc.IsArray())
		{
			return MIXER_ERROR_NO_HOST;
		}

		for (auto itr = doc.Begin(); itr != doc.End(); ++itr)
		{
			auto addressItr = itr->FindMember("address");
			if (addressItr != itr->MemberEnd())
			{
				session.hosts.push_back(addressItr->value.GetString());
				DEBUG_TRACE("Host found: " + std::string(addressItr->value.GetString(), addressItr->value.GetStringLength()));
			}
		}
	}
	catch (std::exception e)
	{
		return MIXER_ERROR_NO_HOST;
	}

	return MIXER_OK;
}

void register_method_handlers(interactive_session_internal& session)
{
	session.methodHandlers.emplace(RPC_METHOD_HELLO, handle_hello);
	session.methodHandlers.emplace(RPC_METHOD_ON_READY_CHANGED, handle_ready);
	session.methodHandlers.emplace(RPC_METHOD_ON_INPUT, handle_input);
	session.methodHandlers.emplace(RPC_METHOD_ON_PARTICIPANT_JOIN, handle_participants_join);
	session.methodHandlers.emplace(RPC_METHOD_ON_PARTICIPANT_LEAVE, handle_participants_leave);
	session.methodHandlers.emplace(RPC_METHOD_ON_PARTICIPANT_UPDATE, handle_participants_update);
	session.methodHandlers.emplace(RPC_METHOD_ON_GROUP_UPDATE, handle_group_changed);
	session.methodHandlers.emplace(RPC_METHOD_ON_GROUP_CREATE, handle_group_changed);
	session.methodHandlers.emplace(RPC_METHOD_UPDATE_SCENES, handle_scene_changed);
}

int update_server_time_offset(interactive_session_internal& session)
{
	// Calculate the server time offset.
	DEBUG_INFO("Calculating server time offset.");
	unsigned int id;
	int err = send_method(session, RPC_METHOD_GET_TIME, nullptr, false, &id);
	if (err)
	{
		DEBUG_ERROR("Method "  RPC_METHOD_GET_TIME " failed: " + std::to_string(err));
		return err;
	}
	auto sentTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

	std::shared_ptr<rapidjson::Document> replyDoc;
	err = receive_reply(session, id, replyDoc);
	if (err)
	{
		DEBUG_ERROR("Failed to receive reply for " RPC_METHOD_GET_TIME ": " + std::to_string(err));
		return err;
	}

	auto receivedTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	auto latency = (receivedTime - sentTime) / 2;

	try
	{
		unsigned long long serverTime = (*replyDoc)[RPC_RESULT][RPC_TIME].GetUint64();
		auto offset = receivedTime - latency - std::chrono::milliseconds(serverTime);
		session.serverTimeOffsetMs = offset.time_since_epoch().count();
		DEBUG_INFO("Server time offset: " + std::to_string(session.serverTimeOffsetMs));
	}
	catch (std::exception e)
	{
		DEBUG_ERROR(std::string("Failed to parse server time: ") + e.what());
		return err;
	}

	return MIXER_OK;
}

// Connect to and interactive session with the supplied interactive configuration. 
int interactive_connect(const char* auth, const char* versionId, const char* shareCode, bool manualStart, interactive_session* sessionPtr)
{
	if (nullptr == auth || nullptr == versionId || nullptr == sessionPtr)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	// Validate parameters
	if (0 == strlen(auth) || 0 == strlen(versionId))
	{
		return MIXER_ERROR_INVALID_VERSION_ID;
	}

	std::auto_ptr<interactive_session_internal> session(new interactive_session_internal(manualStart));

	// Register method handlers
	register_method_handlers(*session);

	// Initialize Http
	session->http = http_factory::make_http_client();
	RETURN_IF_FAILED(get_hosts(*session));

	// Connect long running websocket.
	session->ws = websocket_factory::make_websocket();
	session->ws->add_header("X-Protocol-Version", "2.0");
	session->ws->add_header("Authorization", auth);
	session->ws->add_header("X-Interactive-Version", versionId);
	if (nullptr != shareCode)
	{
		session->ws->add_header("X-Interactive-Sharecode", shareCode);
	}

	// Create a thread to wait on messages from the websocket.
	session->wsThread = std::thread(std::bind(&interactive_session_internal::run_ws, session.get()));

	std::unique_lock<std::mutex> wsOpenLock(session->wsOpenMutex);
	if (!session->wsOpen)
	{
		session->wsOpenCV.wait(wsOpenLock);
	}

	if (!session->wsOpen)
	{
		return MIXER_ERROR_WS_CONNECT_FAILED;
	}

	// Get the server time offset.
	int err = update_server_time_offset(*session);
	if (err)
	{
		// Warn about this but don't fail interactive connecting as this may only affect control cooldowns.
		DEBUG_WARNING("Failed to update server time offset: " + std::to_string(err));
	}

	// Cache scene and group data.
	RETURN_IF_FAILED(cache_scenes(*session));
	DEBUG_TRACE("Cached scene data: " + jsonStringify(session->scenesRoot));
	RETURN_IF_FAILED(cache_groups(*session));

	*sessionPtr = session.release();

	return MIXER_OK;
}

int interactive_set_session_context(interactive_session session, void* context)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->callerContext = context;

	return MIXER_OK;
}

int interactive_run(interactive_session session, unsigned int maxEventsToProcess)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	if (0 == maxEventsToProcess)
	{
		return MIXER_OK;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	if (sessionInternal->shutdownRequested)
	{
		return MIXER_OK;
	}

	unsigned int processed = 0;

	// Check for any errors first
	if (!sessionInternal->errors.empty())
	{
		std::queue<std::pair<unsigned short, std::string>> errors;
		{
			std::lock_guard<std::mutex> l(sessionInternal->errorsMutex);
			while (processed++ < maxEventsToProcess)
			{
				errors.emplace(sessionInternal->errors.front());
				sessionInternal->errors.pop();
			}
		}

		if (sessionInternal->onError)
		{
			while (!errors.empty())
			{
				std::pair<unsigned short, std::string> error = errors.front();
				errors.pop();
				sessionInternal->onError(sessionInternal->callerContext, &sessionInternal, error.first, error.second.c_str(), error.second.length());
				if (sessionInternal->shutdownRequested)
				{
					return MIXER_OK;
				}
			}
		}
	}

	// Process methods
	if (processed < maxEventsToProcess)
	{
		std::queue<std::shared_ptr<rapidjson::Document>> methods;
		{
			std::lock_guard<std::mutex> l(sessionInternal->methodsMutex);
			while (!sessionInternal->methods.empty() && processed++ < maxEventsToProcess)
			{
				methods.emplace(sessionInternal->methods.front());
				sessionInternal->methods.pop();
			}
		}

		while (!methods.empty())
		{
			std::shared_ptr<rapidjson::Document> method = methods.front();
			methods.pop();

			if (method->HasMember(RPC_SEQUENCE))
			{
				sessionInternal->sequenceId = (*method)[RPC_SEQUENCE].GetInt();
			}

			RETURN_IF_FAILED(route_method(*sessionInternal, *method));
			if (sessionInternal->shutdownRequested)
			{
				return MIXER_OK;
			}
		}
	}
	else
	{

	}

	return MIXER_OK;
}

int interactive_get_state(interactive_session session, interactive_state* state)
{
	if (nullptr == session || nullptr == state)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	*state = sessionInternal->state;

	return MIXER_OK;
}

int interactive_set_ready(interactive_session session, bool isReady)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	return send_ready_message(*sessionInternal, isReady);
}

int interactive_capture_transaction(interactive_session session, const char* transactionId)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	unsigned int id;
	RETURN_IF_FAILED(send_method(*sessionInternal, RPC_METHOD_CAPTURE, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_PARAM_TRANSACTION_ID, rapidjson::StringRef(transactionId), allocator);
	}, false, &id));

	std::shared_ptr<rapidjson::Document> doc;
	return receive_reply(*sessionInternal, id, doc);
}

int interactive_control_trigger_cooldown(interactive_session session, const char* controlId, const unsigned long long cooldownMs)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	// Locate the cached control data.
	auto itr = sessionInternal->controls.find(controlId);
	if (itr == sessionInternal->controls.end())
	{
		int errCode = MIXER_ERROR_OBJECT_NOT_FOUND;
		if (sessionInternal->onError)
		{
			std::string errMessage = "Input received for unknown control.";
			sessionInternal->onError(sessionInternal->callerContext, &session, errCode, errMessage.c_str(), errMessage.length());
		}

		return errCode;
	}

	std::string controlPtr = itr->second;

	// The controlPtr is prefixed with a scene pointer, parse it to find the scene this control belongs to.
	size_t controlOffset = controlPtr.find("controls", 0);
	std::string scenePtr = controlPtr.substr(0, controlOffset - 1);

	// Get the scene id.
	rapidjson::Value* scene = rapidjson::Pointer(rapidjson::StringRef(scenePtr.c_str(), scenePtr.length())).Get(sessionInternal->scenesRoot);
	std::string sceneId = (*scene)[RPC_SCENE_ID].GetString();
	long long cooldownTimestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count() - sessionInternal->serverTimeOffsetMs + cooldownMs;

	RETURN_IF_FAILED(send_method(*sessionInternal, RPC_METHOD_UPDATE_CONTROLS, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_SCENE_ID, rapidjson::StringRef(sceneId.c_str(), sceneId.length()), allocator);
		params.AddMember("priority", 1, allocator);

		rapidjson::Value controls(rapidjson::kArrayType);
		rapidjson::Value control(rapidjson::kObjectType);
		control.AddMember(RPC_CONTROL_ID, rapidjson::StringRef(controlId), allocator);
		control.AddMember(RPC_CONTROL_BUTTON_COOLDOWN, cooldownTimestamp, allocator);
		controls.PushBack(control, allocator);
		params.AddMember(RPC_PARAM_CONTROLS, controls, allocator);
	}, true, nullptr));

	return MIXER_OK;
}

int interactive_send_method(interactive_session session, const char* method, const char* paramsJson, bool discardReply, unsigned int* id)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	rapidjson::Document paramsDoc;
	try
	{
		paramsDoc.Parse(paramsJson);
	}
	catch (...)
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	RETURN_IF_FAILED(send_method(*sessionInternal, method, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.CopyFrom(paramsDoc, allocator);
	}, discardReply, id));

	return MIXER_OK;
}

int interactive_receive_reply(interactive_session session, unsigned int id, unsigned int timeoutMs, char* replyJson, size_t* replyJsonLength)
{
	if (nullptr == session || nullptr == replyJsonLength)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	std::shared_ptr<rapidjson::Document> replyDoc;
	RETURN_IF_FAILED(receive_reply(*sessionInternal, id, replyDoc, timeoutMs));
	std::string replyJsonStr = jsonStringify(*replyDoc);

	if (nullptr == replyJson || *replyJsonLength < replyJsonStr.length() + 1)
	{
		*replyJsonLength = replyJsonStr.length() + 1;
		// Put the reply back
		std::lock_guard<std::mutex> l(sessionInternal->repliesMutex);
		sessionInternal->replies[id] = replyDoc;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(replyJson, replyJsonStr.c_str(), replyJsonStr.length());
	replyJson[replyJsonStr.length()] = 0;
	*replyJsonLength = replyJsonStr.length() + 1;
	return MIXER_OK;
}

void interactive_disconnect(interactive_session session)
{
	if (nullptr != session)
	{
		interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
		sessionInternal->shutdownRequested = true;
		if (nullptr != sessionInternal->ws.get())
		{
			sessionInternal->ws->close();
		}

		sessionInternal->wsThread.join();
		delete sessionInternal;
	}
}

// Handler registration
int interactive_reg_error_handler(interactive_session session, on_error onError)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onError = onError;

	return MIXER_OK;
}

int interactive_reg_state_changed_handler(interactive_session session, on_state_changed onStateChanged)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onStateChanged = onStateChanged;

	return MIXER_OK;
}

int interactive_reg_input_handler(interactive_session session, on_input onInput)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onInput = onInput;

	return MIXER_OK;
}

int interactive_reg_participants_changed_handler(interactive_session session, on_participants_changed onParticipantsChanged)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onParticipantsChanged = onParticipantsChanged;

	return MIXER_OK;
}

int interactive_reg_unhandled_method_handler(interactive_session session, on_unhandled_method onUnhandledMethod)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onUnhandledMethod = onUnhandledMethod;

	return MIXER_OK;
}

}