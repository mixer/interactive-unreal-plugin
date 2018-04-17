#include "interactive_session.h"
#include "common.h"

namespace mixer_internal
{

int cache_scenes(interactive_session_internal& session)
{
	DEBUG_INFO("Caching scenes.");
	unsigned int id;
	RETURN_IF_FAILED(send_method(session, RPC_METHOD_GET_SCENES, nullptr, false, &id));
	std::shared_ptr<rapidjson::Document> reply;
	RETURN_IF_FAILED(receive_reply(session, id, reply));

	// Get the scenes array from the result and set up pointers to scenes and controls.
	std::unique_lock<std::shared_mutex> l(session.scenesMutex);
	session.controls.clear();
	session.scenes.clear();
	session.scenesRoot.RemoveAllMembers();

	// Copy just the scenes array portion of the reply into the cached scenes root.
	rapidjson::Value scenesArray(rapidjson::kArrayType);
	rapidjson::Value replyScenesArray = (*reply)[RPC_RESULT][RPC_PARAM_SCENES].GetArray();
	scenesArray.CopyFrom(replyScenesArray, session.scenesRoot.GetAllocator());
	session.scenesRoot.AddMember(RPC_PARAM_SCENES, scenesArray, session.scenesRoot.GetAllocator());

	// Iterate through each scene and set up a pointer to each control.
	int sceneIndex = 0;
	for (auto& scene : session.scenesRoot[RPC_PARAM_SCENES].GetArray())
	{
		std::string scenePointer = "/" + std::string(RPC_PARAM_SCENES) + "/" + std::to_string(sceneIndex++);
		auto controlsArray = scene.FindMember(RPC_PARAM_CONTROLS);
		if (controlsArray != scene.MemberEnd() && controlsArray->value.IsArray())
		{
			int controlIndex = 0;
			for (auto& control : controlsArray->value.GetArray())
			{
				session.controls.emplace(control[RPC_CONTROL_ID].GetString(), scenePointer + "/" + std::string(RPC_PARAM_CONTROLS) + "/" + std::to_string(controlIndex++));
			}
		}

		session.scenes.emplace(scene[RPC_SCENE_ID].GetString(), scenePointer);
	}

	return MIXER_OK;
}

}

int interactive_get_scenes(interactive_session session, on_scene_enumerate onScene)
{
	if (nullptr == session || nullptr == onScene)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	// Lock the scenes while they are being enumerated.
	std::shared_lock<std::shared_mutex> l(sessionInternal->scenesMutex);
	for (auto sceneItr = sessionInternal->scenes.begin(); sessionInternal->scenes.end() != sceneItr; ++sceneItr)
	{
		std::string sceneId = sceneItr->first;

		// Construct an interactive_scene object to pass to the caller.
		interactive_scene scene;
		scene.id = sceneId.c_str();
		scene.idLength = sceneId.length();
		onScene(sessionInternal->callerContext, sessionInternal, &scene);
	}

	return MIXER_OK;
}

int interactive_scene_get_groups(interactive_session session, const char* sceneId, on_group_enumerate onGroup)
{
	if (nullptr == session || nullptr == onGroup)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	// Lock the scenes while they are being enumerated.
	std::shared_lock<std::shared_mutex> l(sessionInternal->scenesMutex);
	auto sceneItr = sessionInternal->scenes.find(sceneId);
	if (sessionInternal->scenes.end() == sceneItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	// Find the cached scene and enumerate all controls.
	rapidjson::Value* sceneVal = rapidjson::Pointer(sceneItr->second.c_str()).Get(sessionInternal->scenesRoot);
	if (sceneVal->HasMember(RPC_PARAM_GROUPS) && (*sceneVal)[RPC_PARAM_GROUPS].IsArray() && !(*sceneVal)[RPC_PARAM_GROUPS].Empty())
	{
		for (auto& groupObj : (*sceneVal)[RPC_PARAM_GROUPS].GetArray())
		{
			interactive_group group;
			group.id = groupObj[RPC_GROUP_ID].GetString();
			group.idLength = groupObj[RPC_GROUP_ID].GetStringLength();
			group.sceneId = sceneId;
			group.sceneIdLength = strlen(sceneId);
			onGroup(sessionInternal->callerContext, sessionInternal, &group);
		}
	}

	return MIXER_OK;
}

int interactive_scene_get_controls(interactive_session session, const char* sceneId, on_control_enumerate onControl)
{
	if (nullptr == session || nullptr == onControl)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	// Lock the scenes while they are being enumerated.
	std::shared_lock<std::shared_mutex> l(sessionInternal->scenesMutex);
	auto sceneItr = sessionInternal->scenes.find(sceneId);
	if (sessionInternal->scenes.end() == sceneItr)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	// Find the cached scene and enumerate all controls.
	rapidjson::Value* sceneVal = rapidjson::Pointer(sceneItr->second.c_str()).Get(sessionInternal->scenesRoot);
	if (sceneVal->HasMember(RPC_PARAM_CONTROLS) && (*sceneVal)[RPC_PARAM_CONTROLS].IsArray() && !(*sceneVal)[RPC_PARAM_CONTROLS].Empty())
	{
		for (auto& controlObj : (*sceneVal)[RPC_PARAM_CONTROLS].GetArray())
		{
			interactive_control control;
			control.id = controlObj[RPC_CONTROL_ID].GetString();
			control.idLength = controlObj[RPC_CONTROL_ID].GetStringLength();
			control.kind = controlObj[RPC_CONTROL_KIND].GetString();
			control.kindLength = controlObj[RPC_CONTROL_KIND].GetStringLength();
			onControl(sessionInternal->callerContext, sessionInternal, &control);
		}
	}

	return MIXER_OK;
}