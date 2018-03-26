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
#include "Async.h"

IMPLEMENT_MODULE(FMixerInteractivityModule_InteractiveCpp2, MixerInteractivity);

namespace
{
	bool GetControlPropertyHelper(mixer::interactive_session Session, const char* ControlName, const char *PropertyName, FString& Result)
	{
		size_t RequiredSize = 0;
		TArray<char> Utf8String;
		if (mixer::interactive_control_get_property_string(Session, ControlName, PropertyName, nullptr, &RequiredSize) != mixer::MIXER_ERROR_BUFFER_SIZE)
		{
			return false;
		}

		Utf8String.AddUninitialized(RequiredSize);
		if (mixer::interactive_control_get_property_string(Session, ControlName, PropertyName, Utf8String.GetData(), &RequiredSize) != mixer::MIXER_OK)
		{
			return false;
		}

		Result = UTF8_TO_TCHAR(Utf8String.GetData());
		return true;
	}

	bool GetControlPropertyHelper(mixer::interactive_session Session, const char* ControlName, const char *PropertyName, FText& Result)
	{
		FString IntermediateResult;
		if (GetControlPropertyHelper(Session, ControlName, PropertyName, IntermediateResult))
		{
			Result = FText::FromString(IntermediateResult);
			return true;
		}
		else
		{
			return false;
		}
	}

	bool GetControlPropertyHelper(mixer::interactive_session Session, const char* ControlName, const char *PropertyName, float& Result)
	{
		return mixer::interactive_control_get_property_float(Session, ControlName, PropertyName, &Result) == mixer::MIXER_OK;
	}

	bool GetControlPropertyHelper(mixer::interactive_session Session, const char* ControlName, const char *PropertyName, bool& Result)
	{
		return mixer::interactive_control_get_property_bool(Session, ControlName, PropertyName, &Result) == mixer::MIXER_OK;
	}

	bool GetControlPropertyHelper(mixer::interactive_session Session, const char* ControlName, const char *PropertyName, int64& Result)
	{
		return mixer::interactive_control_get_property_int64(Session, ControlName, PropertyName, &Result) == mixer::MIXER_OK;
	}

	bool GetControlPropertyHelper(mixer::interactive_session Session, const char* ControlName, const char *PropertyName, uint32& Result)
	{
		int SignedResult;
		if (mixer::interactive_control_get_property_int(Session, ControlName, PropertyName, &SignedResult) != mixer::MIXER_OK)
		{
			return false;
		}

		Result = static_cast<uint32>(SignedResult);
		return true;
	}
}

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

