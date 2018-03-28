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

	enum interactive_input_type
	{
		input_type_button,
		input_type_coordinate,
		input_type_custom
	};

	enum button_action
	{
		up,
		down
	};

	struct interactive_input
	{
		interactive_control control;
		interactive_input_type type;
		const char* participantId;
		size_t participantIdLength;
		const char* jsonData;
		size_t jsonDataLength;
		const char* transactionId;
		size_t transactionIdLength;
		union
		{
			struct buttonData
			{
				button_action action;
			} buttonData;
			struct coordinateData
			{
				float x;
				float y;
			} coordinateData;
		};
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
	typedef void(*on_input)(void* context, interactive_session session, const interactive_input* input);
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
	/// <summary>
	/// Get a short code that can be used to obtain an OAuth token at <c>https://www.mixer.com/go?code=<shortCode></c>.
	/// Note: Blocking function that waits on network IO.
	/// </summary>
	int interactive_auth_get_short_code(const char* clientId, char* shortCode, size_t* shortCodeLength, char* shortCodeHandle, size_t* shortCodeHandleLength);

	/// <summary>
	/// Wait for a <c>shortCode</c> to be authorized or rejected after presenting the OAuth short code web page. The resulting <c>refreshToken</c>
	/// should be securely serialized and linked to the current user.
	/// Note: Blocking function that waits on network IO.
	/// </summary>
	int interactive_auth_wait_short_code(const char* clientId, const char* shortCodeHandle, char* refreshToken, size_t* refreshTokenLength);

	/// <summary>
	/// Determine if a <c>refreshToken</c> returned by <c>interactive_auth_wait_short_code</c> is stale. A token is stale if it has exceeded its half-life.
	/// </summary>
	int interactive_auth_is_token_stale(const char* token, bool* isStale);

	/// <summary>
	/// Refresh a stale <c>refreshToken<c>.
	/// Note: Blocking function that waits on network IO.
	/// </summary>
	int interactive_auth_refresh_token(const char* clientId, const char* staleToken, char* refreshToken, size_t* refreshTokenLength);

	/// <summary>
	/// Parse a <c>refreshToken</c> to get the authorization header that should be passed to <c>interactive_connect()</c>.
	/// </summary>
	int interactive_auth_parse_refresh_token(const char* token, char* authorization, size_t* authorizationLength);

	/// <summary>
	/// Connect to an interactive session.
	/// The <c>auth</c> parameter is passed as the Authorization header to the service. This should either be a OAuth Bearer token or an XToken.
	/// The <c>versionId</c> parameter is the id of the interative project that should be started.
	/// The <c>shareCode</c> parameter is an optional parameter that is used when starting an interactive project that the user does not have implicit access to. This is usually required unless a project has been published.
	/// The <c>manualStart</c> parameter specifies whether you intend to set the ready state manually after calling this function. Setting this to true will connect to the interactive service but remain in the `not ready` state.
	/// Note: Blocking function that waits on network IO.
	/// </summary>
	int interactive_connect(const char* auth, const char* versionId, const char* shareCode, bool manualStart, interactive_session* sessionPtr);

	/// <summary>
	/// Disconnect from an interactive session and destroy it. This must not be called from inside an event handler as the lifetime of registered event handlers is assumed to outlive 
	/// the session. Only call this when there is no thread processing events via <c>interactive_run</c>.
	/// Note: Blocking function that waits on network IO.
	/// </summary>
	void interactive_disconnect(interactive_session session);

	/// <summary>
	/// Get the current <c>interactive_state</c> for the specified session.
	/// </summary>
	int interactive_get_state(interactive_session session, interactive_state* state);

	/// <summary>
	/// Set the ready state for specified session. No participants will be able to see interactive scenes or give input
	/// until the interactive session is ready.
	/// Note: Blocking function that waits on network IO.
	/// </summary>
	int interactive_set_ready(interactive_session session, bool isReady);

	/// <summary>
	/// Set a session context that will be passed to every event callback.
	/// </summary>
	int interactive_set_session_context(interactive_session session, void* context);

	// Event handlers for interactive events that will be called by the <c>interactive_run</c> thread.
	int interactive_reg_error_handler(interactive_session session, on_error onError);
	int interactive_reg_state_changed_handler(interactive_session session, on_state_changed onStateChanged);
	int interactive_reg_input_handler(interactive_session session, on_input onInput);
	int interactive_reg_participants_changed_handler(interactive_session session, on_participants_changed onParticipantsChanged);
	int interactive_reg_unhandled_method_handler(interactive_session session, on_unhandled_method onUnhandledMethod);

	/// <summary>
	/// This function processes the specified number of events from the interactive service and calls back to the handlers registered above.
	/// This should be called often, at least once per frame, so that interactive input is processed in a timely manner.
	/// </summary>
	int interactive_run(interactive_session session, unsigned int maxEventsToProcess);

	/// <summary>
	/// Send a method to the interactive session. This may be used to interface with the interactive protocol directly and implement functionality 
	/// that this SDK does not provide out of the box.
	/// Note: Blocking function that waits on network IO.
	/// </summary>
	int interactive_send_method(interactive_session session, const char* method, const char* paramsJson, bool discardReply, unsigned int* id);

	/// <summary>
	/// Recieve a reply for a method with the specified id. This may be used to interface with the interactive protocol directly and implement functionality
	/// that this SDK does not provide out of the box.
	/// </summary>
	int interactive_receive_reply(interactive_session session, unsigned int id, unsigned int timeoutMs, char* replyJson, size_t* replyJsonLength);
	
	/// <summary>
	/// Capture a transaction to charge a participant the input's spark cost. This should be called before
	/// taking further action on input as the participant may not have enough sparks or the transaction may have expired.
	/// Note: Blocking function that waits on network IO.
	/// </summary>
	int interactive_capture_transaction(interactive_session session, const char* transactionId);

	// Interactive Group
	/// <summary>
	/// Get the interactive groups for the specified session.
	/// </summary>
	int interactive_get_groups(interactive_session session, on_group_enumerate onGroup);

	/// <summary>
	/// Create a new group for the specified session.
	/// </summary>
	int interactive_create_group(interactive_session session, const char* groupId, const char* sceneId);

	/// <summary>
	/// Set a group's scene for the specified session. Use this and <c>interative_set_participant_group</c> to manage which scenes participants see.
	/// </summary>
	int interactive_group_set_scene(interactive_session session, const char* groupId, const char* sceneId);

	// Interactive Scene
	/// <summary>
	/// Get all scenes for the specified session.
	/// </summary>
	int interactive_get_scenes(interactive_session session, on_scene_enumerate onScene);

	/// <summary>
	/// Get each group that this scene belongs to for the specified session.
	/// </summary>
	int interactive_scene_get_groups(interactive_session session, const char* sceneId, on_group_enumerate onGroup);

	/// <summary>
	/// Get a scene's controls for the specified session.
	/// Note: Blocking function that waits on network IO.
	/// </summary>
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

	/// <summary>
	/// Trigger a cooldown on a control for the specified number of milliseconds.
	/// Note: Blocking function that waits on network IO.
	/// </summary>
	int interactive_control_trigger_cooldown(interactive_session session, const char* controlId, const unsigned long long cooldownMs);

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
	/// <summary>
	/// Get all participants for the specified session.
	/// </summary>
	int interactive_get_participants(interactive_session session, on_participant_enumerate onParticipant);

	/// <summary>
	/// Change the group that the specified participant belongs to. Use this along with <c>interactive_group_set_scene</c> to configure which scenes participants see.
	/// Note: Blocking function that waits on network IO.
	/// </summary>
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

	/// <summary>
	/// Configure the debug verbosity for all interactive sessions in the current process.
	/// </summary>
	void interactive_config_debug_level(const interactive_debug_level dbgLevel);

	/// <summary>
	/// Configure the debug verbosity and set the debug callback function for all interactive sessions in the current process.
	/// </summary>
	void interactive_config_debug(const interactive_debug_level dbgLevel, on_debug_msg dbgCallback);
}

}


