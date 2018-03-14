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

#include "Kismet/BlueprintFunctionLibrary.h"

#include "MixerInteractivityTypes.h"
#include "Engine/LatentActionManager.h"
#include "TextProperty.h"
#include "MixerInteractivityBlueprintLibrary.generated.h"

USTRUCT(BlueprintType)
struct MIXERINTERACTIVITY_API FMixerObjectReference
{
public:
	GENERATED_BODY()

public:
	UPROPERTY()
	FName Name;

public:
	/** Export contents of this struct as a string */
	bool ExportTextItem(FString& ValueStr, FMixerObjectReference const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
	{
		ValueStr = Name.ToString();
		return true;
	}

	/** Import string contexts and try to map them into a unique id */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
	{
		Name = Buffer;
		return true;
	}
};

USTRUCT(BlueprintType)
struct MIXERINTERACTIVITY_API FMixerButtonReference : public FMixerObjectReference
{
public:
	GENERATED_BODY()
};

template<>
struct TStructOpsTypeTraits<FMixerButtonReference> : public TStructOpsTypeTraitsBase2<FMixerButtonReference>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true
	};
};

USTRUCT(BlueprintType)
struct MIXERINTERACTIVITY_API FMixerSceneReference : public FMixerObjectReference
{
public:
	GENERATED_BODY()
};

template<>
struct TStructOpsTypeTraits<FMixerSceneReference> : public TStructOpsTypeTraitsBase2<FMixerSceneReference>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true
	};
};

USTRUCT(BlueprintType)
struct MIXERINTERACTIVITY_API FMixerStickReference : public FMixerObjectReference
{
public:
	GENERATED_BODY()
};

template<>
struct TStructOpsTypeTraits<FMixerStickReference> : public TStructOpsTypeTraitsBase2<FMixerStickReference>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true
	};
};

USTRUCT(BlueprintType)
struct MIXERINTERACTIVITY_API FMixerGroupReference : public FMixerObjectReference
{
public:
	GENERATED_BODY()

	FMixerGroupReference()
	{
		Name = NAME_DefaultMixerParticipantGroup;
	}
};

template<>
struct TStructOpsTypeTraits<FMixerGroupReference> : public TStructOpsTypeTraitsBase2<FMixerGroupReference>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true
	};
};

USTRUCT(BlueprintType)
struct MIXERINTERACTIVITY_API FMixerCustomControlReference : public FMixerObjectReference
{
public:
	GENERATED_BODY()
};

template<>
struct TStructOpsTypeTraits<FMixerCustomControlReference> : public TStructOpsTypeTraitsBase2<FMixerCustomControlReference>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true
	};
};

USTRUCT(BlueprintType)
struct MIXERINTERACTIVITY_API FMixerTransactionId
{
public:
	GENERATED_BODY()

