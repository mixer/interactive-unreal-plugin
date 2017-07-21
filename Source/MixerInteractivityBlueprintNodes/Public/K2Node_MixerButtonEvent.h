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

#include "K2Node_Event.h"
#include "K2Node_MixerButtonEvent.generated.h"

UCLASS(MinimalAPI)
class UK2Node_MixerButtonEvent : public UK2Node_Event
{
public:
	GENERATED_BODY()

	UPROPERTY()
	FName ButtonId;

	UPROPERTY()
	bool Pressed;

	virtual UClass* GetDynamicBindingClass() const override;
	virtual void RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const override;

};