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

#include "JsonTypes.h"
#include "JsonSerializerMacros.h"

struct MIXERINTERACTIVITY_API FMixerInteractiveControl : public FJsonSerializable
{
public:
	FString Id;
	FString Kind;

public:
	static const FString ButtonKind;
	static const FString JoystickKind;

	bool IsButton() const { return Kind == ButtonKind; }
	bool IsJoystick() const { return Kind == JoystickKind; }
	bool IsCustom() const;

public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("controlID", Id);
		JSON_SERIALIZE("kind", Kind);
	END_JSON_SERIALIZER
public:
	bool operator==(const FMixerInteractiveControl& Rhs) const
	{
		return Id == Rhs.Id && Kind == Rhs.Kind;
	}
	bool operator !=(const FMixerInteractiveControl& Rhs) const
	{
		return !(*this == Rhs);
	}
};

struct MIXERINTERACTIVITY_API FMixerInteractiveScene : public FJsonSerializable
{
public:
	FString Id;
	TArray<FMixerInteractiveControl> Controls;

public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("sceneID", Id);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("controls", Controls, FMixerInteractiveControl);
	END_JSON_SERIALIZER
public:
	bool operator==(const FMixerInteractiveScene& Rhs) const
	{
		return Id == Rhs.Id && Controls == Rhs.Controls;
	}
	bool operator !=(const FMixerInteractiveScene& Rhs) const
	{
		return !(*this == Rhs);
	}
};

struct MIXERINTERACTIVITY_API FMixerInteractiveControlsCollection : public FJsonSerializable
{
public:
	TArray<FMixerInteractiveScene> Scenes;

public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("scenes", Scenes, FMixerInteractiveScene);
	END_JSON_SERIALIZER
public:
	bool operator==(const FMixerInteractiveControlsCollection& Rhs) const
	{
		return Scenes == Rhs.Scenes;
	}
	bool operator !=(const FMixerInteractiveControlsCollection& Rhs) const
	{
		return !(*this == Rhs);
	}
};

struct MIXERINTERACTIVITY_API FMixerInteractiveGameVersion : public FJsonSerializable
{
public:
	FString Name;
	uint32 Id;
	FMixerInteractiveControlsCollection Controls;
public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("id", Id);
		JSON_SERIALIZE("version", Name);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("controls", Controls);
	END_JSON_SERIALIZER

public:
	bool operator==(const FMixerInteractiveGameVersion& Rhs) const
	{
		return Id == Rhs.Id && Name == Rhs.Name && Controls == Rhs.Controls;
	}
	bool operator !=(const FMixerInteractiveGameVersion& Rhs) const
	{
		return !(*this == Rhs);
	}
};

struct MIXERINTERACTIVITY_API FMixerInteractiveGame : public FJsonSerializable
{
public:
	FString Name;
	FString Description;
	TArray<FMixerInteractiveGameVersion> Versions;
	uint32 Id;

public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("name", Name);
		JSON_SERIALIZE("id", Id);
		JSON_SERIALIZE("description", Description);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("versions", Versions, FMixerInteractiveGameVersion);
	END_JSON_SERIALIZER
};
