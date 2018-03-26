//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

namespace mixer
{

// Known control properties
#define CONTROL_PROP_DISABLED "disabled"
#define CONTROL_PROP_POSITION "position"

#define BUTTON_PROP_KEY_CODE "keyCode"
#define BUTTON_PROP_TEXT "text"
#define BUTTON_PROP_TOOLTIP "tooltip"
#define BUTTON_PROP_COST "cost"
#define BUTTON_PROP_PROGRESS "progress"
#define BUTTON_PROP_COOLDOWN "cooldown"

#define JOYSTICK_PROP_SAMPLE_RATE "sampleRate"
#define JOYSTICK_PROP_ANGLE "angle"
#define JOYSTICK_PROP_INTENSITY "intensity"

extern "C" {

	typedef enum mixer_result_code
	{
		MIXER_OK,
		MIXER_ERROR,
		MIXER_ERROR_AUTH,
		MIXER_ERROR_AUTH_DENIED,
		MIXER_ERROR_AUTH_TIMEOUT,
		MIXER_ERROR_AUTH_INVALID_TOKEN,
		MIXER_ERROR_BUFFER_SIZE,
		MIXER_ERROR_CANCELLED,
		MIXER_ERROR_HTTP,
		MIXER_ERROR_INIT,
		MIXER_ERROR_INVALID_CALLBACK,
		MIXER_ERROR_INVALID_CLIENT_ID,
		MIXER_ERROR_INVALID_OPERATION,
		MIXER_ERROR_INVALID_POINTER,
		MIXER_ERROR_INVALID_PROPERTY_TYPE,
		MIXER_ERROR_INVALID_VERSION_ID,
		MIXER_ERROR_JSON_PARSE,
		MIXER_ERROR_NO_HOST,
		MIXER_ERROR_NO_REPLY,
		MIXER_ERROR_OBJECT_NOT_FOUND,
		MIXER_ERROR_PROPERTY_NOT_FOUND,
		MIXER_ERROR_RECEIVE_TIMED_OUT,
		MIXER_ERROR_UNKNOWN_METHOD,
		MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT,
		MIXER_ERROR_WS_CLOSED,
		MIXER_ERROR_WS_CONNECT_FAILED,
		MIXER_ERROR_WS_DISCONNECT_FAILED,
		MIXER_ERROR_WS_READ_FAILED,
		MIXER_ERROR_WS_SEND_FAILED,
		
	} mixer_result_code;

	enum interactive_state
	{
		disconnected,
		not_ready,
		ready
	};

	struct interactive_object
	{
		const char* id;
		size_t idLength;
	};

	struct interactive_participant : public interactive_object
	{	
		unsigned int userId;
		const char* userName;
		size_t usernameLength;
		unsigned int level;
		unsigned long long lastInputAtMs;
		unsigned long long connectedAtMs;
		bool disabled;
		const char* groupId;
		size_t groupIdLength;
	};

	struct interactive_control : public interactive_object
	{
		const char* kind;
		size_t kindLength;
	};

	struct interactive_input
	{
		interactive_control control;
		const char* participantId;
		size_t participantIdLength;
	};

	enum button_action
	{
		up,
		down
	};

	struct interactive_button_input : interactive_input
	{
		button_action action;
		const char* transactionId;
		size_t transactionIdLength;
	};

	struct interactive_coordinate_input : interactive_input
	{
		float x;
		float y;
	};

	struct interactive_group : public interactive_object
	{
		const char* sceneId;
		size_t sceneIdLength;
	};

	struct interactive_scene : public interactive_object
	{
	};

	typedef void* interactive_session;
	
	// Interactive events
	typedef void(*on_error)(void* context, interactive_session session, int errorCode, const char* errorMessage, size_t errorMessageLength);
	typedef void(*on_state_changed)(void* context, interactive_session session, interactive_state previousState, interactive_state newState);
	typedef void(*on_button_input)(void* context, interactive_session session, const interactive_button_input* input);
	typedef void(*on_coordinate_input)(void* context, interactive_session session, const interactive_coordinate_input* input);
	typedef void(*on_custom_input)(void* context, interactive_session session, const interactive_input* input);
	typedef void(*on_unhandled_method)(void* context, interactive_session session, const char* methodJson, size_t methodJsonLength);

	// Enumeration callbacks
	typedef void(*on_group_enumerate)(void* context, interactive_session session, interactive_group* group);
	typedef void(*on_scene_enumerate)(void* context, interactive_session session, interactive_scene* scene);
	typedef void(*on_control_enumerate)(void* context, interactive_session session, interactive_control* control);
	typedef void(*on_participant_enumerate)(void* context, interactive_session session, interactive_participant* participant);

	enum participant_action
	{
		participant_join,
		participant_leave,
		participant_update
	};

	typedef void(*on_participants_changed)(void* context, interactive_session session, participant_action action, const interactive_participant* participant);

	// Interactive authorization helpers
	int interactive_auth_get_short_code(const char* clientId, char* shortCode, size_t* shortCodeLength, char* shortCodeHandle, size_t* shortCodeHandleLength);
	int interactive_auth_wait_short_code(const char* clientId, const char* shortCodeHandle, char* refreshToken, size_t* refreshTokenLength);
	int interactive_auth_is_token_stale(const char* token, bool* isStale);
	int interactive_auth_refresh_token(const char* clientId, const char* staleToken, char* refreshToken, size_t* refreshTokenLength);
	int interactive_auth_parse_refresh_token(const char* token, char* authorization, size_t* authorizationLength);

	/// <summary>
	/// Connect to an interactive session with the supplied <c>interactive_config</c>. 
	/// </summary>
	int interactive_connect(const char* auth, const char* versionId, const char* shareCode, bool manualStart, interactive_session* sessionPtr);

	/// <summary>
	/// Disconnect from an interactive session and destroy it. This must not be called from inside an event handler as the lifetime of registered event handlers is assumed to outlive 
	/// the session. Only call this when there is no thread processing events via interactive_run.
	/// </summary>
	void interactive_disconnect(interactive_session session);

	int interactive_get_state(interactive_session session, interactive_state* state);
	int interactive_set_ready(interactive_session session, bool isReady);

	/// <summary>
	/// Set a session context that will be passed to every event callback.
	/// </summary>
	int interactive_set_session_context(interactive_session session, void* context);

	// Event handlers
	int interactive_reg_error_handler(interactive_session session, on_error onError);
	int interactive_reg_state_changed_handler(interactive_session session, on_state_changed onStateChanged);
	int interactive_reg_button_input_handler(interactive_session session, on_button_input onButtonInput);
	int interactive_reg_coordinate_input_handler(interactive_session session, on_coordinate_input onCoordinateInput);
	int interactive_reg_participants_changed_handler(interactive_session session, on_participants_changed onParticipantsChanged);
	int interactive_reg_unhandled_method_handler(interactive_session session, on_unhandled_method onUnhandledMethod);

	int interactive_run(interactive_session session, unsigned int maxEventsToProcess);

	/// <summary>
	/// Send a method to the interactive session. This may be used to interface with the interactive protocol directly and implement functionality 
	/// that this SDK does not provide out of the box.
	/// </summary>
	int interactive_send_method(interactive_session session, const char* method, const char* paramsJson, bool discardReply, unsigned int* id);

	/// <summary>
	/// Recieve a reply for a method with the specified id. This may be used to interface with the interactive protocol directly and implement functionality
	/// that this SDK does not provide out of the box.
	/// </summary>
	int interactive_receive_reply(interactive_session session, unsigned int id, unsigned int timeoutMs, char* replyJson, size_t* replyJsonLength);
	
	int interactive_capture_transaction(interactive_session session, const char* transactionId);

	// Interactive Group
	int interactive_get_groups(interactive_session session, on_group_enumerate onGroup);
	int interactive_create_group(interactive_session session, const char* groupId, const char* sceneId);
	int interactive_group_set_scene(interactive_session session, const char* groupId, const char* sceneId);

	// Interactive Scene
	int interactive_get_scenes(interactive_session session, on_scene_enumerate onScene);
	int interactive_scene_get_groups(interactive_session session, const char* sceneId, on_group_enumerate onGroup);
	int interactive_scene_get_controls(interactive_session session, const char* sceneId, on_control_enumerate onControl);
	
	// Interactive control
	typedef enum interactive_property_type
	{
		unknown_t,
		int_t,
		bool_t,
		float_t,
		string_t,
		array_t,
		object_t
	} interactive_property_type;

	int interactive_control_trigger_cooldown(interactive_session session, const char* controlId, const unsigned int cooldownMs);

	int interactive_control_get_property_count(interactive_session session, const char* controlId, size_t* count);
	int interactive_control_get_property_data(interactive_session session, const char* controlId, size_t index, char* propName, size_t* propNameLength, interactive_property_type* type);
	int interactive_control_get_meta_property_count(interactive_session session, const char* controlId, size_t* count);
	int interactive_control_get_meta_property_data(interactive_session session, const char* controlId, size_t index, char* propName, size_t* propNameLength, interactive_property_type* type);

	int interactive_control_get_property_int(interactive_session session, const char* controlId, const char* key, int* property);
	int interactive_control_get_property_int64(interactive_session session, const char* controlId, const char* key, long long* property);
	int interactive_control_get_property_bool(interactive_session session, const char* controlId, const char* key, bool* property);
	int interactive_control_get_property_float(interactive_session session, const char* controlId, const char* key, float* property);
	int interactive_control_get_property_string(interactive_session session, const char* controlId, const char* key, char* property, size_t* propertyLength);

	int interactive_control_get_meta_property_int(interactive_session session, const char* controlId, const char* key, int* property);
	int interactive_control_get_meta_property_int64(interactive_session session, const char* controlId, const char* key, long long* property);
	int interactive_control_get_meta_property_bool(interactive_session session, const char* controlId, const char* key, bool* property);
	int interactive_control_get_meta_property_float(interactive_session session, const char* controlId, const char* key, float* property);
	int interactive_control_get_meta_property_string(interactive_session session, const char* controlId, const char* key, char* property, size_t* propertyLength);

	// Interactive participant
	int interactive_get_participants(interactive_session session, on_participant_enumerate onParticipant);
	int interactive_set_participant_group(interactive_session session, const char* participantId, const char* groupId);

	int interactive_get_participant_user_id(interactive_session session, const char* participantId, unsigned int* userId);
	int interactive_get_participant_user_name(interactive_session session, const char* participantId, char* userName, size_t* userNameLength);
	int interactive_get_participant_level(interactive_session session, const char* participantId, unsigned int* level);
	int interactive_get_participant_last_input_at(interactive_session session, const char* participantId, unsigned long long* lastInputAt);
	int interactive_get_participant_connected_at(interactive_session session, const char* participantId, unsigned long long* connectedAt);
	int interactive_get_participant_is_disabled(interactive_session session, const char* participantId, bool* isDisabled);
	int interactive_get_participant_group(interactive_session session, const char* participantId, char* group, size_t* groupLength);

	// Debugging
	enum interactive_debug_level
	{
		debug_none = 0,
		debug_error,
		debug_warning,
		debug_info,
		debug_trace
	};

	typedef void(*on_debug_msg)(const interactive_debug_level dbgMsgType, const char* dbgMsg, size_t dbgMsgSize);

	void interactive_config_debug_level(const interactive_debug_level dbgLevel);
	void interactive_config_debug(const interactive_debug_level dbgLevel, on_debug_msg dbgCallback);
}

}


