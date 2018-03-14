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

#include <interactive-cpp-v2/source/interactivity.cpp>

IMPLEMENT_MODULE(FMixerInteractivityModule_InteractiveCpp2, MixerInteractivity);

void FMixerInteractivityModule_InteractiveCpp2::StartInteractivity()
{
	if (InteractiveSession != nullptr)
	{
		if (mixer::interactive_set_ready(InteractiveSession, true) == mixer::MIXER_OK)
		{
			SetInteractivityState(EMixerInteractivityState::Interactivity_Starting);
		}
	}
}

void FMixerInteractivityModule_InteractiveCpp2::StopInteractivity()
{
	if (InteractiveSession != nullptr)
	{
		if (mixer::interactive_set_ready(InteractiveSession, false) == mixer::MIXER_OK)
		{
			SetInteractivityState(EMixerInteractivityState::Interactivity_Stopping);
		}
	}
}

void FMixerInteractivityModule_InteractiveCpp2::SetCurrentScene(FName Scene, FName GroupName)
{
	if (InteractiveSession != nullptr)
	{
		mixer::interactive_group_set_scene(InteractiveSession, GroupName != NAME_None ? GroupName.GetPlainANSIString() : "default", Scene.GetPlainANSIString());
	}
}

FName FMixerInteractivityModule_InteractiveCpp2::GetCurrentScene(FName GroupName)
{
	FGetCurrentSceneEnumContext Context;
	Context.GroupName = GroupName != NAME_None ? GroupName : FName("default");
	Context.OutSceneName = NAME_None;
	if (InteractiveSession != nullptr)
	{
		mixer::interactive_set_session_context(InteractiveSession, &Context);
		mixer::interactive_get_groups(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnEnumerateForGetCurrentScene);
		mixer::interactive_set_session_context(InteractiveSession, nullptr);
	}
	return Context.OutSceneName;
}

void FMixerInteractivityModule_InteractiveCpp2::TriggerButtonCooldown(FName Button, FTimespan CooldownTime)
{
	if (InteractiveSession != nullptr)
	{
		mixer::interactive_control_trigger_cooldown(InteractiveSession, Button.GetPlainANSIString(), static_cast<uint32>(CooldownTime.GetTotalMilliseconds()));
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::CreateGroup(FName GroupName, FName InitialScene)
{
	if (InteractiveSession == nullptr)
	{
		return false;
	}

	return mixer::interactive_create_group(InteractiveSession, GroupName.GetPlainANSIString(), InitialScene != NAME_None ? InitialScene.GetPlainANSIString() : "default") == mixer::MIXER_OK;
}

bool FMixerInteractivityModule_InteractiveCpp2::GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants)
{
	if (InteractiveSession == nullptr)
	{
		return false;
	}

	FGetParticipantsInGroupEnumContext Context;
	Context.MixerModule = this;
	Context.GroupName = GroupName != NAME_None ? GroupName : FName("default");
	Context.OutParticipants = &OutParticipants;
	mixer::interactive_set_session_context(InteractiveSession, &Context);
	int32 EnumResult = mixer::interactive_get_participants(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnEnumerateForGetParticipantsInGroup);
	mixer::interactive_set_session_context(InteractiveSession, nullptr);
	return EnumResult == mixer::MIXER_OK;
}

bool FMixerInteractivityModule_InteractiveCpp2::MoveParticipantToGroup(FName GroupName, uint32 ParticipantId)
{
	if (InteractiveSession == nullptr)
	{
		return false;
	}

	TSharedPtr<FMixerRemoteUserCached>* Participant = RemoteParticipantCacheByUint.Find(ParticipantId);
	if (Participant == nullptr)
	{
		return false;
	}

	return mixer::interactive_set_participant_group(
		InteractiveSession,
		TCHAR_TO_UTF8(*(*Participant)->SessionGuid.ToString(EGuidFormats::DigitsWithHyphens).ToLower()),
		GroupName.GetPlainANSIString()) == mixer::MIXER_OK;
}

void FMixerInteractivityModule_InteractiveCpp2::CaptureSparkTransaction(const FString& TransactionId)
{
	if (InteractiveSession != nullptr)
	{
		mixer::interactive_capture_transaction(InteractiveSession, TCHAR_TO_UTF8(*TransactionId));
	}
}

void FMixerInteractivityModule_InteractiveCpp2::CallRemoteMethod(const FString& MethodName, const TSharedRef<FJsonObject> MethodParams)
{
	if (InteractiveSession != nullptr)
	{
		FString SerializedParams;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&SerializedParams, 0);
		FJsonSerializer::Serialize(MethodParams, Writer);

		uint32 MessageId = 0;
		mixer::interactive_send_method(InteractiveSession, TCHAR_TO_UTF8(*MethodName), TCHAR_TO_UTF8(*SerializedParams), true, &MessageId);
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::StartInteractiveConnection()
{
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("StartInteractiveConnection failed - plugin state %d."),
			static_cast<int32>(GetInteractiveConnectionAuthState()));
		return false;
	}

	SetInteractiveConnectionAuthState(EMixerLoginState::Logging_In);

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();

	int32 ConnectResult = mixer::interactive_connect(
							TCHAR_TO_UTF8(*UserSettings->GetAuthZHeaderValue()),
							TCHAR_TO_UTF8(*FString::FromInt(Settings->GameVersionId)),
							TCHAR_TO_UTF8(*Settings->ShareCode),
							true,
							&InteractiveSession);
	if (ConnectResult != mixer::MIXER_OK)
	{
		return false;
	}

	mixer::interactive_reg_error_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionError);
	mixer::interactive_reg_state_changed_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionStateChanged);
	mixer::interactive_reg_button_input_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionButtonInput);
	mixer::interactive_reg_coordinate_input_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionCoordinateInput);
	mixer::interactive_reg_participants_changed_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionParticipantsChanged);

	SetInteractiveConnectionAuthState(EMixerLoginState::Logged_In);

	return true;
}