	UPROPERTY()
	FString Id;

public:
	/** Export contents of this struct as a string */
	bool ExportTextItem(FString& ValueStr, FMixerTransactionId const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
	{
		ValueStr = Id;
		return true;
	}

	/** Import string contexts and try to map them into a unique id */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
	{
		Id = Buffer;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FMixerTransactionId> : public TStructOpsTypeTraitsBase2<FMixerTransactionId>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true
	};
};

UENUM()
enum class EMixerInteractivityParticipantState : uint8
{
	Joined,
	Input_Disabled,
	Left,
};


UCLASS()
class MIXERINTERACTIVITY_API UMixerInteractivityBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Mixer|Interactivity", 
		meta = (DisplayName = "Start Interactivity", Latent, WorldContext = "WorldContextObject", LatentInfo = "LatentInfo",		
		Tooltip="Notify the Mixer service that the game is ready for interactive input.  This version is latent and waits for the service request to complete before continuing."))
	static void StartInteractivityLatent(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo);

	UFUNCTION(BlueprintCallable, Category = "Mixer|Interactivity", 
		meta = (DisplayName="Start Interactivity",
		Tooltip = "Notify the Mixer service that the game is ready for interactive input.  This version is not latent."))
	static void StartInteractivityNonLatent();

	UFUNCTION(BlueprintCallable, Category = "Mixer|Interactivity", 
		meta = (DisplayName = "Stop Interactivity", Latent, WorldContext = "WorldContextObject", LatentInfo = "LatentInfo",
		Tooltip = "Notify the Mixer service that the game is no longer accepting interactive input.  This version is latent and waits for the service request to complete before continuing."))
	static void StopInteractivityLatent(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo);

	UFUNCTION(BlueprintCallable, Category = "Mixer|Interactivity", 
		meta = (DisplayName = "Stop Interactivity",
		Tooltip = "Notify the Mixer service that the game is no longer accepting interactive input.  This version is not latent."))
	static void StopInteractivityNonLatent();

	/**
	* Sign a local user into the Mixer service without displaying UI.  The operation will fail if user interaction
	* would be required to complete sign-in.  This operation is latent and waits for the request to complete before
	* continuing.
	*
	* @param	PlayerController		Controller for the local player whose identity will be used for Mixer login.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mixer", meta = (Latent, WorldContext = "WorldContextObject", LatentInfo = "LatentInfo"))
	static void LoginSilently(UObject* WorldContextObject, class APlayerController* PlayerController, struct FLatentActionInfo LatentInfo);

	/**
	* Request that a button enter a cooldown state for the specified period.  While cooling down
	* the button will be non-interactive.
	*
	* @param	Button			Reference to the button that should be on cooldown.
	* @param	CooldownTime	Duration for which the button should be non-interactive.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mixer|Interactivity")
	static void TriggerButtonCooldown(FMixerButtonReference Button, FTimespan Cooldown);

	/**
	* Retrieve information about a button that is independent of its current state.
	*
	* @param	Button			Reference to the button for which information should be returned.
	* @param	ButtonText		Text displayed on this button to remote users.
	* @param	HelpText		NOT IMPLEMENTED Button help text that is displayed to remote users (e.g. as a tooltip).
	* @param	SparkCost		Number of Sparks a remote user will be charged for pressing this button.
	*/
	UFUNCTION(BlueprintPure, Category = "Mixer|Interactivity")
	static void GetButtonDescription(FMixerButtonReference Button, FText& ButtonText, FText& HelpText, int32& SparkCost);

	/**
	* Retrieve information about a button that is dependent on remote user and title interactions.
	*
	* @param	Button			Reference to the button for which information should be returned.
	* @param	RemainingCooldown	Time remaining before the button will be interactive again.
	* @param	Progress		
	* @param	DownCount		Number of remote users currently holding the button down.
	* @param	PressCount		Number of remote users who have pressed (down and up) the button in the last interval.
	* @param	UpCount			Number of remote users who have released the button in the last interval.
	* @param	Enabled			Whether the button is currently enabled.
	* @param	ParticipantId	If provided, Mixer id of the remote user whose view of button state should be returned.  Otherwise state is aggregated over all participants.
	*/
	UFUNCTION(BlueprintPure, Category = "Mixer|Interactivity", Meta=(AdvancedDisplay = "7"))
	static void GetButtonState(FMixerButtonReference Button, FTimespan& RemainingCooldown, float& Progress, int32& DownCount, int32& PressCount, int32& UpCount, bool& Enabled, int32 ParticipantId = 0);

	/**
	* Retrieve information about a joystick that is independent of its current state.
	*
	* @param	Stick			Reference to the joystick for which information should be returned.
	* @param	HelpText		Joystick help text that is displayed to remote users (e.g. as a tooltip).
	*/
	UFUNCTION(BlueprintPure, Category = "Mixer|Interactivity")
	static void GetStickDescription(FMixerStickReference Stick, FText& HelpText);

	/**
	* Retrieve information about a joystick that is dependent on remote user and title interactions.
	*
	* @param	Stick			Reference to the joystick for which information should be returned.
	* @param	XAxis			Current aggregate state of the joystick along the X axis [-1,1]
	* @param	YAxis			Current aggregate state of the joystick along the Y axis [-1,1]
	* @param	Enabled			Whether the joystick is currently enabled.
	* @param	ParticipantId	If provided, Mixer id of the remote user whose view of joystick state should be returned.  Otherwise state is aggregated over all participants.
	*/
	UFUNCTION(BlueprintPure, Category = "Mixer|Interactivity", Meta = (AdvancedDisplay = "4"))
	static void GetStickState(FMixerStickReference Stick, float& XAxis, float& YAxis, bool& Enabled, int32 ParticipantId = 0);

	/**
	* Request a change in the interactive scene displayed to remote users.
	*
	* @param	Scene			Reference to the new interactive scene to display.
	* @param	Group			Reference to the user group to which this scene should be displayed.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mixer|Interactivity")
	static void SetCurrentScene(FMixerSceneReference Scene, FMixerGroupReference Group);

	/**
	* Retrieve information describing the local user currently signed in to the Mixer service.
	*
	* @param	UserId			Mixer id of the local user, if logged in.
	* @param	IsLoggedIn		Whether a user is currently logged in.
	* @param	Name			Mixer name of the local user, if logged in.
	* @param	Level			Mixer level of the local user, if logged in.
	* @param	Experience		Mixer experience points of the local user, if logged in.
	* @param	Sparks			Current Spark balance of the local user, if logged in.
	*/
	UFUNCTION(BlueprintPure, Category = "Mixer")
	static void GetLoggedInUserInfo(int32& UserId, bool& IsLoggedIn, FString& Name, int32& Level, int32& Experience, int32& Sparks);

	/**
	* Retrieve information describing a remote user currently interacting with the title on the Mixer service.
	*
	* @param	ParticipantId	Mixer id of the remote user.
	* @param	IsParticipating	Whether the given user is currently interacting with the title on the Mixer service.
	* @param	Name			Mixer name of the given user, if currently interacting.
	* @param	Level			Mixer level of the given user, if currently interacting.
	* @param	Group			The group the given user belongs to.
	* @param	InputEnabled	Whether the given user is able to provide interactive input.
	* @param	ConnectedAt		Date and time when the given user first begain interacting with the title.
	* @param	LastInputAt		Date and time when the given user last provided interactive input.
	*/
	UFUNCTION(BlueprintPure, Category = "Mixer|Interactivity", meta=(AdvancedDisplay="5"))
	static void GetRemoteParticipantInfo(int32 ParticipantId, bool& IsParticipating, FString& Name, int32& Level, FMixerGroupReference& Group, bool& InputEnabled, FDateTime& ConnectedAt, FDateTime& LastInputAt);

	/**
	* Get the collection of all users assigned to a group.
	*
	* @param	Group			Reference to the user group whose members should be returned.
	* @param	ParticipantIds	Collection of ids for members in the given group.
	*/
	UFUNCTION(BlueprintPure, Category = "Mixer|Interactivity")
	static void GetParticipantsInGroup(FMixerGroupReference Group, TArray<int32>& ParticipantIds);

	/**
	* Move a user to a new group.  The group must already exist.
	*
	* @param	Group			Reference to the group that the given user should be placed in.
	* @param	ParticipantId	Id of the user to be moved into the given group.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mixer|Interactivity")
	static void MoveParticipantToGroup(FMixerGroupReference Group, int32 ParticipantId);

	/**
	* Convert a strongly typed reference to a design-time Mixer object to its FName representation.
	*/
	UFUNCTION(BlueprintPure, Category = "Mixer|Interactivity", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FName GetName(const FMixerObjectReference& Obj);

	/**
	* Captures a given interactive event transaction, charging the sparks to the appropriate remote participant.
	*
	* @param	TransactionId	Id of the transaction for which sparks should be charged (obtained from event)
	*/
	UFUNCTION(BlueprintCallable, Category = "Mixer|Interactivity")
	static void CaptureSparkTransaction(FMixerTransactionId TransactionId);

	UFUNCTION(BlueprintPure, Category = "Mixer|Interactivity", CustomThunk, meta=(BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static void GetCustomControlProperty_Helper(UObject* WorldContextObject, FMixerCustomControlReference Control, FString PropertyName, int32 &OutProperty);

	DECLARE_FUNCTION(execGetCustomControlProperty_Helper);

};
