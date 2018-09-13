//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "MixerInteractivityBlueprintLibrary.h"
#include "MixerInteractivityModule.h"
#include "MixerCustomControl.h"
#include "LatentActions.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Logging/MessageLog.h"
#include "JsonObjectConverter.h"

const FName MixerObjectKindMetadataTag = "MixerObjectKind";

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

struct FMixerInteractivityChangeAction : public FPendingLatentAction
{
public:
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;
	EMixerInteractivityState StateToLeave;

public:
	FMixerInteractivityChangeAction(const FLatentActionInfo& LatentInfo, EMixerInteractivityState InStateToLeave)
		: ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
		, StateToLeave(InStateToLeave)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		bool IsDoneChanging = IMixerInteractivityModule::Get().GetInteractivityState() != StateToLeave;
		Response.FinishAndTriggerIf(IsDoneChanging, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		switch (StateToLeave)
		{
		case EMixerInteractivityState::Interactivity_Starting:
			return TEXT("Contacting the service to start interactivity.");
		case EMixerInteractivityState::Interactivity_Stopping:
			return TEXT("Contacting the service to stop interactivity.");
		default:
			return TEXT("Unknown interactivity state change operation.");
		}
	}
#endif
};

static UWorld* GetWorldFromContextObject_EngineVersionHelper(UObject* WorldContextObject)
{
#if ENGINE_MINOR_VERSION >= 17
	return GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
#else
	return GEngine->GetWorldFromContextObject(WorldContextObject);
#endif
}

void UMixerInteractivityBlueprintLibrary::StartInteractivityLatent(UObject* WorldContextObject, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GetWorldFromContextObject_EngineVersionHelper(WorldContextObject))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		if (LatentActionManager.FindExistingAction<FMixerInteractivityChangeAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			IMixerInteractivityModule::Get().StartInteractivity();
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FMixerInteractivityChangeAction(LatentInfo, EMixerInteractivityState::Interactivity_Starting));
		}
	}
}

void UMixerInteractivityBlueprintLibrary::StopInteractivityLatent(UObject* WorldContextObject, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GetWorldFromContextObject_EngineVersionHelper(WorldContextObject))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		if (LatentActionManager.FindExistingAction<FMixerInteractivityChangeAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			IMixerInteractivityModule::Get().StopInteractivity();
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FMixerInteractivityChangeAction(LatentInfo, EMixerInteractivityState::Interactivity_Stopping));
		}
	}
}

void UMixerInteractivityBlueprintLibrary::StartInteractivityNonLatent()
{
	IMixerInteractivityModule::Get().StartInteractivity();
}

void UMixerInteractivityBlueprintLibrary::StopInteractivityNonLatent()
{
	IMixerInteractivityModule::Get().StopInteractivity();
}

struct FMixerLoginAction : public FPendingLatentAction
{
public:
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

public:
	FMixerLoginAction(const FLatentActionInfo& LatentInfo)
		: ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		bool IsDoneLoggingIn = IMixerInteractivityModule::Get().GetLoginState() != EMixerLoginState::Logging_In;
		Response.FinishAndTriggerIf(IsDoneLoggingIn, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return TEXT("Mixer Login Action");
	}
#endif
};

void UMixerInteractivityBlueprintLibrary::LoginSilently(UObject* WorldContextObject, class APlayerController* PlayerController, FLatentActionInfo LatentInfo)
{
	TSharedPtr<const FUniqueNetId> NetId;
	APlayerState* PlayerState = PlayerController ? PlayerController->PlayerState : nullptr;
	if (PlayerState)
	{
		NetId = PlayerState->UniqueId.GetUniqueNetId();
	}

	if (UWorld* World = GetWorldFromContextObject_EngineVersionHelper(WorldContextObject))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		if (LatentActionManager.FindExistingAction<FMixerLoginAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			IMixerInteractivityModule::Get().LoginSilently(NetId);
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FMixerLoginAction(LatentInfo));
		}
	}
}

void UMixerInteractivityBlueprintLibrary::TriggerButtonCooldown(FMixerButtonReference Button, FTimespan Cooldown)
{
	IMixerInteractivityModule::Get().TriggerButtonCooldown(Button.Name, Cooldown);
}

void UMixerInteractivityBlueprintLibrary::GetButtonDescription(FMixerButtonReference Button, FText& ButtonText, FText& HelpText, int32& SparkCost)
{
	FMixerButtonDescription ButtonDesc;
	if (IMixerInteractivityModule::Get().GetButtonDescription(Button.Name, ButtonDesc))
	{
		ButtonText = ButtonDesc.ButtonText;
		HelpText = ButtonDesc.HelpText;
		SparkCost = static_cast<int32>(ButtonDesc.SparkCost);
	}
	else
	{
		SparkCost = 0;
	}
}

