//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "K2Node_MixerButtonEvent.h"
#include "MixerDynamicDelegateBinding.h"

UClass* UK2Node_MixerButtonEvent::GetDynamicBindingClass() const
{
	return UMixerDelegateBinding::StaticClass();
}

void UK2Node_MixerButtonEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	FMixerButtonEventBinding BindingInfo;
	BindingInfo.TargetFunctionName = CustomFunctionName;
	BindingInfo.ButtonId = ButtonId;
	BindingInfo.Pressed = Pressed;

	UMixerDelegateBinding* MixerBindingObject =  CastChecked<UMixerDelegateBinding>(BindingObject);
	MixerBindingObject->AddButtonBinding(BindingInfo);
}
