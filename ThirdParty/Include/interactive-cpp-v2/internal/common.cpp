#include "common.h"

#include "rapidjson\stringbuffer.h"
#include "rapidjson\writer.h"

namespace mixer
{

// String conversion functions
std::wstring utf8_to_wstring(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	return conv.from_bytes(str);
}

std::string wstring_to_utf8(const std::wstring& wstr)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	return conv.to_bytes(wstr);
}

std::string jsonStringify(rapidjson::Value& value)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	value.Accept(writer);
	return std::string(buffer.GetString(), buffer.GetSize());
}

// Debugging

void interactive_config_debug_level(const interactive_debug_level dbgLevel)
{
	g_dbgLevel = dbgLevel;
}

void interactive_config_debug(const interactive_debug_level dbgLevel, const on_debug_msg dbgCallback)
{
	g_dbgLevel = dbgLevel;
	g_dbgCallback = dbgCallback;
}

}