void UMixerInteractivityBlueprintLibrary::GetButtonState(FMixerButtonReference Button, FTimespan& RemainingCooldown, float& Progress, int32& DownCount, int32& PressCount, int32& UpCount, bool& Enabled, int32 ParticipantId)
{
	FMixerButtonState ButtonState;
	bool GotState = false;
	if (ParticipantId != 0)
	{
		GotState = IMixerInteractivityModule::Get().GetButtonState(Button.Name, ParticipantId, ButtonState);
	}
	else
	{
		GotState = IMixerInteractivityModule::Get().GetButtonState(Button.Name, ButtonState);
	}

	if (GotState)
	{
		RemainingCooldown = ButtonState.RemainingCooldown;
		Progress = ButtonState.Progress;
		DownCount = static_cast<int32>(ButtonState.DownCount);
		PressCount = static_cast<int32>(ButtonState.PressCount);
		UpCount = static_cast<int32>(ButtonState.UpCount);
		Enabled = ButtonState.Enabled;
	}
	else
	{
		RemainingCooldown = FTimespan(0);
		Progress = 0.0f;
		DownCount = 0;
		PressCount = 0;
		UpCount = 0;
		Enabled = false;
	}
}

void UMixerInteractivityBlueprintLibrary::GetStickDescription(FMixerStickReference Stick, FText& HelpText)
{
	FMixerStickDescription StickDesc;
	if (IMixerInteractivityModule::Get().GetStickDescription(Stick.Name, StickDesc))
	{
		HelpText = StickDesc.HelpText;
	}
}

void UMixerInteractivityBlueprintLibrary::GetStickState(FMixerStickReference Stick, float& XAxis, float& YAxis, bool& Enabled, int32 ParticipantId)
{
	FMixerStickState StickState;
	bool GotState = false;
	if (ParticipantId != 0)
	{
		GotState = IMixerInteractivityModule::Get().GetStickState(Stick.Name, ParticipantId, StickState);
	}
	else
	{
		GotState = IMixerInteractivityModule::Get().GetStickState(Stick.Name, StickState);
	}

	if (GotState)
	{
		XAxis = StickState.Axes.X;
		YAxis = StickState.Axes.Y;
		Enabled = StickState.Enabled;
	}
	else
	{
		XAxis = 0.0f;
		YAxis = 0.0f;
		Enabled = false;
	}
}

void UMixerInteractivityBlueprintLibrary::SetLabelText(FMixerLabelReference Label, const FText& Text)
{
	IMixerInteractivityModule::Get().SetLabelText(Label.Name, Text);
}

void UMixerInteractivityBlueprintLibrary::GetLabelDescription(FMixerLabelReference Label, FText& Text, FString& TextSize, FColor& TextColor, bool& Bold, bool& Underline, bool& Italic)
{
	FMixerLabelDescription LabelDesc;
	if (IMixerInteractivityModule::Get().GetLabelDescription(Label.Name, LabelDesc))
	{
		Text = LabelDesc.Text;
		TextSize = LabelDesc.TextSize;
		TextColor = LabelDesc.TextColor;
		Bold = LabelDesc.Bold;
		Underline = LabelDesc.Underline;
		Italic = LabelDesc.Italic;
	}
	else
	{
		TextColor = FColor::Black;
		Bold = false;
		Underline = false;
		Italic = false;
	}
}

void UMixerInteractivityBlueprintLibrary::GetTextboxDescription(FMixerTextboxReference Textbox, FText& PlaceholderText, bool& Multiline, bool& HasSubmit, FText& SubmitText, int32& SparkCost)
{
	FMixerTextboxDescription TextboxDesc;
	if (IMixerInteractivityModule::Get().GetTextboxDescription(Textbox.Name, TextboxDesc))
	{
		PlaceholderText = TextboxDesc.Placeholder;
		Multiline = TextboxDesc.Multiline;
		HasSubmit = TextboxDesc.HasSubmit;
		SubmitText = TextboxDesc.SubmitText;
		SparkCost = TextboxDesc.SparkCost;
	}
	else
	{
		Multiline = false;
		HasSubmit = false;
		SparkCost = 0;
	}
}

void UMixerInteractivityBlueprintLibrary::SetCurrentScene(FMixerSceneReference Scene, FMixerGroupReference Group)
{
	IMixerInteractivityModule::Get().SetCurrentScene(Scene.Name, Group.Name);
}

