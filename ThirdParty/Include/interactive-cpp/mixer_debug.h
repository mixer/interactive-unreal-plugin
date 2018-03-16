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
#include "../../Include/namespaces.h"
#include "../../Include/interactivity.h"

NAMESPACE_MICROSOFT_MIXER_BEGIN

typedef enum mixer_debug_level
{
	none = 0,
	error,
	warning,
	info,
	trace
} mixer_debug_level;

typedef void(*on_debug_msg)(const mixer_debug_level dbgMsgType, const string_t& dbgMsg);

void mixer_config_debug_level(const mixer_debug_level dbgLevel);
void mixer_config_debug(const mixer_debug_level dbgLevel, on_debug_msg dbgCallback);

void mixer_debug(const mixer_debug_level level, const string_t& message);
string_t convertStr(const char*);

#define DEBUG_ERROR(x) mixer_debug(mixer_debug_level::error, x)
#define DEBUG_EXCEPTION(x, y) mixer_debug(mixer_debug_level::error, x + convertStr(y))
#define DEBUG_WARNING(x) mixer_debug(mixer_debug_level::warning, x)
#define DEBUG_INFO(x) mixer_debug(mixer_debug_level::info, x)
#define DEBUG_TRACE(x) mixer_debug(mixer_debug_level::trace, x)

NAMESPACE_MICROSOFT_MIXER_END
