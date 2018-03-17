//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MixerInteractivityModule_WithSessionState.h"
#include "MixerJsonHelpers.h"
#include "MixerInteractivityLog.h"

void FMixerInteractivityModule_WithSessionState::TriggerButtonCooldown(FName Button, FTimespan CooldownTime)
{
	FMixerButtonPropertiesCached* CachedButton = Buttons.Find(Button);
	if (CachedButton != nullptr)
	{
		double NewCooldownTime = static_cast<double>((FDateTime::UtcNow() + CooldownTime).ToUnixTimestamp() * 1000);
		TSharedRef<FJsonObject> UpdatedProps = MakeShared<FJsonObject>();
		UpdatedProps->SetNumberField(TEXT("cooldown"), NewCooldownTime);
		UpdateRemoteControl(CachedButton->SceneId, Button, UpdatedProps);
	}
}

bool FMixerInteractivityModule_WithSessionState::GetButtonDescription(FName Button, FMixerButtonDescription& OutDesc)
{
	FMixerButtonPropertiesCached* CachedProps = Buttons.Find(Button);
	if (CachedProps != nullptr)
	{
		OutDesc = CachedProps->Desc;
		return true;
	}
	else
	{
		return false;
	}
}

bool FMixerInteractivityModule_WithSessionState::GetButtonState(FName Button, FMixerButtonState& OutState)
{
	FMixerButtonPropertiesCached* CachedProps = Buttons.Find(Button);
	if (CachedProps != nullptr)
	{
		OutState = CachedProps->State;
		if (!bPerParticipantState)
		{
			OutState.PressCount = 0;
		}
		return true;
	}
	else
	{
		return false;
	}

	return true;
}

