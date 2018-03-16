//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// If using interactive-cpp v2, bring in the entire sdk in source form.
// Placed here to isolate it from other UE code (e.g. IWebSocket conflicts)
#if MIXER_BACKEND_INTERACTIVE_CPP_2
#include "PreWindowsApi.h"
#include <interactive-cpp-v2/interactivity.cpp>
#include "PostWindowsApi.h"
#endif