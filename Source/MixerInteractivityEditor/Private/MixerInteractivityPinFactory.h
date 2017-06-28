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

#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "MixerInteractivitySettings.h"
#include "MixerInteractivityBlueprintLibrary.h"
#include "SGraphPin.h"
#include "MixerInteractivityEditorModule.h"
#include "SGraphPinMixerObjectNameList.h"


class FMixerInteractivityPinFactory : public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		TSharedPtr<SGraphPinMixerObjectNameList> PinWidget;
		if (InPin->PinType.PinCategory == K2Schema->PC_Struct)
		{
			SGraphPinMixerObjectNameList::FGetMixerObjectNames ItemsSource;
			if (InPin->PinType.PinSubCategoryObject == FMixerButtonReference::StaticStruct())
			{
				ItemsSource = SGraphPinMixerObjectNameList::FGetMixerObjectNames::CreateRaw(&IMixerInteractivityEditorModule::Get(), &IMixerInteractivityEditorModule::GetDesignTimeButtons);
			}
			else if (InPin->PinType.PinSubCategoryObject == FMixerSceneReference::StaticStruct())
			{
				ItemsSource = SGraphPinMixerObjectNameList::FGetMixerObjectNames::CreateRaw(&IMixerInteractivityEditorModule::Get(), &IMixerInteractivityEditorModule::GetDesignTimeScenes);
			}
			else if (InPin->PinType.PinSubCategoryObject == FMixerStickReference::StaticStruct())
			{
				ItemsSource = SGraphPinMixerObjectNameList::FGetMixerObjectNames::CreateRaw(&IMixerInteractivityEditorModule::Get(), &IMixerInteractivityEditorModule::GetDesignTimeSticks);
			}
			else if (InPin->PinType.PinSubCategoryObject == FMixerGroupReference::StaticStruct())
			{
				ItemsSource = SGraphPinMixerObjectNameList::FGetMixerObjectNames::CreateRaw(&IMixerInteractivityEditorModule::Get(), &IMixerInteractivityEditorModule::GetDesignTimeGroups);
			}

			if (ItemsSource.IsBound())
			{
				return SNew(SGraphPinMixerObjectNameList, InPin, ItemsSource);
			}
		}

		return nullptr;
	}
};
