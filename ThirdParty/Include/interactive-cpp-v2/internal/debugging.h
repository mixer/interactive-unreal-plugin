#pragma once
#include "interactivity.h"

namespace mixer_internal
{

static on_debug_msg g_dbgInteractiveCallback = nullptr;
static interactive_debug_level g_dbgInteractiveLevel = interactive_debug_none;

#define _DEBUG_IF_LEVEL(x, y) do { if (y <= g_dbgInteractiveLevel && nullptr != g_dbgInteractiveCallback) { g_dbgInteractiveCallback(y, std::string(x).c_str(), std::string(x).length()); } } while (0)
#define DEBUG_ERROR(x) _DEBUG_IF_LEVEL(x, interactive_debug_error)
#define DEBUG_WARNING(x) _DEBUG_IF_LEVEL(x, interactive_debug_warning)
#define DEBUG_INFO(x) _DEBUG_IF_LEVEL(x, interactive_debug_info)
#define DEBUG_TRACE(x) _DEBUG_IF_LEVEL(x, interactive_debug_trace)

}