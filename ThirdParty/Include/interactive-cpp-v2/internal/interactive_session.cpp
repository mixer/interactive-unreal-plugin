#include "interactive_session.h"
#include "common.h"

#include <functional>

namespace mixer_internal
{

typedef std::function<void(rapidjson::Document::AllocatorType& allocator, rapidjson::Value& value)> on_get_params;

int create_method_json(interactive_session_internal& session, const std::string& method, on_get_params getParams, bool discard, unsigned int* id, std::shared_ptr<rapidjson::Document>& methodDoc)
{
	std::shared_ptr<rapidjson::Document> doc(std::make_shared<rapidjson::Document>());
	doc->SetObject();
	rapidjson::Document::AllocatorType& allocator = doc->GetAllocator();

	unsigned int packetID = session.packetId++;
	doc->AddMember(RPC_ID, packetID, allocator);
	doc->AddMember(RPC_METHOD, method, allocator);
	doc->AddMember(RPC_DISCARD, discard, allocator);
	doc->AddMember(RPC_SEQUENCE, session.sequenceId, allocator);

	// Get the parameters from the caller.
	rapidjson::Value params(rapidjson::kObjectType);
	if (getParams)
	{
		getParams(allocator, params);
	}
	doc->AddMember(RPC_PARAMS, params, allocator);

	if (nullptr != id)
	{
		*id = packetID;
	}
	methodDoc = doc;
	return MIXER_OK;
}

int send_method(interactive_session_internal& session, const std::string& method, on_get_params getParams, bool discard, unsigned int* id)
{
	std::shared_ptr<rapidjson::Document> methodDoc;
	RETURN_IF_FAILED(create_method_json(session, method, getParams, discard, id, methodDoc));

	// Synchronize access to the websocket.
	std::string methodJson = jsonStringify(*methodDoc);
	std::lock_guard<std::mutex> sendLock(session.sendMutex);

	DEBUG_TRACE("Sending websocket message: " + methodJson);
	return session.ws->send(methodJson);
}

int queue_method(interactive_session_internal& session, const std::string& method, on_get_params getParams, method_handler onReply)
{
	std::shared_ptr<rapidjson::Document> methodDoc;
	unsigned int packetId = 0;
	RETURN_IF_FAILED(create_method_json(session, method, getParams, nullptr == onReply, &packetId, methodDoc));
	DEBUG_TRACE(std::string("Queueing method: ") + jsonStringify(*methodDoc));
	if (onReply)
	{
		session.replyHandlersById[packetId] = onReply;
	}

	// Synchronize write access to the queue.
	std::unique_lock<std::mutex> queueLock(session.outgoingMutex);
	session.outgoingMethods.emplace(methodDoc);
	session.outgoingCV.notify_one();

	return MIXER_OK;
}

int queue_request(interactive_session_internal& session, const std::string uri, std::string& verb, const std::map<std::string, std::string>* headers, const std::string* body, http_response_handler onResponse)
{
	http_request_data httpRequest;
	httpRequest.packetId = session.packetId++;
	httpRequest.uri = uri;
	httpRequest.verb = verb;
	if (nullptr != headers)
	{
		httpRequest.headers = *headers;
	}
	if (nullptr != body)
	{
		httpRequest.body = *body;
	}

	if (nullptr != onResponse)
	{
		session.httpResponseHandlers[httpRequest.packetId] = onResponse;
	}

	// Queue the request, synchronizing access.
	{
		std::unique_lock<std::mutex> lock(session.outgoingMutex);
		session.outgoingRequests.emplace(httpRequest);
		session.outgoingCV.notify_one();
	}

	return MIXER_OK;
}

int check_reply_errors(interactive_session_internal& session, rapidjson::Document& reply)
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
	{
		std::unique_lock<std::mutex> l(session.repliesMutex);
		auto replyItr = session.replies.end();
		replyItr = session.replies.find(id);
		while (session.replies.end() == replyItr)
		{
			auto waitStatus = session.repliesCV.wait_for(l, std::chrono::milliseconds(timeoutMs));
			if (waitStatus == std::cv_status::timeout)
			{
				return MIXER_ERROR_TIMED_OUT;
			}

			if (session.shutdownRequested)
			{
				return MIXER_ERROR_CANCELLED;
			}
			replyItr = session.replies.find(id);
		}

		if (session.replies.end() == replyItr)
		{
			return MIXER_ERROR_TIMED_OUT;
		}

		spReply = (*replyItr).second;
		session.replies.erase(replyItr);
	}

	// Check for errors.
	RETURN_IF_FAILED(check_reply_errors(session, *spReply));

