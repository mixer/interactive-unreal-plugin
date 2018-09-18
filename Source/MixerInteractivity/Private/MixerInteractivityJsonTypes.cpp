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
#include "Dom/JsonObject.h"

const FString FMixerInteractiveControl::ButtonKind = TEXT("button");
const FString FMixerInteractiveControl::JoystickKind = TEXT("joystick");
const FString FMixerInteractiveControl::LabelKind = TEXT("label");
const FString FMixerInteractiveControl::TextboxKind = TEXT("textbox");

bool FMixerInteractiveControl::IsCustom() const
{
	return !IsButton()
		&& !IsJoystick()
		&& !IsLabel()
		&& !IsTextbox();
}

void FMixerInteractiveGame::Serialize(FJsonSerializerBase& Serializer, bool bFlatObject)
{
	if (!bFlatObject) { Serializer.StartObject(); }
	JSON_SERIALIZE("name", Name);
	JSON_SERIALIZE("id", Id);
	JSON_SERIALIZE("description", Description);
	JSON_SERIALIZE_ARRAY_SERIALIZABLE("versions", Versions, FMixerInteractiveGameVersion);
	if (!bFlatObject) { Serializer.EndObject(); }
}