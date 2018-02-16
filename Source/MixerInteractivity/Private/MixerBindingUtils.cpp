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
#include "UObjectGlobals.h"
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
				check(PropIt->GetOffset_ForUFunction() + PropIt->GetSize() <= ParamStorageSize);
				void* ThisParamStorage = static_cast<uint8*>(ParamStorage) + PropIt->GetOffset_ForUFunction();
				PropIt->InitializeValue(ThisParamStorage);
				TSharedPtr<FJsonValue> F = JsonObject->TryGetField(PropIt->GetName());
				if (F.IsValid())
				{
					PropIt->ImportText(*F->AsString(), ThisParamStorage, 0, nullptr);
				}
				else
				{
					UE_LOG(LogMixerInteractivity, Error, TEXT("Custom event %s does not contain expected parameter %s"), *JsonObject->GetStringField(TEXT("method")), *PropIt->GetName());
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
				check(PropIt->GetOffset_ForUFunction() + PropIt->GetSize() <= ParamStorageSize);
				void* ThisParamStorage = static_cast<uint8*>(ParamStorage) + PropIt->GetOffset_ForUFunction();
				PropIt->DestroyValue(ThisParamStorage);
			}
		}
	}
}