	replyPtr = spReply;
	return MIXER_OK;
}

int send_ready_message(interactive_session_internal& session, bool ready = true)
{
	return queue_method(session, RPC_METHOD_READY, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_PARAM_IS_READY, ready, allocator);
	}, nullptr);
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

	if (session.isReady && interactive_state::ready != session.state)
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
		inputData.type = input_type_move;
		inputData.coordinateData.x = input[RPC_INPUT_EVENT_MOVE_X].GetFloat();
		inputData.coordinateData.y = input[RPC_INPUT_EVENT_MOVE_Y].GetFloat();
	}
	else if (0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_DOWN) ||
		0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_UP))
	{
		inputData.type = input_type_key;
		interactive_button_action action = interactive_button_action_up;
		if (0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_DOWN))
		{
			action = interactive_button_action_down;
		}

		inputData.buttonData.action = action;
	}
	else if (0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_DOWN) ||
		0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_UP))
	{
		inputData.type = input_type_click;
		interactive_button_action action = interactive_button_action_up;
		if (0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_DOWN))
		{
			action = interactive_button_action_down;
		}

		inputData.buttonData.action = action;

		if (input.HasMember(RPC_INPUT_EVENT_MOVE_X))
		{
			inputData.coordinateData.x = input[RPC_INPUT_EVENT_MOVE_X].GetFloat();
		}
		if (input.HasMember(RPC_INPUT_EVENT_MOVE_Y))
		{
			inputData.coordinateData.y = input[RPC_INPUT_EVENT_MOVE_Y].GetFloat();
		}
	}
	else
	{
		inputData.type = input_type_custom;
	}

	session.onInput(session.callerContext, &session, &inputData);

	return MIXER_OK;
}

int handle_participants_change(interactive_session_internal& session, rapidjson::Document& doc, interactive_participant_action action)
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

	rapidjson::Document doc;
	if (doc.Parse(response.body.c_str()).HasParseError() || !doc.IsArray())
	{
		return MIXER_ERROR_JSON_PARSE;
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

	return MIXER_OK;
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

	if (!replyDoc->HasMember(RPC_RESULT) || !(*replyDoc)[RPC_RESULT].HasMember(RPC_TIME))
	{
		DEBUG_ERROR("Unexpected reply format for server time reply");
	}

	auto receivedTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	auto latency = (receivedTime - sentTime) / 2;
	unsigned long long serverTime = (*replyDoc)[RPC_RESULT][RPC_TIME].GetUint64();
	auto offset = receivedTime - latency - std::chrono::milliseconds(serverTime);
	session.serverTimeOffsetMs = offset.time_since_epoch().count();
	DEBUG_INFO("Server time offset: " + std::to_string(session.serverTimeOffsetMs));
	return MIXER_OK;
}

int do_connect(interactive_session_internal& session, bool isReady)
{
	session.isReady = isReady;

	int err = 0;
	if (session.shutdownRequested)
	{
		return MIXER_OK;
	}

	err = get_hosts(session);
	if (err && session.onError)
	{
		std::string errorMessage = "Failed to acquire interactive host servers.";
		session.onError(session.callerContext, &session, err, errorMessage.c_str(), errorMessage.length());
		return err;
	}

	// Connect long running websocket.
	session.ws->add_header("X-Protocol-Version", "2.0");
	session.ws->add_header("Authorization", session.authorization);
	session.ws->add_header("X-Interactive-Version", session.versionId);
	if (!session.shareCode.empty())
	{
		session.ws->add_header("X-Interactive-Sharecode", session.shareCode);
	}

	// Create thread to open websocket and receive messages.
	session.incomingThread = std::thread(std::bind(&interactive_session_internal::run_incoming_thread, &session));

	std::unique_lock<std::mutex> wsOpenLock(session.wsOpenMutex);
	if (!session.wsOpen)
	{
		session.wsOpenCV.wait(wsOpenLock);
	}

	if (!session.wsOpen)
	{
		return MIXER_ERROR_WS_CONNECT_FAILED;
	}

	// Create thread to send messages over the open websocket.
	session.outgoingThread = std::thread(std::bind(&interactive_session_internal::run_outgoing_thread, &session));

	// Get the server time offset.
	err = update_server_time_offset(session);
	if (err)
	{
		// Warn about this but don't fail interactive connecting as this may only affect control cooldowns.
		DEBUG_WARNING("Failed to update server time offset: " + std::to_string(err));
	}

	// Cache scene and group data.
	RETURN_IF_FAILED(cache_scenes(session));
	DEBUG_TRACE("Cached scene data: " + jsonStringify(session.scenesRoot));
	RETURN_IF_FAILED(cache_groups(session));

	return MIXER_OK;
}

}

