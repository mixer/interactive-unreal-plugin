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

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "MixerCustomControl.h"
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

	UMixerInteractivitySettings();

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
	UPROPERTY(EditAnywhere, Config, Category = "Game Binding", meta=(NoSpinBox=true))
	int32 GameVersionId;

	/**
	* The Share Code for the bound game and version (if configured).  Used to allow
	* Mixer users other than the author to broadcast the interactive project before
	* it is published.  See https://dev.mixer.com/reference/interactive/index.html#sharing-your-project
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Game Binding")
	FString ShareCode;

	UPROPERTY(EditAnywhere, Config, Category = "Interactive Controls", AdvancedDisplay, meta = (DisplayName = "Groups"))
	TArray<FMixerPredefinedGroup> DesignTimeGroups;

	/**
	* Type defining (via Delegates/Event Dispatchers) a set of custom methods that
	* the Mixer Interactive service may invoke on the client.  Provide a valid class
	* here in order to enable handling of these methods in Blueprint with strongly
	* typed parameters.
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Interactive Controls", AdvancedDisplay, meta=(MetaClass="MixerCustomMethods"))
	FSoftClassPath CustomMethods;

	/**
	* Asset containing the scenes/controls for the Mixer Interactive Project that will
	* be used to populate available Blueprint Nodes in the Unreal Editor and define
	* custom control mappings.
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Interactive Controls", meta = (AllowedClasses = "MixerProjectAsset"))
	FSoftObjectPath ProjectDefinition;

	/**
	* Choose whether built-in controls such as buttons and joysticks maintain information
	* about their state for each remote user in an interactive session.  Disabling this
	* may reduce memory consumption (and to a lesser extent CPU), especially when there
	* are many remote users, but will limit the ability to poll for state.
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Interactive Controls", AdvancedDisplay, meta = (DisplayName = "Track built-in control state per remote participant"))
	bool bPerParticipantStateCaching;

public:
	FString GetResolvedRedirectUri() const
	{
		FString ResolvedRedirectUri = RedirectUri;
		ResolvedRedirectUri = ResolvedRedirectUri.Replace(TEXT(".*."), TEXT("."));
		if (!ResolvedRedirectUri.StartsWith(TEXT("http")))
		{
			ResolvedRedirectUri = FString(TEXT("http://")) + ResolvedRedirectUri;
		}
		ResolvedRedirectUri = ResolvedRedirectUri.Replace(TEXT("/*."), TEXT("/www."));

		return ResolvedRedirectUri;
	}

	FString GetSandboxForOAuth() const
	{
#if WITH_EDITOR
		return Sandbox.IsEmpty() ? TEXT("RETAIL") : Sandbox;
#else
		return TEXT("RETAIL");
#endif
	}

#if WITH_EDITORONLY_DATA
	static void GetAllControls(const FString& Kind, TArray<FString>& OutControls);
	static void GetAllUnmappedCustomControls(TArray<FString>& OutCustomControls);
#endif
};