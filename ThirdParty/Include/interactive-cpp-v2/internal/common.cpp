#include "common.h"
#include "json.h"
#include "debugging.h"

#include "rapidjson\stringbuffer.h"
#include "rapidjson\writer.h"

#include <codecvt>

namespace mixer_internal
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

// JSON

std::string jsonStringify(rapidjson::Value& value)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	value.Accept(writer);
	return std::string(buffer.GetString(), buffer.GetSize());
}

}