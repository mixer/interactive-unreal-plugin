//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#if !PLATFORM_XBOXONE
#include "Windows/WindowsHWrapper.h"
#endif

// If using interactive-cpp v2, bring in the entire sdk in source form.
// Placed here to isolate it from other UE code (e.g. IWebSocket conflicts)
#if MIXER_BACKEND_INTERACTIVE_CPP_2
#if PLATFORM_WINDOWS
#include "Windows/PreWindowsApi.h"
#elif PLATFORM_XBOXONE
#include "XboxOnePreApi.h"
#endif
#include <interactive-cpp-v2/interactivity.cpp>
#if PLATFORM_WINDOWS
#include "Windows/PostWindowsApi.h"
#elif PLATFORM_XBOXONE
#include "XboxOnePostApi.h"
#endif
#endif

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
int MixerV2SdkLinkerHelper;