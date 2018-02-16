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

#include "HAL/Platform.h"

namespace MixerBindingUtils
{
	void ExtractCustomEventParamsFromMessage(const class FJsonObject* JsonObject, class UFunction* FunctionPrototype, void* ParamStorage, SIZE_T ParamStorageSize);
	void DestroyCustomEventParams(class UFunction* FunctionPrototype, void* ParamStorage, SIZE_T ParamStorageSize);
}