void UMixerInteractivityBlueprintLibrary::GetLoggedInUserInfo(int32& UserId, bool& IsLoggedIn, FString& Name, int32& Level, int32& Experience, int32& Sparks)
{
	TSharedPtr<const FMixerLocalUser> User = IMixerInteractivityModule::Get().GetCurrentUser();
	if (User.IsValid())
	{
		IsLoggedIn = true;
		UserId = User->Id;
		Name = User->Name;
		Level = User->Level;
		Experience = User->Experience;
		Sparks = User->Sparks;
	}
	else
	{
		UserId = 0;
		Level = 0;
		Experience = 0;
		Sparks = 0;
	}
}

void UMixerInteractivityBlueprintLibrary::GetRemoteParticipantInfo(int32 ParticipantId, bool& IsParticipating, FString& Name, int32& Level, FMixerGroupReference& Group, bool& InputEnabled, FDateTime& ConnectedAt, FDateTime& LastInputAt)
{
	TSharedPtr<const FMixerRemoteUser> User = IMixerInteractivityModule::Get().GetParticipant(ParticipantId);
	if (User.IsValid())
	{
		IsParticipating = true;
		Name = User->Name;
		Level = User->Level;
		Group.Name = User->Group;
		InputEnabled = User->InputEnabled;
		ConnectedAt = User->ConnectedAt;
		LastInputAt = User->InputAt;
	}
	else
	{
		IsParticipating = false;
		Level = 0;
		InputEnabled = false;
	}
}

void UMixerInteractivityBlueprintLibrary::GetParticipantsInGroup(FMixerGroupReference Group, TArray<int32>& ParticipantIds)
{
	TArray<TSharedPtr<const FMixerRemoteUser>> ParticipantInfo;
	IMixerInteractivityModule::Get().GetParticipantsInGroup(Group.Name, ParticipantInfo);
	ParticipantIds.Empty(ParticipantInfo.Num());
	for (TSharedPtr<const FMixerRemoteUser> Info : ParticipantInfo)
	{
		ParticipantIds.Add(Info->Id);
	}
}

void UMixerInteractivityBlueprintLibrary::MoveParticipantToGroup(FMixerGroupReference Group, int32 ParticipantId)
{
	if (!IMixerInteractivityModule::Get().MoveParticipantToGroup(Group.Name, ParticipantId))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Warning(FText::Format(
			LOCTEXT("MoveToGroupError_NotFound", "MoveParticipantToGroup failed: group {0} was not found.  Create it via Mixer Interactivity Project Settings."),
			FText::FromName(Group.Name)
		));
#endif
	}
}

FName UMixerInteractivityBlueprintLibrary::GetName(const FMixerObjectReference& Obj)
{
	return Obj.Name;
}

void UMixerInteractivityBlueprintLibrary::CaptureSparkTransaction(FMixerTransactionId TransactionId)
{
	IMixerInteractivityModule::Get().CaptureSparkTransaction(TransactionId.Id);
}

#if defined(DEFINE_FUNCTION)
DEFINE_FUNCTION(UMixerInteractivityBlueprintLibrary::execGetCustomControlProperty_Helper)
#else
DECLARE_FUNCTION(UMixerInteractivityBlueprintLibrary::execGetCustomControlProperty_Helper)
#endif
{
	P_GET_PROPERTY(UObjectProperty, WorldContextObject);
	P_GET_STRUCT(FMixerCustomControlReference, Control);
	P_GET_PROPERTY(UStrProperty, PropertyName);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<UProperty>(nullptr);
	P_FINISH;

	P_NATIVE_BEGIN;
	if (WorldContextObject != nullptr)
	{
		UWorld* ForWorld = WorldContextObject->GetWorld();
		if (ForWorld != nullptr)
		{
			if (Stack.MostRecentPropertyAddress != nullptr && Stack.MostRecentProperty != nullptr)
			{
				TSharedPtr<FJsonObject> ControlObject;
				if (IMixerInteractivityModule::Get().GetCustomControl(ForWorld, Control.Name, ControlObject))
				{
					TSharedPtr<FJsonValue> LocatedProperty = ControlObject->TryGetField(PropertyName);
					if (LocatedProperty.IsValid())
					{
						FJsonObjectConverter::JsonValueToUProperty(LocatedProperty, Stack.MostRecentProperty, Stack.MostRecentPropertyAddress, 0, 0);
					}
				}
			}
		}
	}
	P_NATIVE_END;
}

#undef LOCTEXT_NAMESPACE