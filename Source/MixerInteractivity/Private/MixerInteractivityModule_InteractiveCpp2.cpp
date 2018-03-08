//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "MixerInteractivityModule_InteractiveCpp2.h"

#if MIXER_BACKEND_INTERACTIVE_CPP_2

#include "MixerInteractivitySettings.h"
#include "MixerInteractivityUserSettings.h"
#include "MixerInteractivityLog.h"
#include "StringConv.h"

bool FMixerInteractivityModule_InteractiveCpp2::StartInteractiveConnection()
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();

	mixer::interactive_config Config;
	FTCHARToUTF8 AuthCodeUtf8(*UserSettings->AccessToken);
	Config.authorization = AuthCodeUtf8.Get();
	Config.authorizationLength = AuthCodeUtf8.Length();
	FTCHARToUTF8 ShareCodeUtf8(*Settings->ShareCode);
	Config.shareCode = ShareCodeUtf8.Get();
	Config.shareCodeLength = ShareCodeUtf8.Length();
	FTCHARToUTF8 VersionIdUtf8(*FString::FromInt(Settings->GameVersionId));
	Config.versionId = VersionIdUtf8.Get();
	Config.versionIdLength = VersionIdUtf8.Length();
	Config.manualStart = true;

	// Don't cache participante internally, we'll do it ourselves
	Config.cacheParticipants = false;

	if (mixer::interactive_connect(&Config, nullptr, &InteractiveSession) != mixer::MIXER_OK)
	{
		return false;
	}

	mixer::interactive_reg_error_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionError);
	mixer::interactive_reg_state_changed_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionStateChanged);
	mixer::interactive_reg_button_input_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionButtonInput);
	mixer::interactive_reg_coordinate_input_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionCoordinateInput);
	mixer::interactive_reg_participants_changed_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionParticipantsChanged);

	return true;
}

