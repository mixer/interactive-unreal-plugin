#pragma once
#include "interactivity.h"
#include <codecvt>

#include "rapidjson\document.h"

namespace mixer
{

#define RETURN_IF_FAILED(x) do { int __err_c = x; if(0 != __err_c) return __err_c; } while(0)

// String conversion functions
std::wstring utf8_to_wstring(const std::string& str);
std::string wstring_to_utf8(const std::wstring& wstr);

std::string jsonStringify(rapidjson::Document& doc);

// Debuggig
static on_debug_msg g_dbgCallback = nullptr;
static interactive_debug_level g_dbgLevel = interactive_debug_level::debug_none;

#define _DEBUG_IF_LEVEL(x, y) do { if (y <= g_dbgLevel && nullptr != g_dbgCallback) { g_dbgCallback(y, std::string(x).c_str(), std::string(x).length()); } } while (0)
#define DEBUG_ERROR(x) _DEBUG_IF_LEVEL(x, interactive_debug_level::debug_error)
#define DEBUG_WARNING(x) _DEBUG_IF_LEVEL(x, interactive_debug_level::debug_warning)
#define DEBUG_INFO(x) _DEBUG_IF_LEVEL(x, interactive_debug_level::debug_info)
#define DEBUG_TRACE(x) _DEBUG_IF_LEVEL(x, interactive_debug_level::debug_trace)

}