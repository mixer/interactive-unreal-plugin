//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MixerBindingUtils.h"

#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "UObject/UObjectGlobals.h"
#include "MixerInteractivityLog.h"

namespace MixerBindingUtils
{
	void ExtractCustomEventParamsFromMessage(const FJsonObject* JsonObject, UFunction* FunctionPrototype, void* ParamStorage, SIZE_T ParamStorageSize)
	{
		check(ParamStorageSize == FunctionPrototype->ParmsSize);
		for (TFieldIterator<UProperty> PropIt(FunctionPrototype); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			if (!PropIt->HasAnyPropertyFlags(CPF_OutParm) || PropIt->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				check(static_cast<SIZE_T>(PropIt->GetOffset_ForUFunction() + PropIt->GetSize()) <= ParamStorageSize);
				void* ThisParamStorage = static_cast<uint8*>(ParamStorage) + PropIt->GetOffset_ForUFunction();
				PropIt->InitializeValue(ThisParamStorage);
				TSharedPtr<FJsonValue> F = JsonObject->TryGetField(PropIt->GetName());
				if (F.IsValid())
				{
					if (!FJsonObjectConverter::JsonValueToUProperty(F, *PropIt, ThisParamStorage, 0, 0))
					{
						UE_LOG(LogMixerInteractivity, Error, TEXT("Custom event %s: failed to convert Json value %s for parameter %s"), *FunctionPrototype->GetName(), *F->AsString(), *PropIt->GetName());
					}
				}
				else
				{
					UE_LOG(LogMixerInteractivity, Error, TEXT("Custom event %s does not contain expected parameter %s"), *FunctionPrototype->GetName(), *PropIt->GetName());
				}
			}
		}
	}

	void DestroyCustomEventParams(UFunction* FunctionPrototype, void* ParamStorage, SIZE_T ParamStorageSize)
	{
		for (TFieldIterator<UProperty> PropIt(FunctionPrototype); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			if (!PropIt->HasAnyPropertyFlags(CPF_OutParm) || PropIt->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				check(static_cast<SIZE_T>(PropIt->GetOffset_ForUFunction() + PropIt->GetSize()) <= ParamStorageSize);
				void* ThisParamStorage = static_cast<uint8*>(ParamStorage) + PropIt->GetOffset_ForUFunction();
				PropIt->DestroyValue(ThisParamStorage);
			}
		}
	}
}