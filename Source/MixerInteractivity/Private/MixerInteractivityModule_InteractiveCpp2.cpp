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
#include "MixerJsonHelpers.h"
#include "Containers/StringConv.h"
#include "Async/Async.h"

IMPLEMENT_MODULE(FMixerInteractivityModule_InteractiveCpp2, MixerInteractivity);

namespace
{
	bool GetControlPropertyHelper(interactive_session Session, const char* ControlName, const char *PropertyName, FString& Result)
	{
		size_t RequiredSize = 0;
		TArray<char> Utf8String;
		if (interactive_control_get_property_string(Session, ControlName, PropertyName, nullptr, &RequiredSize) != MIXER_ERROR_BUFFER_SIZE)
		{
			return false;
		}

		Utf8String.AddUninitialized(RequiredSize);
		if (interactive_control_get_property_string(Session, ControlName, PropertyName, Utf8String.GetData(), &RequiredSize) != MIXER_OK)
		{
			return false;
		}

		Result = UTF8_TO_TCHAR(Utf8String.GetData());
		return true;
	}

	bool GetControlPropertyHelper(interactive_session Session, const char* ControlName, const char *PropertyName, FText& Result)
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

	bool GetControlPropertyHelper(interactive_session Session, const char* ControlName, const char *PropertyName, float& Result)
	{
		return interactive_control_get_property_float(Session, ControlName, PropertyName, &Result) == MIXER_OK;
	}

	bool GetControlPropertyHelper(interactive_session Session, const char* ControlName, const char *PropertyName, bool& Result)
	{
		return interactive_control_get_property_bool(Session, ControlName, PropertyName, &Result) == MIXER_OK;
	}

	bool GetControlPropertyHelper(interactive_session Session, const char* ControlName, const char *PropertyName, int64& Result)
	{
		return interactive_control_get_property_int64(Session, ControlName, PropertyName, &Result) == MIXER_OK;
	}

	bool GetControlPropertyHelper(interactive_session Session, const char* ControlName, const char *PropertyName, uint32& Result)
	{
		int SignedResult;
		if (interactive_control_get_property_int(Session, ControlName, PropertyName, &SignedResult) != MIXER_OK)
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
		if (interactive_set_ready(InteractiveSession, true) == MIXER_OK)
		{
			SetInteractivityState(EMixerInteractivityState::Interactivity_Starting);
		}
	}
}

void FMixerInteractivityModule_InteractiveCpp2::StopInteractivity()
{
	if (InteractiveSession != nullptr)
	{
		if (interactive_set_ready(InteractiveSession, false) == MIXER_OK)
		{
			SetInteractivityState(EMixerInteractivityState::Interactivity_Stopping);
		}
	}
}

void FMixerInteractivityModule_InteractiveCpp2::SetCurrentScene(FName Scene, FName GroupName)
{
	if (InteractiveSession != nullptr)
	{
		// Special case - 'default' is used all over the place as a name, but with 'D'
		const ANSICHAR* ActualGroupName = GroupName != NAME_None && GroupName != NAME_DefaultMixerParticipantGroup ? GroupName.GetPlainANSIString() : "default";
		const ANSICHAR* ActualSceneName = Scene != NAME_None && Scene != NAME_DefaultMixerParticipantGroup ? Scene.GetPlainANSIString() : "default";
		interactive_group_set_scene(InteractiveSession, ActualGroupName, ActualSceneName);
	}
}

FName FMixerInteractivityModule_InteractiveCpp2::GetCurrentScene(FName GroupName)
{
	FGetCurrentSceneEnumContext Context;
	Context.GroupName = GroupName != NAME_None ? GroupName : NAME_DefaultMixerParticipantGroup;
	Context.OutSceneName = NAME_None;
	if (InteractiveSession != nullptr)
	{
		interactive_set_session_context(InteractiveSession, &Context);
		interactive_get_groups(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnEnumerateForGetCurrentScene);
		interactive_set_session_context(InteractiveSession, nullptr);
	}
	return Context.OutSceneName;
}

