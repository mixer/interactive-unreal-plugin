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

#include "ObjectMacros.h"
#include "Object.h"
#include "MixerInteractivitySettings.generated.h"

USTRUCT()
struct FMixerPredefinedGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Game Binding")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Game Binding")
	FName InitialScene;
};

UCLASS(config=Game, defaultconfig)
class MIXERINTERACTIVITY_API UMixerInteractivitySettings : public UObject
{
	GENERATED_BODY()

public:

	/** 
	* The OAuth Client Id for this application.
	* Obtain this value from https://mixer.com/lab
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Auth")
	FString ClientId;

	/**
	* The OAuth RedirectUri for this application.
	* Configure this value at https://mixer.com/lab
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Auth")
	FString RedirectUri;

	/**
	* The Xbox Live Sandbox for a developer Microsoft Account.
	* Required when using a developer Microsoft Account to log in
	* inside the Editor.
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Auth", AdvancedDisplay)
	FString Sandbox;

	/** 
	* The name of the Mixer Interactive Project that this title should be associated with.
	* Available options are based on the set of games owned by the current logged in Mixer user.
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Game Binding")
	FString GameName;

	/**
	* The version of the Mixer Interactive Project that this title should be associated with.
	* Available options are based on the set of versions for the currently selected game.
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Game Binding")
	int32 GameVersionId;

	/** Collection of buttons, saved into the Unreal project and used to populate UI in the Blueprint Editor. */
	UPROPERTY(EditAnywhere, Config, Category= "Game Binding", meta = (DisplayName = "Buttons"))
	TArray<FName> CachedButtons;

	/** Collection of joysticks, saved into the Unreal project and used to populate UI in the Blueprint Editor. */
	UPROPERTY(EditAnywhere, Config, Category = "Game Binding", meta=(DisplayName="Sticks"))
	TArray<FName> CachedSticks;

	/** Collection of scenes, saved into the Unreal project and used to populate UI in the Blueprint Editor. */
	UPROPERTY(EditAnywhere, Config, Category = "Game Binding", meta = (DisplayName = "Scenes"))
	TArray<FName> CachedScenes;

	UPROPERTY(EditAnywhere, Config, Category = "Game Binding", meta = (DisplayName = "Groups"))
	TArray<FMixerPredefinedGroup> DesignTimeGroups;
};