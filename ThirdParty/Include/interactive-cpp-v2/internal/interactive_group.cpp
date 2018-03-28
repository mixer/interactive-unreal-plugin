#include "interactive_session.h"
#include "common.h"

namespace mixer
{

int cache_groups(interactive_session_internal& session)
{
	DEBUG_INFO("Caching groups.");
	unsigned int id;
	RETURN_IF_FAILED(send_method(session, RPC_METHOD_GET_GROUPS, nullptr, false, &id));
	std::shared_ptr<rapidjson::Document> reply;
	RETURN_IF_FAILED(receive_reply(session, id, reply));

	try
	{
		std::unique_lock<std::shared_mutex> l(session.scenesMutex);
		session.scenesByGroup.clear();
		rapidjson::Value& groups = (*reply)[RPC_RESULT][RPC_PARAM_GROUPS];
		for (auto& group : groups.GetArray())
		{
			std::string groupId = group[RPC_GROUP_ID].GetString();
			std::string sceneId;
			if (group.HasMember(RPC_SCENE_ID))
			{
				sceneId = group[RPC_SCENE_ID].GetString();
			}

			session.scenesByGroup.emplace(groupId, sceneId);
		}
	}
	catch (std::exception e)
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	return MIXER_OK;
}

int interactive_get_groups(interactive_session session, on_group_enumerate onGroup)
{
	if (nullptr == session || nullptr == onGroup)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	// Lock the scene cache
	std::shared_lock<std::shared_mutex> l(sessionInternal->scenesMutex);
	for (auto& sceneByGroup : sessionInternal->scenesByGroup)
	{
		interactive_group group;
		group.id = sceneByGroup.first.c_str();
		group.idLength = sceneByGroup.first.length();
		group.sceneId = sceneByGroup.second.c_str();
		group.sceneIdLength = sceneByGroup.second.length();
		onGroup(sessionInternal->callerContext, sessionInternal, &group);
	}

	return MIXER_OK;
}

int interactive_create_group(interactive_session session, const char* groupId, const char* sceneId)
{
	if (nullptr == session || nullptr == groupId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	unsigned int packetId;
	std::string sceneIdStr = RPC_SCENE_DEFAULT;
	if (nullptr != sceneId)
	{
		sceneIdStr = sceneId;
	}
	RETURN_IF_FAILED(send_method(*sessionInternal, RPC_METHOD_CREATE_GROUPS, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		rapidjson::Value groups(rapidjson::kArrayType);
		rapidjson::Value group(rapidjson::kObjectType);
		group.AddMember(RPC_GROUP_ID, rapidjson::StringRef(groupId), allocator);
		group.AddMember(RPC_SCENE_ID, rapidjson::StringRef(sceneIdStr.c_str(), sceneIdStr.length()), allocator);
		groups.PushBack(group, allocator);
		params.AddMember(RPC_PARAM_GROUPS, groups, allocator);
	}, false, &packetId));

	// Receive a reply to ensure that creation was successful.
	std::shared_ptr<rapidjson::Document> replyDoc;
	RETURN_IF_FAILED(receive_reply(*sessionInternal, packetId, replyDoc));

	return MIXER_OK;
}

int interactive_group_set_scene(interactive_session session, const char* groupId, const char* sceneId)
{
	if (nullptr == session || nullptr == groupId || nullptr == sceneId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	unsigned int packetId;
	RETURN_IF_FAILED(send_method(*sessionInternal, RPC_METHOD_UPDATE_GROUPS, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		rapidjson::Value groups(rapidjson::kArrayType);
		rapidjson::Value group(rapidjson::kObjectType);
		group.AddMember(RPC_GROUP_ID, rapidjson::StringRef(groupId), allocator);
		group.AddMember(RPC_SCENE_ID, rapidjson::StringRef(sceneId), allocator);
		groups.PushBack(group, allocator);
		params.AddMember(RPC_PARAM_GROUPS, groups, allocator);
	}, false, &packetId));

	// Receive a reply to ensure that creation was successful.
	std::shared_ptr<rapidjson::Document> replyDoc;
	RETURN_IF_FAILED(receive_reply(*sessionInternal, packetId, replyDoc));

	return MIXER_OK;
}
}