using namespace mixer_internal;

int interactive_open_session(const char* auth, const char* versionId, const char* shareCode, bool setReady, interactive_session* sessionPtr)
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

	std::auto_ptr<interactive_session_internal> session(new interactive_session_internal());
	session->authorization = auth;
	session->versionId = versionId;
	if (nullptr != shareCode)
	{
		session->shareCode = shareCode;
	}

	// Register method handlers
	register_method_handlers(*session);

	// Initialize Http and Websocket clients
	session->http = http_factory::make_http_client();
	session->ws = websocket_factory::make_websocket();
	do_connect(*session, setReady);

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

int interactive_get_session_context(interactive_session session, void** context)
{
	if (nullptr == session || nullptr == context)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	*context = sessionInternal->callerContext;
	return MIXER_OK;
}

int interactive_set_bandwidth_throttle(interactive_session session, interactive_throttle_type throttleType, unsigned int maxBytes, unsigned int bytesPerSecond)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	std::string throttleMethod;
	switch (throttleType)
	{
	case throttle_input:
		throttleMethod = RPC_METHOD_ON_INPUT;
		break;
	case throttle_participant_join:
		throttleMethod = RPC_METHOD_ON_PARTICIPANT_JOIN;
		break;
	case throttle_participant_leave:
		throttleMethod = RPC_METHOD_ON_PARTICIPANT_LEAVE;
		break;
	case throttle_global:
	default:
		throttleMethod = "*";
		break;
	}

	RETURN_IF_FAILED(queue_method(*sessionInternal, RPC_METHOD_SET_THROTTLE, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		rapidjson::Value method(rapidjson::kObjectType);
		method.SetObject();
		method.AddMember(RPC_PARAM_CAPACITY, maxBytes, allocator);
		method.AddMember(RPC_PARAM_DRAIN_RATE, bytesPerSecond, allocator);
		rapidjson::Value throttleVal(rapidjson::kStringType);
		throttleVal.SetString(throttleMethod, allocator);
		params.AddMember(throttleVal, method, allocator);
	}, check_reply_errors));

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
		return MIXER_ERROR_CANCELLED;
	}

	unsigned int processed = 0;

	// Check for any errors first.
	if (!sessionInternal->errors.empty() && processed < maxEventsToProcess)
	{
		std::queue<protocol_error> errors;
		{
			std::lock_guard<std::mutex> l(sessionInternal->errorsMutex);
			while (!sessionInternal->errors.empty() && processed++ < maxEventsToProcess)
			{
				errors.emplace(sessionInternal->errors.front());
				sessionInternal->errors.pop();
			}
		}

		if (sessionInternal->onError)
		{
			while (!errors.empty())
			{
				protocol_error error = errors.front();
				errors.pop();
				sessionInternal->onError(sessionInternal->callerContext, &sessionInternal, error.first, error.second.c_str(), error.second.length());

				if (sessionInternal->shutdownRequested)
				{
					return MIXER_OK;
				}
			}
		}
	}

	// Process any websocket replies.
	if (!sessionInternal->replies.empty() && processed < maxEventsToProcess)
	{
		std::unique_lock<std::mutex> repliesLock(sessionInternal->repliesMutex);

		for (auto replyByIdItr = sessionInternal->replies.begin(); replyByIdItr != sessionInternal->replies.end() && processed++ < maxEventsToProcess; /* No increment */)
		{
			auto replyHandlerItr = sessionInternal->replyHandlersById.find(replyByIdItr->first);
			if (replyHandlerItr != sessionInternal->replyHandlersById.end())
			{
				// Call the registered handler for this reply
				std::shared_ptr<rapidjson::Document> replyDoc = replyByIdItr->second;
				replyHandlerItr->second(*sessionInternal, *replyDoc);
			}

			// This reply was processed, clear it.
			sessionInternal->replies.erase(replyByIdItr++);

			if (sessionInternal->shutdownRequested)
			{
				break;
			}
		}
	}

	// Process any http responses.
	if (!sessionInternal->httpResponsesById.empty() && processed < maxEventsToProcess)
	{
		std::unique_lock<std::mutex> responsesLock(sessionInternal->httpResponsesMutex);
		
		for (auto responsesItr = sessionInternal->httpResponsesById.begin(); responsesItr != sessionInternal->httpResponsesById.end(); /* No increment */)
		{
			// Check if there is a handler for this response
			auto responseHandlerItr = sessionInternal->httpResponseHandlers.find(responsesItr->first);
			if (responseHandlerItr != sessionInternal->httpResponseHandlers.end())
			{
				responseHandlerItr->second(responsesItr->second.statusCode, responsesItr->second.body);
				
				// Clean up this handler now that is has been called.
				sessionInternal->httpResponseHandlers.erase(responseHandlerItr);
			}

			// Clean up this response.
			sessionInternal->httpResponsesById.erase(responsesItr++);

			if (sessionInternal->shutdownRequested)
			{
				break;
			}
		}
	}

	// Process any incoming methods last.
	if (processed < maxEventsToProcess)
	{
		std::queue<std::shared_ptr<rapidjson::Document>> methods;
		{
			std::lock_guard<std::mutex> l(sessionInternal->methodsMutex);
			while (!sessionInternal->incomingMethods.empty() && processed++ < maxEventsToProcess)
			{
				methods.emplace(sessionInternal->incomingMethods.front());
				sessionInternal->incomingMethods.pop();
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
	if (nullptr == session || nullptr == transactionId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	std::string transactionIdStr(transactionId);
	RETURN_IF_FAILED(queue_method(*sessionInternal, RPC_METHOD_CAPTURE, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_PARAM_TRANSACTION_ID, transactionIdStr, allocator);
	}, [transactionIdStr](interactive_session_internal& session, rapidjson::Document& replyDoc)
	{
		if (session.onTransactionComplete)
		{
			unsigned int err = 0;
			std::string errMessage;
			if (replyDoc.HasMember(RPC_ERROR))
			{
				if (replyDoc[RPC_ERROR].IsObject() && replyDoc[RPC_ERROR].HasMember(RPC_ERROR_CODE))
				{
					err = replyDoc[RPC_ERROR][RPC_ERROR_CODE].GetUint();
				}
				if (replyDoc[RPC_ERROR].IsObject() && replyDoc[RPC_ERROR].HasMember(RPC_ERROR_MESSAGE))
				{
					errMessage = replyDoc[RPC_ERROR][RPC_ERROR_MESSAGE].GetString();
				}
			}

			session.onTransactionComplete(session.callerContext, &session, transactionIdStr.c_str(), transactionIdStr.length(), err, errMessage.c_str(), errMessage.length());
		}

		return MIXER_OK;
	}));

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
	if (paramsDoc.Parse(paramsJson).HasParseError())
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

void interactive_close_session(interactive_session session)
{
	if (nullptr != session)
	{
		interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

		// Mark the session as inactive and close the websocket. This will notify any functions in flight to exit at their earliest convenience.
		sessionInternal->shutdownRequested = true;
		if (nullptr != sessionInternal->ws.get())
		{
			sessionInternal->ws->close();
		}

		// Notify the outgoing websocket thread to shutdown.
		{
			std::unique_lock<std::mutex> outgoingLock(sessionInternal->outgoingMutex);
			sessionInternal->outgoingCV.notify_all();
		}

		// Wait for both threads to terminate.
		sessionInternal->incomingThread.join();
		sessionInternal->outgoingThread.join();

		// Clean up the session memory.
		delete sessionInternal;
	}
}

// Handler registration
int interactive_register_error_handler(interactive_session session, on_error onError)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onError = onError;

	return MIXER_OK;
}

int interactive_register_state_changed_handler(interactive_session session, on_state_changed onStateChanged)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onStateChanged = onStateChanged;

	return MIXER_OK;
}

int interactive_register_input_handler(interactive_session session, on_input onInput)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onInput = onInput;

	return MIXER_OK;
}

int interactive_register_participants_changed_handler(interactive_session session, on_participants_changed onParticipantsChanged)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onParticipantsChanged = onParticipantsChanged;

	return MIXER_OK;
}

int interactive_register_transaction_complete_handler(interactive_session session, on_transaction_complete onTransactionComplete)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onTransactionComplete = onTransactionComplete;

	return MIXER_OK;
}

int interactive_register_unhandled_method_handler(interactive_session session, on_unhandled_method onUnhandledMethod)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	sessionInternal->onUnhandledMethod = onUnhandledMethod;

	return MIXER_OK;
}

// Debugging

void interactive_config_debug_level(const interactive_debug_level dbgLevel)
{
	g_dbgInteractiveLevel = dbgLevel;
}

void interactive_config_debug(const interactive_debug_level dbgLevel, const on_debug_msg dbgCallback)
{
	g_dbgInteractiveLevel = dbgLevel;
	g_dbgInteractiveCallback = dbgCallback;
}