void FMixerInteractivityModule_InteractiveCpp2::TriggerButtonCooldown(FName Button, FTimespan CooldownTime)
{
	if (InteractiveSession != nullptr)
	{
		interactive_control_trigger_cooldown(InteractiveSession, Button.GetPlainANSIString(), static_cast<uint32>(CooldownTime.GetTotalMilliseconds()));
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::CreateGroup(FName GroupName, FName InitialScene)
{
	if (InteractiveSession == nullptr)
	{
		return false;
	}

	// Default group is created automatically, don't send this to the service.
	if (GroupName == NAME_None || GroupName == NAME_DefaultMixerParticipantGroup)
	{
		return false;
	}

	// Special case - 'default' is used all over the place as a name, but with 'D'
	const ANSICHAR* ActualSceneName = InitialScene != NAME_None && InitialScene != NAME_DefaultMixerParticipantGroup ? InitialScene.GetPlainANSIString() : "default";
	return interactive_create_group(InteractiveSession, GroupName.GetPlainANSIString(), ActualSceneName) == MIXER_OK;
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

	return interactive_set_participant_group(
		InteractiveSession,
		TCHAR_TO_UTF8(*Participant->SessionGuid.ToString(EGuidFormats::DigitsWithHyphens).ToLower()),
		GroupName.GetPlainANSIString()) == MIXER_OK;
}

void FMixerInteractivityModule_InteractiveCpp2::CaptureSparkTransaction(const FString& TransactionId)
{
	if (InteractiveSession != nullptr)
	{
		interactive_capture_transaction(InteractiveSession, TCHAR_TO_UTF8(*TransactionId));
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
		interactive_send_method(InteractiveSession, TCHAR_TO_UTF8(*MethodName), TCHAR_TO_UTF8(*SerializedParams), true, &MessageId);
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

	ConnectOperation = Async<interactive_session>(EAsyncExecution::ThreadPool,
		[]() -> interactive_session
	{
		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();

		interactive_session Session;
		int32 ConnectResult = interactive_open_session(
			TCHAR_TO_UTF8(*UserSettings->GetAuthZHeaderValue()),
			TCHAR_TO_UTF8(*FString::FromInt(Settings->GameVersionId)),
			TCHAR_TO_UTF8(*Settings->ShareCode),
			false,
			&Session);
		if (ConnectResult == MIXER_OK)
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
		interactive_close_session(InteractiveSession);
		EndSession();
		InteractiveSession = nullptr;
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::Tick(float DeltaTime)
{
	FMixerInteractivityModule_WithSessionState::Tick(DeltaTime);

	if (InteractiveSession != nullptr)
	{
		interactive_run(InteractiveSession, 10);
	}
	else if (ConnectOperation.IsReady())
	{
		InteractiveSession = ConnectOperation.Get();
		ConnectOperation = TFuture<interactive_session>();

		if (InteractiveSession != nullptr)
		{
			const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();

			StartSession(Settings->bPerParticipantStateCaching);

			interactive_register_error_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionError);
			interactive_register_state_changed_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionStateChanged);
			interactive_register_input_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionInput);
			interactive_register_participants_changed_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionParticipantsChanged);
			interactive_register_unhandled_method_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnUnhandledMethod);
			interactive_register_transaction_complete_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnTransactionComplete);

			interactive_get_scenes(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnEnumerateScenesForInit);

			SetInteractiveConnectionAuthState(EMixerLoginState::Logged_In);
		}
		else
		{
			SetInteractiveConnectionAuthState(EMixerLoginState::Not_Logged_In);
		}
	}

	return true;
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionStateChanged(void* Context, interactive_session Session, interactive_state PreviousState, interactive_state NewState)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());
	switch (NewState)
	{
	case not_ready:
		InteractiveModule.SetInteractivityState(EMixerInteractivityState::Not_Interactive);
		break;

	case ready:
		InteractiveModule.SetInteractivityState(EMixerInteractivityState::Interactive);
		break;

	case disconnected:
	default:
		InteractiveModule.SetInteractivityState(EMixerInteractivityState::Not_Interactive);
		InteractiveModule.SetInteractiveConnectionAuthState(EMixerLoginState::Not_Logged_In);
		break;
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionError(void* Context, interactive_session Session, int ErrorCode, const char* ErrorMessage, size_t ErrorMessageLength)
{
	UE_LOG(LogMixerInteractivity, Error, TEXT("Session error %d: %hs"), ErrorCode, ErrorMessage);
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionInput(void* Context, interactive_session Session, const interactive_input* Input)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());

	FGuid ParticipantGuid;
	if (!FGuid::Parse(Input->participantId, ParticipantGuid))
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("Participant id %hs was not in the expected format (guid)"), Input->participantId);
		return;
	}

	TSharedPtr<FMixerRemoteUser> ButtonUser = InteractiveModule.GetCachedUser(ParticipantGuid);

	switch (Input->type)
	{
	case input_type_click:
		InteractiveModule.OnSessionButtonInput(ButtonUser, Input);
		break;

	case input_type_move:
		InteractiveModule.OnSessionCoordinateInput(ButtonUser, Input);
		break;

	case input_type_custom:
	default:
		InteractiveModule.OnSessionCustomInput(ButtonUser, Input);
		break;
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionButtonInput(TSharedPtr<const FMixerRemoteUser> User, const interactive_input* Input)
{
	FMixerButtonPropertiesCached* CachedProps = GetButton(FName(Input->control.id));
	if (CachedProps != nullptr)
	{
		FMixerButtonEventDetails ButtonEventDetails;
		ButtonEventDetails.Pressed = Input->buttonData.action == interactive_button_action_down;
		ButtonEventDetails.TransactionId = Input->transactionId;
		ButtonEventDetails.SparkCost = CachedProps->Desc.SparkCost;
		if (ButtonEventDetails.Pressed)
		{
			CachedProps->State.DownCount += 1;
			if (CachePerParticipantState())
			{
				CachedProps->HoldingParticipants.Add(User->Id);
				CachedProps->State.PressCount = CachedProps->HoldingParticipants.Num();
			}
		}
		else
		{
			CachedProps->State.UpCount += 1;
			if (CachePerParticipantState())
			{
				CachedProps->HoldingParticipants.Remove(User->Id);
				CachedProps->State.PressCount = CachedProps->HoldingParticipants.Num();
			}
		}

		OnButtonEvent().Broadcast(Input->control.id, User, ButtonEventDetails);
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionCoordinateInput(TSharedPtr<const FMixerRemoteUser> User, const interactive_input* Input)
{
	if (CachePerParticipantState())
	{
		FMixerStickPropertiesCached* CachedProps = GetStick(FName(Input->control.id));
		if (CachedProps != nullptr)
		{
			if (Input->coordinateData.x != 0 || Input->coordinateData.y != 0)
			{
				CachedProps->State.Axes *= CachedProps->PerParticipantStickValue.Num();
				FVector2D& PerUserStickValue = CachedProps->PerParticipantStickValue.FindOrAdd(User->Id);
				CachedProps->State.Axes -= PerUserStickValue;
				PerUserStickValue = FVector2D(Input->coordinateData.x, Input->coordinateData.y);
				CachedProps->State.Axes += PerUserStickValue;
				CachedProps->State.Axes /= CachedProps->PerParticipantStickValue.Num();
			}
			else
			{
				FVector2D* OldPerUserStickValue = CachedProps->PerParticipantStickValue.Find(User->Id);
				if (OldPerUserStickValue != nullptr)
				{
					CachedProps->State.Axes *= CachedProps->PerParticipantStickValue.Num();
					CachedProps->State.Axes -= *OldPerUserStickValue;
					CachedProps->PerParticipantStickValue.Remove(User->Id);
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

	OnStickEvent().Broadcast(Input->control.id, User, FVector2D(Input->coordinateData.x, Input->coordinateData.y));
}

bool FMixerInteractivityModule_InteractiveCpp2::OnSessionCustomInput(TSharedPtr<const FMixerRemoteUser> User, const interactive_input* Input)
{
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(FString(UTF8_TO_TCHAR(Input->jsonData)));
	TSharedPtr<FJsonObject> FullParamsJson;
	if (!FJsonSerializer::Deserialize(JsonReader, FullParamsJson) && FullParamsJson.IsValid())
	{
		return false;
	}

	// Alias so macros work
	const FJsonObject* JsonObj = FullParamsJson.Get();
	GET_JSON_OBJECT_RETURN_FAILURE(Input, InputObj);

	JsonObj = InputObj->Get();

	GET_JSON_STRING_RETURN_FAILURE(ControlId, ControlIdRaw);
	GET_JSON_STRING_RETURN_FAILURE(Event, EventType);

	FName ControlId = *ControlIdRaw;
	bool bHandled = false;
	if (EventType == MixerStringConstants::EventTypes::Submit)
	{
		FMixerTextboxPropertiesCached* Textbox = GetTextbox(ControlId);
		if (Textbox != nullptr)
		{
			GET_JSON_STRING_RETURN_FAILURE(Value, Value);

			FMixerTextboxEventDetails EventDetails;
			EventDetails.SubmittedText = FText::FromString(Value);
			if (Textbox->Desc.SparkCost > 0)
			{
				if (FullParamsJson->TryGetStringField(MixerStringConstants::FieldNames::TransactionId, EventDetails.TransactionId))
				{
					EventDetails.SparkCost = Textbox->Desc.SparkCost;
				}
			}
			else
			{
				EventDetails.SparkCost = 0;
			}

			OnTextboxSubmitEvent().Broadcast(ControlId, User, EventDetails);
			bHandled = true;
		}
	}

	if (!bHandled)
	{
		OnCustomControlInput().Broadcast(ControlId, *EventType, User, InputObj->ToSharedRef());
	}

	return true;
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionParticipantsChanged(void* Context, interactive_session Session, interactive_participant_action Action, const interactive_participant* Participant)
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
	case participant_join:
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

	case participant_leave:
		InteractiveModule.RemoveUser(SessionGuid);
		break;

	case participant_update:
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

void FMixerInteractivityModule_InteractiveCpp2::OnUnhandledMethod(void* Context, interactive_session Session, const char* MethodJson, size_t MethodJsonLength)
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
				if (Method == TEXT("onControlUpdate"))
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

void FMixerInteractivityModule_InteractiveCpp2::OnTransactionComplete(void *Context, interactive_session Session, const char* TransactionId, size_t TransactionIdLength, unsigned int ErrorCode, const char* ErrorMessage, size_t ErrorMessageLength)
{
	if (ErrorCode != MIXER_OK)
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("Transaction completion error %d: %hs"), ErrorCode, ErrorMessage);
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateForGetCurrentScene(void* Context, interactive_session Session, interactive_group* Group)
{
	FGetCurrentSceneEnumContext* GetSceneContext = static_cast<FGetCurrentSceneEnumContext*>(Context);
	if (GetSceneContext->GroupName == Group->id)
	{
		GetSceneContext->OutSceneName = Group->sceneId;
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateScenesForInit(void* Context, interactive_session Session, interactive_scene* Scene)
{
	interactive_set_session_context(Session, Scene);
	interactive_scene_get_controls(Session, Scene->id, &FMixerInteractivityModule_InteractiveCpp2::OnEnumerateControlsForInit);
	interactive_set_session_context(Session, nullptr);
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateControlsForInit(void* Context, interactive_session Session, interactive_control* Control)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());
	interactive_scene* Scene = static_cast<interactive_scene*>(Context);

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