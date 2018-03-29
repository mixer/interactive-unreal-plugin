#include "interactive_session.h"
#include "common.h"

namespace mixer
{

int get_scene_object_prop_count(interactive_session_internal& session, const char* pointer, size_t* count)
{
	if (nullptr == pointer || nullptr == count)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*count = 0;
	try
	{
		// Find the control in the cached scene document.
		rapidjson::Value* value = rapidjson::Pointer(rapidjson::StringRef(pointer)).Get(session.scenesRoot);
		if (nullptr == value)
		{
			return MIXER_ERROR_OBJECT_NOT_FOUND;
		}

		*count = value->MemberCount();
	}
	catch (std::exception e)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	return MIXER_OK;
}

int get_scene_object_prop_data(interactive_session_internal& session, const char* pointer, size_t index, char* propName, size_t* propNameLength, interactive_property_type* propType)
{
	if (nullptr == pointer || nullptr == propNameLength || nullptr == propType)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	if (*propNameLength > 0 && nullptr == propName)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*propType = interactive_property_type::unknown_t;

	// Find the object in the cached scene document.
	rapidjson::Value* value = rapidjson::Pointer(rapidjson::StringRef(pointer)).Get(session.scenesRoot);
	if (nullptr == value)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	// Verify that this index is valid for this object.
	if (value->MemberCount() <= index)
	{
		return MIXER_ERROR_PROPERTY_NOT_FOUND;
	}

	// Move iterator to property index.
	auto itr = value->MemberBegin();
	for (size_t i = 0; i < index; ++i)
	{
		++itr;
	}

	// Verify the caller's buffer is large enough to hold the contents.
	if (*propNameLength < itr->name.GetStringLength())
	{
		*propNameLength = itr->name.GetStringLength() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(propName, itr->name.GetString(), *propNameLength - 1);
	propName[*propNameLength - 1] = '\0';

	// Determine the type of this property.
	if (itr->value.IsString())
	{
		*propType = interactive_property_type::string_t;
	}
	else if (itr->value.IsInt())
	{
		*propType = interactive_property_type::int_t;
	}
	else if (itr->value.IsBool())
	{
		*propType = interactive_property_type::bool_t;
	}
	else if (itr->value.IsFloat())
	{
		*propType = interactive_property_type::float_t;
	}
	else if (itr->value.IsArray())
	{
		*propType = interactive_property_type::array_t;
	}
	else if (itr->value.IsObject())
	{
		*propType = interactive_property_type::object_t;
	}

	return MIXER_OK;
}

int interactive_control_get_property_count(interactive_session session, const char* controlId, size_t* count)
{
	if (nullptr == session || nullptr == controlId || nullptr == count)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*count = 0;
	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	auto controlItr = sessionInternal->controls.find(std::string(controlId));
	if (controlItr == sessionInternal->controls.end())
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	return get_scene_object_prop_count(*sessionInternal, controlItr->second.c_str(), count);
}

int interactive_control_get_meta_property_count(interactive_session session, const char* controlId, size_t* count)
{
	if (nullptr == session || nullptr == controlId || nullptr == count)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*count = 0;
	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	auto controlItr = sessionInternal->controls.find(std::string(controlId));
	if (controlItr == sessionInternal->controls.end())
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::string metaPropPointer = controlItr->second + "/" + RPC_METADATA;
	return get_scene_object_prop_count(*sessionInternal, metaPropPointer.c_str(), count);
}

int interactive_control_get_property_data(interactive_session session, const char* controlId, size_t index, char* propName, size_t* propNameLength, interactive_property_type* propType)
{
	if (nullptr == session || nullptr == controlId || nullptr == propNameLength || nullptr == propType)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*propType = interactive_property_type::unknown_t;
	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	auto controlItr = sessionInternal->controls.find(std::string(controlId));
	if (controlItr == sessionInternal->controls.end())
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	const std::string& controlPointer = controlItr->second;
	return get_scene_object_prop_data(*sessionInternal, controlPointer.c_str(), index, propName, propNameLength, propType);
}

int interactive_control_get_meta_property_data(interactive_session session, const char* controlId, size_t index, char* propName, size_t* propNameLength, interactive_property_type* propType)
{
	if (nullptr == session || nullptr == controlId || nullptr == propNameLength || nullptr == propType)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	*propType = interactive_property_type::unknown_t;
	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);

	auto controlItr = sessionInternal->controls.find(std::string(controlId));
	if (controlItr == sessionInternal->controls.end())
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::string controlPointer = controlItr->second + "/" + RPC_METADATA;
	interactive_property_type valueType = interactive_property_type::unknown_t;
	int err = get_scene_object_prop_data(*sessionInternal, controlPointer.c_str(), index, propName, propNameLength, &valueType);
	if (MIXER_OK == err)
	{
		// Metadata properties are stored in a sub-"value" for some reason.
		controlPointer = controlPointer + "/" + propName;
		char value[6]; // To store "value"
		size_t valueLength = sizeof(value);
		return get_scene_object_prop_data(*sessionInternal, controlPointer.c_str(), 0, value, &valueLength, propType);
	}

	return err;
}

int verify_get_property_args_and_get_control_value(interactive_session session, const char* controlId, const char* key, void* property, rapidjson::Value** controlValue)
{
	if (nullptr == session || nullptr == property || nullptr == controlValue)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	interactive_session_internal* sessionInternal = reinterpret_cast<interactive_session_internal*>(session);
	if (sessionInternal->shutdownRequested)
	{
		return MIXER_OK;
	}

	std::shared_lock<std::shared_mutex> lock(sessionInternal->scenesMutex);
	auto controlItr = sessionInternal->controls.find(std::string(controlId));
	if (controlItr == sessionInternal->controls.end())
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	std::string controlPointer = controlItr->second + "/" + std::string(key);
	try
	{	
		*controlValue = rapidjson::Pointer(controlPointer.c_str()).Get(sessionInternal->scenesRoot);
		if (nullptr == *controlValue)
		{
			return MIXER_ERROR_PROPERTY_NOT_FOUND;
		}
	}
	catch (std::exception e)
	{
		return MIXER_ERROR_OBJECT_NOT_FOUND;
	}

	return MIXER_OK;
}

int interactive_control_get_property_int(interactive_session session, const char* controlId, const char* key, int* property)
{
	rapidjson::Value* controlValue;
	RETURN_IF_FAILED(verify_get_property_args_and_get_control_value(session, controlId, key, property, &controlValue));
	if (!controlValue->IsInt())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*property = controlValue->GetInt();
	return MIXER_OK;
}

int interactive_control_get_property_int64(interactive_session session, const char* controlId, const char* key, long long* property)
{
	rapidjson::Value* controlValue;
	RETURN_IF_FAILED(verify_get_property_args_and_get_control_value(session, controlId, key, property, &controlValue));
	if (!controlValue->IsInt64())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*property = controlValue->GetInt64();
	return MIXER_OK;
}

int interactive_control_get_property_bool(interactive_session session, const char* controlId, const char* key, bool* property)
{
	rapidjson::Value* controlValue;
	RETURN_IF_FAILED(verify_get_property_args_and_get_control_value(session, controlId, key, property, &controlValue));
	if (!controlValue->IsBool())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*property = controlValue->GetBool();
	return MIXER_OK;
}

int interactive_control_get_property_float(interactive_session session, const char* controlId, const char* key, float* property)
{
	rapidjson::Value* controlValue;
	RETURN_IF_FAILED(verify_get_property_args_and_get_control_value(session, controlId, key, property, &controlValue));
	if (!controlValue->IsFloat())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*property = controlValue->GetFloat();
	return MIXER_OK;
}

int interactive_control_get_property_string(interactive_session session, const char* controlId, const char* key, char* property, size_t* propertyLength)
{
	rapidjson::Value* controlValue;
	RETURN_IF_FAILED(verify_get_property_args_and_get_control_value(session, controlId, key, propertyLength, &controlValue));
	if (!controlValue->IsString())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	if (*propertyLength > 0 && nullptr == property)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	if (!controlValue->IsString())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	if (*propertyLength < controlValue->GetStringLength() + 1)
	{
		*propertyLength = controlValue->GetStringLength() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	if (0 != controlValue->GetStringLength())
	{
		memcpy(property, controlValue->GetString(), *propertyLength - 1);
	}

	property[*propertyLength - 1] = '\0';

	return MIXER_OK;
}

int interactive_control_get_meta_property_int(interactive_session session, const char* controlId, const char* key, int* property)
{
	rapidjson::Value* controlValue;
	std::string metaKey = std::string(RPC_METADATA) + "/" + std::string(key) + "/value";
	RETURN_IF_FAILED(verify_get_property_args_and_get_control_value(session, controlId, metaKey.c_str(), property, &controlValue));
	if (!controlValue->IsInt())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*property = controlValue->GetInt();
	return MIXER_OK;
}

int interactive_control_get_meta_property_int64(interactive_session session, const char* controlId, const char* key, long long* property)
{
	rapidjson::Value* controlValue;
	std::string metaKey = std::string(RPC_METADATA) + "/" + std::string(key) + "/value";
	RETURN_IF_FAILED(verify_get_property_args_and_get_control_value(session, controlId, metaKey.c_str(), property, &controlValue));
	if (!controlValue->IsInt64())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*property = controlValue->GetInt64();
	return MIXER_OK;
}

int interactive_control_get_meta_property_bool(interactive_session session, const char* controlId, const char* key, bool* property)
{
	rapidjson::Value* controlValue;
	std::string metaKey = std::string(RPC_METADATA) + "/" + std::string(key) + "/value";
	RETURN_IF_FAILED(verify_get_property_args_and_get_control_value(session, controlId, metaKey.c_str(), property, &controlValue));
	if (!controlValue->IsBool())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*property = controlValue->GetBool();
	return MIXER_OK;
}

int interactive_control_get_meta_property_float(interactive_session session, const char* controlId, const char* key, float* property)
{
	rapidjson::Value* controlValue;
	std::string metaKey = std::string(RPC_METADATA) + "/" + std::string(key) + "/value";
	RETURN_IF_FAILED(verify_get_property_args_and_get_control_value(session, controlId, metaKey.c_str(), property, &controlValue));
	if (!controlValue->IsFloat())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	*property = controlValue->GetFloat();
	return MIXER_OK;
}

int interactive_control_get_meta_property_string(interactive_session session, const char* controlId, const char* key, char* property, size_t* propertyLength)
{
	rapidjson::Value* controlValue;
	std::string metaKey = std::string(RPC_METADATA) + "/" + std::string(key) + "/value";
	RETURN_IF_FAILED(verify_get_property_args_and_get_control_value(session, controlId, metaKey.c_str(), propertyLength, &controlValue));
	if (!controlValue->IsString())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	if (*propertyLength > 0 && nullptr == property)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	if (!controlValue->IsString())
	{
		return MIXER_ERROR_INVALID_PROPERTY_TYPE;
	}

	if (nullptr == property || *propertyLength < controlValue->GetStringLength() + 1)
	{
		*propertyLength = controlValue->GetStringLength() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	if (0 != controlValue->GetStringLength())
	{
		memcpy(property, controlValue->GetString(), *propertyLength - 1);
	}

	property[*propertyLength - 1] = '\0';

	return MIXER_OK;
}
}