bool FMixerInteractivityModule_InteractiveCpp2::Tick(float DeltaTime)
{
	FMixerInteractivityModule::Tick(DeltaTime);

	mixer::interactive_run(InteractiveSession, 10);

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

	TSharedPtr<FMixerRemoteUserCached> ButtonUser = InteractiveModule.RemoteParticipantCacheByGuid.FindChecked(ParticipantGuid);
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

	TSharedPtr<FMixerRemoteUserCached> StickUser = InteractiveModule.RemoteParticipantCacheByGuid.FindChecked(ParticipantGuid);
	InteractiveModule.OnStickEvent().Broadcast(Input->control.id, StickUser, FVector2D(Input->x, Input->y));
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionParticipantsChanged(void* Context, mixer::interactive_session Session, mixer::participant_action Action, const mixer::interactive_participant* Participant)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());

	FGuid SessionGuid;
	if (!FGuid::Parse(Participant->id, SessionGuid))
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("Participant session id %hs was not in the expected format (guid)"), Participant->id);
		return;
	}

	switch (Action)
	{
	case mixer::participant_join:
		{
			check(!InteractiveModule.RemoteParticipantCacheByGuid.Contains(SessionGuid));

			TSharedPtr<FMixerRemoteUserCached> CachedParticipant = MakeShared<FMixerRemoteUserCached>();
			CachedParticipant->Id = Participant->userId;
			CachedParticipant->SessionGuid = SessionGuid;
			CachedParticipant->Name = UTF8_TO_TCHAR(Participant->userName);
			CachedParticipant->Level = Participant->level;
			CachedParticipant->Group = Participant->groupId;
			CachedParticipant->InputEnabled = !Participant->disabled;
			// Timestamps are in ms since January 1 1970
			CachedParticipant->ConnectedAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Participant->connectedAtMs / 1000.0));
			CachedParticipant->InputAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Participant->lastInputAtMs / 1000.0));

			InteractiveModule.RemoteParticipantCacheByGuid.Add(CachedParticipant->SessionGuid, CachedParticipant);
			InteractiveModule.RemoteParticipantCacheByUint.Add(CachedParticipant->Id, CachedParticipant);
	}
		break;

	case mixer::participant_leave:
		{
			TSharedPtr<FMixerRemoteUser> RemovedUser = InteractiveModule.RemoteParticipantCacheByGuid.FindAndRemoveChecked(SessionGuid);
			InteractiveModule.RemoteParticipantCacheByUint.FindAndRemoveChecked(RemovedUser->Id);
		}
		break;

	case mixer::participant_update:
		{
			TSharedPtr<FMixerRemoteUser> CachedParticipant = InteractiveModule.RemoteParticipantCacheByGuid.FindChecked(SessionGuid);
			check(CachedParticipant.IsValid());
			check(CachedParticipant->Id == Participant->userId);
			CachedParticipant->Name = UTF8_TO_TCHAR(Participant->userName);
			CachedParticipant->Level = Participant->level;
			CachedParticipant->Group = Participant->groupId;
			CachedParticipant->InputAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Participant->lastInputAtMs / 1000.0));
			CachedParticipant->InputEnabled = !Participant->disabled;
	}
		break;

	default:
		UE_LOG(LogMixerInteractivity, Error, TEXT("Unknown participant change type %d"), static_cast<int32>(Action));
		break;
	}
}

