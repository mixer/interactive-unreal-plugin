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

#include "MixerInteractivityCustomMethods.generated.h"

UCLASS(Blueprintable, NotBlueprintType, NotPlaceable, NotEditInlineNew, HideDropdown)
class UMixerCustomMethods : public UObject
{
public:
	GENERATED_BODY()

	virtual bool IsEditorOnly() const { return true; }
};