bool FMixerInteractivityModule_InteractiveCpp2::Tick(float DeltaTime)
{
	FMixerInteractivityModule::Tick(DeltaTime);

	mixer::interactive_run_one(InteractiveSession);

	return true;
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionStateChanged(void* Context, mixer::interactive_session Session, mixer::interactive_state PreviousState, mixer::interactive_state NewState)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());
	switch (NewState)
	{
	case mixer::not_ready:
		InteractiveModule.SetInteractivityState(EMixerInteractivityState::Not_Interactive);
		break;

	case mixer::ready:
		InteractiveModule.SetInteractivityState(EMixerInteractivityState::Interactive);
		break;

	case mixer::disconnected:
	default:
		InteractiveModule.SetInteractivityState(EMixerInteractivityState::Not_Interactive);
		InteractiveModule.SetInteractiveConnectionAuthState(EMixerLoginState::Not_Logged_In);
		break;
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionError(void* Context, mixer::interactive_session Session, int ErrorCode, const char* ErrorMessage, size_t ErrorMessageLength)
{
	UE_LOG(LogMixerInteractivity, Error, TEXT("Session error %d: %hs"), ErrorCode, ErrorMessage);
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionButtonInput(void* Context, mixer::interactive_session Session, const mixer::interactive_button_input* Input)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());

	FGuid ParticipantGuid;
	if (!FGuid::Parse(Input->participantId, ParticipantGuid))
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("Participant id %hs was not in the expected format (guid)"), Input->participantId);
		return;
	}

	TSharedPtr<FMixerRemoteUser> ButtonUser = InteractiveModule.RemoteParticipantCacheByGuid.FindChecked(ParticipantGuid);
	FMixerButtonEventDetails ButtonEventDetails;
	ButtonEventDetails.Pressed = Input->action == mixer::down;
	ButtonEventDetails.SparkCost = 0;
	InteractiveModule.OnButtonEvent().Broadcast(Input->control.id, ButtonUser, ButtonEventDetails);
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionCoordinateInput(void* Context, mixer::interactive_session Session, const mixer::interactive_coordinate_input* Input)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());

	FGuid ParticipantGuid;
	if (!FGuid::Parse(Input->participantId, ParticipantGuid))
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("Participant id %hs was not in the expected format (guid)"), Input->participantId);
		return;
	}

	TSharedPtr<FMixerRemoteUser> StickUser = InteractiveModule.RemoteParticipantCacheByGuid.FindChecked(ParticipantGuid);
	InteractiveModule.OnStickEvent().Broadcast(Input->control.id, StickUser, FVector2D(Input->x, Input->y));
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionParticipantsChanged(void* Context, mixer::interactive_session Session, mixer::participant_action Action, const mixer::interactive_participant* Participant)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());

	FGuid ParticipantGuid;
	if (!FGuid::Parse(Participant->id, ParticipantGuid))
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("Participant id %hs was not in the expected format (guid)"), Participant->id);
		return;
	}

	switch (Action)
	{
	case mixer::participant_join:
		{
			check(!InteractiveModule.RemoteParticipantCacheByGuid.Contains(ParticipantGuid));

			TSharedPtr<FMixerRemoteUser> CachedParticipant = MakeShared<FMixerRemoteUser>();
			CachedParticipant->Id = Participant->userId;
			CachedParticipant->Name = UTF8_TO_TCHAR(Participant->userName);
			CachedParticipant->Level = Participant->level;
			CachedParticipant->Group = UTF8_TO_TCHAR(Participant->groupId);
			CachedParticipant->InputEnabled = !Participant->disabled;
			// Timestamps are in ms since January 1 1970
			CachedParticipant->ConnectedAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Participant->connectedAtMs / 1000.0));
			CachedParticipant->InputAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Participant->lastInputAtMs / 1000.0));

			InteractiveModule.RemoteParticipantCacheByGuid.Add(ParticipantGuid, CachedParticipant);
			InteractiveModule.RemoteParticipantCachedByUint.Add(CachedParticipant->Id, CachedParticipant);
	}
		break;

	case mixer::participant_leave:
		{
			TSharedPtr<FMixerRemoteUser> RemovedUser = InteractiveModule.RemoteParticipantCacheByGuid.FindAndRemoveChecked(ParticipantGuid);
			InteractiveModule.RemoteParticipantCachedByUint.FindAndRemoveChecked(RemovedUser->Id);
		}
		break;

	case mixer::participant_update:
		{
			TSharedPtr<FMixerRemoteUser> CachedParticipant = InteractiveModule.RemoteParticipantCacheByGuid.FindChecked(ParticipantGuid);
			check(CachedParticipant.IsValid());
			check(CachedParticipant->Id == Participant->userId);
			CachedParticipant->Name = UTF8_TO_TCHAR(Participant->userName);
			CachedParticipant->Level = Participant->level;
			CachedParticipant->Group = UTF8_TO_TCHAR(Participant->groupId);
			CachedParticipant->InputAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Participant->lastInputAtMs / 1000.0));
			CachedParticipant->InputEnabled = !Participant->disabled;
	}
		break;

	default:
		UE_LOG(LogMixerInteractivity, Error, TEXT("Unknown participant change type %d"), static_cast<int32>(Action));
		break;
	}
}

template <size_t N>
bool GetControlPropertyHelper(mixer::interactive_session Session, const FTCHARToUTF8& ControlName, const char (&PropertyName)[N], uint32& Result)
{
	int SignedResult;
	if (mixer::interactive_control_get_property_int(Session, ControlName.Get(), ControlName.Length(), PropertyName, N, &SignedResult) != mixer::MIXER_OK)
	{
		return false;
	}

	Result = static_cast<uint32>(SignedResult);
	return true;
}

template <size_t N>
bool GetControlPropertyHelper(mixer::interactive_session Session, const FTCHARToUTF8& ControlName, const char (&PropertyName)[N], FText& Result)
{
	size_t RequiredSize;
	TArray<char> Utf8String;
	if (mixer::interactive_control_get_property_string(Session, ControlName.Get(), ControlName.Length(), PropertyName, N, nullptr, &RequiredSize) != mixer::MIXER_ERROR_BUFFER_SIZE)
	{
		return false;
	}

	Utf8String.AddUninitialized(RequiredSize);
	if (mixer::interactive_control_get_property_string(Session, ControlName.Get(), ControlName.Length(), PropertyName, N, Utf8String.GetData(), &RequiredSize) != mixer::MIXER_OK)
	{
		return false;
	}

	Result = FText::FromString(UTF8_TO_TCHAR(Utf8String.GetData()));
	return true;
}

