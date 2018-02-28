//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MixerInteractivityJsonTypes.h"
#include "JsonObject.h"

const FString FMixerInteractiveControl::ButtonKind = TEXT("button");
const FString FMixerInteractiveControl::JoystickKind = TEXT("joystick");

bool FMixerInteractiveControl::IsCustom() const
{
	return !IsButton()
		&& !IsJoystick();
}