bool GetControlPropertyHelper(mixer::interactive_session Session, const FTCHARToUTF8& ControlName, const char *PropertyName, uint32& Result)
{
	int SignedResult;
	if (mixer::interactive_control_get_property_int(Session, ControlName.Get(), PropertyName, &SignedResult) != mixer::MIXER_OK)
	{
		return false;
	}

	Result = static_cast<uint32>(SignedResult);
	return true;
}

bool GetControlPropertyHelper(mixer::interactive_session Session, const FTCHARToUTF8& ControlName, const char *PropertyName, FText& Result)
{
	size_t RequiredSize;
	TArray<char> Utf8String;
	if (mixer::interactive_control_get_property_string(Session, ControlName.Get(), PropertyName, nullptr, &RequiredSize) != mixer::MIXER_ERROR_BUFFER_SIZE)
	{
		return false;
	}

	Utf8String.AddUninitialized(RequiredSize);
	if (mixer::interactive_control_get_property_string(Session, ControlName.Get(), PropertyName, Utf8String.GetData(), &RequiredSize) != mixer::MIXER_OK)
	{
		return false;
	}

	Result = FText::FromString(UTF8_TO_TCHAR(Utf8String.GetData()));
	return true;
}

bool GetControlPropertyHelper(mixer::interactive_session Session, const FTCHARToUTF8& ControlName, const char *PropertyName, float& Result)
{
	return mixer::interactive_control_get_property_float(Session, ControlName.Get(), PropertyName, &Result) == mixer::MIXER_OK;
}

bool GetControlPropertyHelper(mixer::interactive_session Session, const FTCHARToUTF8& ControlName, const char *PropertyName, bool& Result)
{
	return mixer::interactive_control_get_property_bool(Session, ControlName.Get(), PropertyName, &Result) == mixer::MIXER_OK;
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
	TSharedPtr<FMixerRemoteUserCached>* FoundUser = RemoteParticipantCacheByUint.Find(ParticipantId);
	return FoundUser ? *FoundUser : TSharedPtr<const FMixerRemoteUser>();
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateForGetCurrentScene(void* Context, mixer::interactive_session Session, mixer::interactive_group* Group)
{
	FGetCurrentSceneEnumContext* GetSceneContext = static_cast<FGetCurrentSceneEnumContext*>(Context);
	if (GetSceneContext->GroupName == Group->id)
	{
		GetSceneContext->OutSceneName = Group->sceneId;
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateForGetParticipantsInGroup(void* Context, mixer::interactive_session Session, mixer::interactive_participant* Participant)
{
	FGetParticipantsInGroupEnumContext* GetParticipantsContext = static_cast<FGetParticipantsInGroupEnumContext*>(Context);
	if (GetParticipantsContext->GroupName == Participant->groupId)
	{
		TSharedPtr<FMixerRemoteUserCached>* User = GetParticipantsContext->MixerModule->RemoteParticipantCacheByUint.Find(Participant->userId);
		if (User != nullptr)
		{
			GetParticipantsContext->OutParticipants->Add(*User);
		}
	}
}

#endif