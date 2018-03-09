//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "MixerInteractivityModule_Null.h"

#if MIXER_BACKEND_NULL
IMPLEMENT_MODULE(FMixerInteractivityModule_Null, MixerInteractivity);
#endif

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
int32 MixerNullLinkerHelper;