template <size_t N>
bool GetControlPropertyHelper(mixer::interactive_session Session, const FTCHARToUTF8& ControlName, const char(&PropertyName)[N], float& Result)
{
	return mixer::interactive_control_get_property_float(Session, ControlName.Get(), ControlName.Length(), PropertyName, N, &Result) == mixer::MIXER_OK;
}

template <size_t N>
bool GetControlPropertyHelper(mixer::interactive_session Session, const FTCHARToUTF8& ControlName, const char(&PropertyName)[N], bool& Result)
{
	return mixer::interactive_control_get_property_bool(Session, ControlName.Get(), ControlName.Length(), PropertyName, N, &Result) == mixer::MIXER_OK;
}



bool FMixerInteractivityModule_InteractiveCpp2::GetButtonDescription(FName Button, FMixerButtonDescription& OutDesc)
{
	FTCHARToUTF8 ButtonNameUtf8(*Button.GetPlainNameString());
	if (!GetControlPropertyHelper(InteractiveSession, ButtonNameUtf8, "cost", OutDesc.SparkCost))
	{
		return false;
	}

	if (!GetControlPropertyHelper(InteractiveSession, ButtonNameUtf8, "text", OutDesc.ButtonText))
	{
		return false;
	}

	if (!GetControlPropertyHelper(InteractiveSession, ButtonNameUtf8, "tooltip", OutDesc.HelpText))
	{
		return false;
	}

	return true;
}

bool FMixerInteractivityModule_InteractiveCpp2::GetButtonState(FName Button, FMixerButtonState& OutState)
{
	FTCHARToUTF8 ButtonNameUtf8(*Button.GetPlainNameString());
	// @TODO: how is cooldown specified?
	//int CooldownTime;
	//if (!GetControlPropertyHelper(InteractiveSession, ButtonNameUtf8, "cooldown", CooldownTime))
	//{
	//	return false;
	//}
	OutState.RemainingCooldown = FTimespan(0);

	if (!GetControlPropertyHelper(InteractiveSession, ButtonNameUtf8, "progress", OutState.Progress))
	{
		return false;
	}

	bool bDisabled = false;
	if (!GetControlPropertyHelper(InteractiveSession, ButtonNameUtf8, "disabled", bDisabled))
	{
		return false;
	}
	OutState.Enabled = !bDisabled;

	// @TODO
	OutState.PressCount = 0;
	OutState.DownCount = 0;
	OutState.UpCount = 0;

	return true;
}

bool FMixerInteractivityModule_InteractiveCpp2::GetButtonState(FName Button, uint32 ParticipantId, FMixerButtonState& OutState)
{
	// @TODO
	return false;
}

bool FMixerInteractivityModule_InteractiveCpp2::GetStickDescription(FName Stick, FMixerStickDescription& OutDesc)
{
	// No supported properties
	return false;
}

bool FMixerInteractivityModule_InteractiveCpp2::GetStickState(FName Stick, FMixerStickState& OutState)
{
	FTCHARToUTF8 StickNameUtf8(*Stick.GetPlainNameString());

	bool bDisabled = false;
	if (!GetControlPropertyHelper(InteractiveSession, StickNameUtf8, "disabled", bDisabled))
	{
		return false;
	}
	OutState.Enabled = !bDisabled;

	// @TODO
	OutState.Axes = FVector2D(0, 0);
	return true;
}

bool FMixerInteractivityModule_InteractiveCpp2::GetStickState(FName Stick, uint32 ParticipantId, FMixerStickState& OutState)
{
	// @TODO
	return false;
}

TSharedPtr<const FMixerRemoteUser> FMixerInteractivityModule_InteractiveCpp2::GetParticipant(uint32 ParticipantId)
{
	TSharedPtr<FMixerRemoteUser>* FoundUser = RemoteParticipantCachedByUint.Find(ParticipantId);
	return FoundUser ? *FoundUser : TSharedPtr<const FMixerRemoteUser>();
}

#endif