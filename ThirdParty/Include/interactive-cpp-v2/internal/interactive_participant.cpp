#include "interactive_session.h"
#include "common.h"

namespace mixer
{


void parse_participant(rapidjson::Value& participantJson, interactive_participant& participant)
{
	participant.id = participantJson[RPC_SESSION_ID].GetString();
	participant.idLength = participantJson[RPC_SESSION_ID].GetStringLength();
	participant.userId = participantJson[RPC_USER_ID].GetUint();
	participant.userName = participantJson[RPC_USERNAME].GetString();
	participant.usernameLength = participantJson[RPC_USERNAME].GetStringLength();
	participant.level = participantJson[RPC_LEVEL].GetUint();
	participant.lastInputAtMs = participantJson[RPC_PART_LAST_INPUT].GetUint64();
	participant.connectedAtMs = participantJson[RPC_PART_CONNECTED].GetUint64();
	participant.disabled = participantJson[RPC_DISABLED].GetBool();
	participant.groupId = participantJson[RPC_GROUP_ID].GetString();
	participant.groupIdLength = participantJson[RPC_GROUP_ID].GetStringLength();
}

int interactive_get_participants(interactive_session session, on_participant_enumerate onParticipant)
{
	if (nullptr == session || nullptr == onParticipant)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	for (auto& participantById : sessionInternal->participants)
	{
		interactive_participant participant;
		parse_participant(*participantById.second, participant);
		onParticipant(sessionInternal->callerContext, sessionInternal, &participant);
	}

	return MIXER_OK;
}

int interactive_set_participant_group(interactive_session session, const char* participantId, const char* groupId)
{
	if (nullptr == session || nullptr == participantId || nullptr == groupId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	unsigned int packetId;
	RETURN_IF_FAILED(send_method(*sessionInternal, RPC_METHOD_UPDATE_PARTICIPANTS, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		rapidjson::Value participants(rapidjson::kArrayType);
		rapidjson::Value participant(rapidjson::kObjectType);
		participant.AddMember(RPC_SESSION_ID, rapidjson::StringRef(participantId), allocator);
		participant.AddMember(RPC_GROUP_ID, rapidjson::StringRef(groupId), allocator);
		participants.PushBack(participant, allocator);
		params.AddMember(RPC_PARAM_PARTICIPANTS, participants, allocator);
		params.AddMember("priority", 0, allocator);
	}, false, &packetId));

	// Receive a reply to ensure that creation was successful.
	std::shared_ptr<rapidjson::Document> replyDoc;
	RETURN_IF_FAILED(receive_reply(*sessionInternal, packetId, replyDoc));

	return MIXER_OK;
}

int interactive_get_participant_user_id(interactive_session session, const char* participantId, unsigned int* userId)
{
	if (nullptr == session || nullptr == participantId || nullptr == userId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	*userId = (*participantDoc)[RPC_USER_ID].GetUint();
	return MIXER_OK;
}

int interactive_get_participant_user_name(interactive_session session, const char* participantId, char* userName, size_t* userNameLength)
{
	if (nullptr == session || nullptr == participantId || nullptr == userNameLength)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;

	size_t actualLength = (*participantDoc)[RPC_USERNAME].GetStringLength();
	if (nullptr == userName || *userNameLength < actualLength + 1)
	{
		*userNameLength = actualLength + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(userName, (*participantDoc)[RPC_USERNAME].GetString(), actualLength);
	userName[actualLength] = 0;
	*userNameLength = actualLength + 1;
	return MIXER_OK;
}

int interactive_get_participant_level(interactive_session session, const char* participantId, unsigned int* level)
{
	if (nullptr == session || nullptr == participantId || nullptr == level)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	*level = (*participantDoc)[RPC_LEVEL].GetUint();
	return MIXER_OK;
}

int interactive_get_participant_last_input_at(interactive_session session, const char* participantId, unsigned long long* lastInputAt)
{
	if (nullptr == session || nullptr == participantId || nullptr == lastInputAt)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	*lastInputAt = (*participantDoc)[RPC_PART_LAST_INPUT].GetUint64();
	return MIXER_OK;
}

int interactive_get_participant_connected_at(interactive_session session, const char* participantId, unsigned long long* connectedAt)
{
	if (nullptr == session || nullptr == participantId || nullptr == connectedAt)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	*connectedAt = (*participantDoc)[RPC_PART_CONNECTED].GetUint64();
	return MIXER_OK;
}

int interactive_get_participant_is_disabled(interactive_session session, const char* participantId, bool* isDisabled)
{
	if (nullptr == session || nullptr == participantId || nullptr == isDisabled)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	*isDisabled = (*participantDoc)[RPC_DISABLED].GetBool();
	return MIXER_OK;
}

int interactive_get_participant_group(interactive_session session, const char* participantId, char* group, size_t* groupLength)
{
	if (nullptr == session || nullptr == participantId || nullptr == groupLength)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	auto participantItr = sessionInternal->participants.find(std::string(participantId));
	if (sessionInternal->participants.end() == participantItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::shared_ptr<rapidjson::Document> participantDoc = participantItr->second;
	size_t actualLength = (*participantDoc)[RPC_GROUP_ID].GetStringLength();
	if (nullptr == group || *groupLength < actualLength + 1)
	{
		*groupLength = actualLength + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(group, (*participantDoc)[RPC_GROUP_ID].GetString(), actualLength);
	group[actualLength] = 0;
	*groupLength = actualLength + 1;
	return MIXER_OK;
}


}