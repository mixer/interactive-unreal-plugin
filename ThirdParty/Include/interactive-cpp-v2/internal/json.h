#pragma once

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson\document.h"

namespace mixer_internal
{

std::string jsonStringify(rapidjson::Value& doc);

}