bool FMixerInteractivityModule_InteractiveCpp2::MoveParticipantToGroup(FName GroupName, uint32 ParticipantId)
{
	if (InteractiveSession == nullptr)
	{
		return false;
	}

	TSharedPtr<FMixerRemoteUser> Participant = GetCachedUser(ParticipantId);
	if (!Participant.IsValid())
	{
		return false;
	}

	return mixer::interactive_set_participant_group(
		InteractiveSession,
		TCHAR_TO_UTF8(*Participant->SessionGuid.ToString(EGuidFormats::DigitsWithHyphens).ToLower()),
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

	ConnectOperation = Async<mixer::interactive_session>(EAsyncExecution::ThreadPool,
		[]() -> mixer::interactive_session
	{
		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();

		mixer::interactive_session Session;
		int32 ConnectResult = mixer::interactive_connect(
			TCHAR_TO_UTF8(*UserSettings->GetAuthZHeaderValue()),
			TCHAR_TO_UTF8(*FString::FromInt(Settings->GameVersionId)),
			TCHAR_TO_UTF8(*Settings->ShareCode),
			true,
			&Session);
		if (ConnectResult == mixer::MIXER_OK)
		{
			return Session;
		}
		else
		{
			return nullptr;
		}
	});

	return true;
}

void FMixerInteractivityModule_InteractiveCpp2::StopInteractiveConnection()
{
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
	{
		SetInteractiveConnectionAuthState(EMixerLoginState::Not_Logged_In);
		mixer::interactive_disconnect(InteractiveSession);
		EndSession();
		InteractiveSession = nullptr;
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::Tick(float DeltaTime)
{
	FMixerInteractivityModule_WithSessionState::Tick(DeltaTime);

	if (InteractiveSession != nullptr)
	{
		mixer::interactive_run(InteractiveSession, 10);
	}
	else if (ConnectOperation.IsReady())
	{
		InteractiveSession = ConnectOperation.Get();
		ConnectOperation = TFuture<mixer::interactive_session>();

		if (InteractiveSession != nullptr)
		{
			const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();

			StartSession(Settings->bPerParticipantStateCaching);

			mixer::interactive_reg_error_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionError);
			mixer::interactive_reg_state_changed_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionStateChanged);
			mixer::interactive_reg_button_input_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionButtonInput);
			mixer::interactive_reg_coordinate_input_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionCoordinateInput);
			mixer::interactive_reg_participants_changed_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionParticipantsChanged);
			mixer::interactive_reg_unhandled_method_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnUnhandledMethod);

			mixer::interactive_get_scenes(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnEnumerateScenesForInit);

			SetInteractiveConnectionAuthState(EMixerLoginState::Logged_In);
		}
		else
		{
			SetInteractiveConnectionAuthState(EMixerLoginState::Not_Logged_In);
		}
	}

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

	TSharedPtr<FMixerRemoteUser> ButtonUser = InteractiveModule.GetCachedUser(ParticipantGuid);

	FMixerButtonPropertiesCached* CachedProps = InteractiveModule.GetButton(FName(Input->control.id));
	if (CachedProps != nullptr)
	{
		FMixerButtonEventDetails ButtonEventDetails;
		ButtonEventDetails.Pressed = Input->action == mixer::down;
		ButtonEventDetails.SparkCost = CachedProps->Desc.SparkCost;
		if (ButtonEventDetails.Pressed)
		{
			CachedProps->State.DownCount += 1;
			if (InteractiveModule.CachePerParticipantState())
			{
				CachedProps->HoldingParticipants.Add(ButtonUser->Id);
				CachedProps->State.PressCount = CachedProps->HoldingParticipants.Num();
			}
		}
		else
		{
			CachedProps->State.UpCount += 1;
			if (InteractiveModule.CachePerParticipantState())
			{
				CachedProps->HoldingParticipants.Remove(ButtonUser->Id);
				CachedProps->State.PressCount = CachedProps->HoldingParticipants.Num();
			}
		}

		InteractiveModule.OnButtonEvent().Broadcast(Input->control.id, ButtonUser, ButtonEventDetails);
	}
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

	TSharedPtr<FMixerRemoteUser> StickUser = InteractiveModule.GetCachedUser(ParticipantGuid);
	if (InteractiveModule.CachePerParticipantState())
	{
		FMixerStickPropertiesCached* CachedProps = InteractiveModule.GetStick(FName(Input->control.id));
		if (CachedProps != nullptr)
		{
			if (Input->x != 0 || Input->y != 0)
			{
				CachedProps->State.Axes *= CachedProps->PerParticipantStickValue.Num();
				FVector2D& PerUserStickValue = CachedProps->PerParticipantStickValue.FindOrAdd(StickUser->Id);
				CachedProps->State.Axes -= PerUserStickValue;
				PerUserStickValue = FVector2D(Input->x, Input->y);
				CachedProps->State.Axes += PerUserStickValue;
				CachedProps->State.Axes /= CachedProps->PerParticipantStickValue.Num();
			}
			else
			{
				FVector2D* OldPerUserStickValue = CachedProps->PerParticipantStickValue.Find(StickUser->Id);
				if (OldPerUserStickValue != nullptr)
				{
					CachedProps->State.Axes *= CachedProps->PerParticipantStickValue.Num();
					CachedProps->State.Axes -= *OldPerUserStickValue;
					CachedProps->PerParticipantStickValue.Remove(StickUser->Id);
					if (CachedProps->PerParticipantStickValue.Num() > 0)
					{
						CachedProps->State.Axes /= CachedProps->PerParticipantStickValue.Num();
					}
					else
					{
						CachedProps->State.Axes = FVector2D(0, 0);
					}
				}
			}
		}
	}

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
			TSharedPtr<FMixerRemoteUser> CachedParticipant = MakeShared<FMixerRemoteUser>();
			CachedParticipant->Id = Participant->userId;
			CachedParticipant->SessionGuid = SessionGuid;
			CachedParticipant->Name = UTF8_TO_TCHAR(Participant->userName);
			CachedParticipant->Level = Participant->level;
			CachedParticipant->Group = Participant->groupId;
			CachedParticipant->InputEnabled = !Participant->disabled;
			// Timestamps are in ms since January 1 1970
			CachedParticipant->ConnectedAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Participant->connectedAtMs / 1000.0));
			CachedParticipant->InputAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Participant->lastInputAtMs / 1000.0));

			InteractiveModule.AddUser(CachedParticipant);
		}
		break;

	case mixer::participant_leave:
		InteractiveModule.RemoveUser(SessionGuid);
		break;

	case mixer::participant_update:
		{
			TSharedPtr<FMixerRemoteUser> CachedParticipant = InteractiveModule.GetCachedUser(SessionGuid);
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

void FMixerInteractivityModule_InteractiveCpp2::OnUnhandledMethod(void* Context, mixer::interactive_session Session, const char* MethodJson, size_t MethodJsonLength)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(FString(UTF8_TO_TCHAR(MethodJson)));
	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		FString Method;
		if (JsonObject->TryGetStringField(TEXT("method"), Method))
		{
			const TSharedPtr<FJsonObject> *ParamsObject;
			if (JsonObject->TryGetObjectField(TEXT("params"), ParamsObject))
			{
				if (Method == TEXT("giveInput"))
				{
					InteractiveModule.HandleCustomControlInputMessage(ParamsObject->Get());
				}
				else if (Method == TEXT("onControlUpdate"))
				{
					InteractiveModule.HandleControlUpdateMessage(ParamsObject->Get());
				}
				else
				{
					InteractiveModule.OnCustomMethodCall().Broadcast(*Method, ParamsObject->ToSharedRef());
				}
			}
		}
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateForGetCurrentScene(void* Context, mixer::interactive_session Session, mixer::interactive_group* Group)
{
	FGetCurrentSceneEnumContext* GetSceneContext = static_cast<FGetCurrentSceneEnumContext*>(Context);
	if (GetSceneContext->GroupName == Group->id)
	{
		GetSceneContext->OutSceneName = Group->sceneId;
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateScenesForInit(void* Context, mixer::interactive_session Session, mixer::interactive_scene* Scene)
{
	mixer::interactive_set_session_context(Session, Scene);
	mixer::interactive_scene_get_controls(Session, Scene->id, &FMixerInteractivityModule_InteractiveCpp2::OnEnumerateControlsForInit);
	mixer::interactive_set_session_context(Session, nullptr);
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateControlsForInit(void* Context, mixer::interactive_session Session, mixer::interactive_control* Control)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());
	mixer::interactive_scene* Scene = static_cast<mixer::interactive_scene*>(Context);

	if (FPlatformString::Strcmp(Control->kind, "button") == 0)
	{
		FMixerButtonPropertiesCached CachedProps;

		GetControlPropertyHelper(Session, Control->id, "cost", CachedProps.Desc.SparkCost);
		GetControlPropertyHelper(Session, Control->id, "text", CachedProps.Desc.ButtonText);
		GetControlPropertyHelper(Session, Control->id, "tooltip", CachedProps.Desc.HelpText);

		CachedProps.State.DownCount = 0;
		CachedProps.State.UpCount = 0;
		CachedProps.State.PressCount = 0;
		CachedProps.State.Enabled = true;
		CachedProps.State.RemainingCooldown = FTimespan::Zero();
		CachedProps.State.Progress = 0.0f;

		CachedProps.SceneId = Scene->id;

		InteractiveModule.AddButton(FName(Control->id), CachedProps);
	}
	else if (FPlatformString::Strcmp(Control->kind, "joystick") == 0)
	{
		FMixerStickPropertiesCached CachedProps;
		CachedProps.State.Enabled = true;

		InteractiveModule.AddStick(FName(Control->id), CachedProps);
	}
	else if (FPlatformString::Strcmp(Control->kind, "label") == 0)
	{
		FMixerLabelPropertiesCached CachedProps;
		GetControlPropertyHelper(Session, Control->id, "text", CachedProps.Desc.Text);
		GetControlPropertyHelper(Session, Control->id, "textSize", CachedProps.Desc.TextSize);
		GetControlPropertyHelper(Session, Control->id, "underline", CachedProps.Desc.Underline);
		GetControlPropertyHelper(Session, Control->id, "bold", CachedProps.Desc.Bold);
		GetControlPropertyHelper(Session, Control->id, "italic", CachedProps.Desc.Italic);

		CachedProps.SceneId = Scene->id;

		InteractiveModule.AddLabel(FName(Control->id), CachedProps);
	}
	else if (FPlatformString::Strcmp(Control->kind, "textbox") == 0)
	{
		FMixerTextboxPropertiesCached Textbox;
		GetControlPropertyHelper(Session, Control->id, "placeholder", Textbox.Desc.Placeholder);
		GetControlPropertyHelper(Session, Control->id, "cost", Textbox.Desc.SparkCost);
		GetControlPropertyHelper(Session, Control->id, "hasSubmit", Textbox.Desc.HasSubmit);
		GetControlPropertyHelper(Session, Control->id, "multiline", Textbox.Desc.Multiline);
		GetControlPropertyHelper(Session, Control->id, "submitText", Textbox.Desc.SubmitText);

		InteractiveModule.AddTextbox(FName(Control->id), Textbox);
	}
}

#endif

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
int32 MixerInteractiveCpp2LinkerHelper;