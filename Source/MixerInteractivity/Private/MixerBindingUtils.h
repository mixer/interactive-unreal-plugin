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

class UFunction;
class FJsonObject;

namespace MixerBindingUtils
{
	void ExtractCustomEventParamsFromMessage(const FJsonObject* JsonObject, UFunction* FunctionPrototype, void* ParamStorage, SIZE_T ParamStorageSize);
	void DestroyCustomEventParams(UFunction* FunctionPrototype, void* ParamStorage, SIZE_T ParamStorageSize);
}