bool FMixerInteractivityModule_WithSessionState::GetButtonState(FName Button, uint32 ParticipantId, FMixerButtonState& OutState)
{
	if (bPerParticipantState)
	{
		FMixerButtonPropertiesCached* CachedProps = Buttons.Find(Button);
		if (CachedProps != nullptr)
		{
			OutState = CachedProps->State;

			// Even with per-participant tracking on we don't maintain these.  
			OutState.DownCount = 0;
			OutState.UpCount = 0;
			OutState.PressCount = CachedProps->HoldingParticipants.Contains(ParticipantId) ? 1 : 0;

			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
		{
			UE_LOG(LogMixerInteractivity, Error, TEXT("Polling per-participant button state requires that per-participant state caching is enabled."));
		}
		return false;
	}
}

bool FMixerInteractivityModule_WithSessionState::GetStickDescription(FName Stick, FMixerStickDescription& OutDesc)
{
	// No supported properties
	return false;
}

bool FMixerInteractivityModule_WithSessionState::GetStickState(FName Stick, FMixerStickState& OutState)
{
	if (bPerParticipantState)
	{
		FMixerStickPropertiesCached* CachedProps = Sticks.Find(Stick);
		if (CachedProps != nullptr)
		{
			OutState = CachedProps->State;

			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
		{
			UE_LOG(LogMixerInteractivity, Error, TEXT("Polling aggregate stick state requires that per-participant state caching is enabled."));
		}
		return false;
	}
}

bool FMixerInteractivityModule_WithSessionState::GetStickState(FName Stick, uint32 ParticipantId, FMixerStickState& OutState)
{
	if (bPerParticipantState)
	{
		FMixerStickPropertiesCached* CachedProps = Sticks.Find(Stick);
		if (CachedProps != nullptr)
		{
			OutState.Enabled = CachedProps->State.Enabled;

			FVector2D* PerParticipantState = CachedProps->PerParticipantStickValue.Find(ParticipantId);
			OutState.Axes = (PerParticipantState != nullptr) ? *PerParticipantState : FVector2D(0, 0);
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
		{
			UE_LOG(LogMixerInteractivity, Error, TEXT("Polling per-participant stick state requires that per-participant state caching is enabled."));
		}
		return false;
	}
}


void FMixerInteractivityModule_WithSessionState::SetLabelText(FName Label, const FText& DisplayText)
{
	FMixerLabelPropertiesCached* CachedLabel = Labels.Find(Label);
	if (CachedLabel != nullptr)
	{
		TSharedRef<FJsonObject> UpdateJson = MakeShared<FJsonObject>();
		UpdateJson->SetStringField(MixerStringConstants::FieldNames::Text, DisplayText.ToString());
		UpdateRemoteControl(CachedLabel->SceneId, Label, UpdateJson);
	}
}

bool FMixerInteractivityModule_WithSessionState::GetLabelDescription(FName Label, FMixerLabelDescription& OutDesc)
{
	FMixerLabelPropertiesCached* CachedProps = Labels.Find(Label);
	if (CachedProps != nullptr)
	{
		OutDesc = CachedProps->Desc;
		return true;
	}
	else
	{
		return false;
	}
}

bool FMixerInteractivityModule_WithSessionState::GetTextboxDescription(FName Textbox, FMixerTextboxDescription& OutDesc)
{
	FMixerTextboxPropertiesCached* CachedProps = Textboxes.Find(Textbox);
	if (CachedProps != nullptr)
	{
		OutDesc = CachedProps->Desc;
		return true;
	}
	else
	{
		return false;
	}
}

TSharedPtr<const FMixerRemoteUser> FMixerInteractivityModule_WithSessionState::GetParticipant(uint32 ParticipantId)
{
	TSharedPtr<FMixerRemoteUser>* FoundUser = RemoteParticipantCacheByUint.Find(ParticipantId);
	return FoundUser ? *FoundUser : TSharedPtr<const FMixerRemoteUser>();
}

bool FMixerInteractivityModule_WithSessionState::GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants)
{
	for (TMap<uint32, TSharedPtr<FMixerRemoteUser>>::TConstIterator It(RemoteParticipantCacheByUint); It; ++It)
	{
		if (It->Value->Group == GroupName)
		{
			OutParticipants.Add(It->Value);
		}
	}

	return true;
}

bool FMixerInteractivityModule_WithSessionState::Tick(float DeltaTime)
{
	FMixerInteractivityModule::Tick(DeltaTime);

	FTimespan CooldownDecrement = FTimespan::FromSeconds(DeltaTime);
	for (TMap<FName, FMixerButtonPropertiesCached>::TIterator It(Buttons); It; ++It)
	{
		It->Value.State.RemainingCooldown = FMath::Max(It->Value.State.RemainingCooldown - CooldownDecrement, FTimespan::Zero());

		It->Value.State.DownCount = 0;
		It->Value.State.UpCount = 0;

		// Leave PressCount alone
	}

	return true;
}

bool FMixerInteractivityModule_WithSessionState::HandleSingleControlUpdate(FName ControlId, const TSharedRef<FJsonObject> ControlData)
{
	FMixerButtonPropertiesCached* ButtonProps = Buttons.Find(ControlId);
	if (ButtonProps != nullptr)
	{
		double Cooldown = 0.0f;
		if (ControlData->TryGetNumberField(MixerStringConstants::FieldNames::Cooldown, Cooldown))
		{
			uint64 TimeNowInMixerUnits = FDateTime::UtcNow().ToUnixTimestamp() * 1000;
			if (Cooldown > TimeNowInMixerUnits)
			{
				ButtonProps->State.RemainingCooldown = FTimespan::FromMilliseconds(static_cast<double>(static_cast<uint64>(Cooldown) - TimeNowInMixerUnits));
			}
			else
			{
				ButtonProps->State.RemainingCooldown = FTimespan::Zero();
			}
		}

		FString Text;
		if (ControlData->TryGetStringField(MixerStringConstants::FieldNames::Text, Text))
		{
			ButtonProps->Desc.ButtonText = FText::FromString(Text);
		}

		FString Tooltip;
		if (ControlData->TryGetStringField(MixerStringConstants::FieldNames::Tooltip , Tooltip))
		{
			ButtonProps->Desc.HelpText = FText::FromString(Tooltip);
		}

		uint32 Cost;
		if (ControlData->TryGetNumberField(MixerStringConstants::FieldNames::Cost, Cost))
		{
			ButtonProps->Desc.SparkCost = Cost;
		}

		bool bDisabled;
		if (ControlData->TryGetBoolField(MixerStringConstants::FieldNames::Disabled, bDisabled))
		{
			ButtonProps->State.Enabled = !bDisabled;
		}

		double Progress;
		if (ControlData->TryGetNumberField(MixerStringConstants::FieldNames::Progress, Progress))
		{
			ButtonProps->State.Progress = Progress;
		}

		return true;
	}

	FMixerStickPropertiesCached* StickProps = Sticks.Find(ControlId);
	if (StickProps != nullptr)
	{
		bool bDisabled;
		if (ControlData->TryGetBoolField(MixerStringConstants::FieldNames::Disabled, bDisabled))
		{
			StickProps->State.Enabled = !bDisabled;
		}

		return true;
	}

	return false;
}

void FMixerInteractivityModule_WithSessionState::StartSession(bool bCachePerParticipantState)
{
	check(Buttons.Num() == 0);
	check(Sticks.Num() == 0);
	check(Labels.Num() == 0);
	check(Textboxes.Num() == 0);
	check(RemoteParticipantCacheByGuid.Num() == 0);
	check(RemoteParticipantCacheByUint.Num() == 0);
	bPerParticipantState = bCachePerParticipantState;
}

void FMixerInteractivityModule_WithSessionState::EndSession()
{
	Buttons.Empty();
	Sticks.Empty();
	Labels.Empty();
	Textboxes.Empty();
	RemoteParticipantCacheByGuid.Empty();
	RemoteParticipantCacheByUint.Empty();
}

bool FMixerInteractivityModule_WithSessionState::CachePerParticipantState()
{
	return bPerParticipantState;
}

void FMixerInteractivityModule_WithSessionState::AddButton(FName ControlId, const FMixerButtonPropertiesCached& Props)
{
	Buttons.Add(ControlId, Props);
}

FMixerButtonPropertiesCached* FMixerInteractivityModule_WithSessionState::GetButton(FName ControlId)
{
	return Buttons.Find(ControlId);
}

void FMixerInteractivityModule_WithSessionState::AddStick(FName ControlId, const FMixerStickPropertiesCached& Props)
{
	Sticks.Add(ControlId, Props);
}

FMixerStickPropertiesCached* FMixerInteractivityModule_WithSessionState::GetStick(FName ControlId)
{
	return Sticks.Find(ControlId);
}

void FMixerInteractivityModule_WithSessionState::AddLabel(FName ControlId, const FMixerLabelPropertiesCached& Props)
{
	Labels.Add(ControlId, Props);
}

FMixerLabelPropertiesCached* FMixerInteractivityModule_WithSessionState::GetLabel(FName ControlId)
{
	return Labels.Find(ControlId);
}

void FMixerInteractivityModule_WithSessionState::AddTextbox(FName ControlId, const FMixerTextboxPropertiesCached& Props)
{
	Textboxes.Add(ControlId, Props);
}

FMixerTextboxPropertiesCached* FMixerInteractivityModule_WithSessionState::GetTextbox(FName ControlId)
{
	return Textboxes.Find(ControlId);
}

void FMixerInteractivityModule_WithSessionState::AddUser(TSharedPtr<FMixerRemoteUser> User)
{
	RemoteParticipantCacheByGuid.Add(User->SessionGuid, User);
	RemoteParticipantCacheByUint.Add(User->Id, User);
}

void FMixerInteractivityModule_WithSessionState::RemoveUser(TSharedPtr<FMixerRemoteUser> User)
{
	RemoteParticipantCacheByGuid.Remove(User->SessionGuid);
	RemoteParticipantCacheByUint.Remove(User->Id);
}

void FMixerInteractivityModule_WithSessionState::RemoveUser(FGuid ParticipantSessionId)
{
	TSharedPtr<FMixerRemoteUser> RemovedUser = RemoteParticipantCacheByGuid.FindAndRemoveChecked(ParticipantSessionId);
	RemoteParticipantCacheByUint.Remove(RemovedUser->Id);
}

TSharedPtr<FMixerRemoteUser> FMixerInteractivityModule_WithSessionState::GetCachedUser(uint32 ParticipantId)
{
	TSharedPtr<FMixerRemoteUser>* User = RemoteParticipantCacheByUint.Find(ParticipantId);
	return User != nullptr ? *User : nullptr;
}

TSharedPtr<FMixerRemoteUser> FMixerInteractivityModule_WithSessionState::GetCachedUser(FGuid ParticipantSessionId)
{
	TSharedPtr<FMixerRemoteUser>* User = RemoteParticipantCacheByGuid.Find(ParticipantSessionId);
	return User != nullptr ? *User : nullptr;
}

void FMixerInteractivityModule_WithSessionState::ReassignUsers(FName FromGroup, FName ToGroup)
{
	for (TMap<uint32, TSharedPtr<FMixerRemoteUser>>::TConstIterator It(RemoteParticipantCacheByUint); It; ++It)
	{
		if (It->Value->Group == FromGroup)
		{
			It->Value->Group = ToGroup;
		}
	}
}