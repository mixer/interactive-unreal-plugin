//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MixerInteractivityModule_UE.h"

#if MIXER_BACKEND_UE
#include "MixerInteractivityLog.h"
#include "MixerInteractivitySettings.h"
#include "MixerInteractivityUserSettings.h"
#include "MixerInteractivityBlueprintLibrary.h"
#include "MixerJsonHelpers.h"
#include "MixerInteractivityJsonTypes.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "WebsocketsModule.h"
#include "IWebSocket.h"

#if !WITH_WEBSOCKETS
#error "UE backend requires UE websockets"
#endif

IMPLEMENT_MODULE(FMixerInteractivityModule_UE, MixerInteractivity);

struct FMixerReadyMessageParams : public FJsonSerializable
{
public:
	bool bReady;
public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("isReady", bReady);
	END_JSON_SERIALIZER
};

struct FMixerUpdateGroupMessageParamsEntry : public FJsonSerializable
{
public:
	FString GroupId;
	FString SceneId;
public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("groupID", GroupId);
		JSON_SERIALIZE("sceneID", SceneId);
	END_JSON_SERIALIZER
};

struct FMixerUpdateGroupMessageParams : public FJsonSerializable
{
public:
	TArray<FMixerUpdateGroupMessageParamsEntry> Groups;
public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("groups", Groups, FMixerUpdateGroupMessageParamsEntry);
	END_JSON_SERIALIZER
};

struct FMixerUpdateParticipantGroupParamsEntry : public FJsonSerializable
{
public:
	FString ParticipantSessionGuid;
	FString GroupId;
public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("sessionID", ParticipantSessionGuid);
		JSON_SERIALIZE("groupID", GroupId);
	END_JSON_SERIALIZER
};

struct FMixerUpdateParticipantGroupParams : public FJsonSerializable
{
public:
	TArray<FMixerUpdateParticipantGroupParamsEntry> Participants;
public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("participants", Participants, FMixerUpdateParticipantGroupParamsEntry);
	END_JSON_SERIALIZER
};

struct FMixerCaptureTransactionParams : public FJsonSerializable
{
public:
	FString TransactionId;
public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("transactionID", TransactionId);
	END_JSON_SERIALIZER
};


FMixerInteractivityModule_UE::FMixerInteractivityModule_UE()
	: TMixerWebSocketOwnerBase<FMixerInteractivityModule_UE>(MixerStringConstants::MessageTypes::Method, MixerStringConstants::FieldNames::Method, MixerStringConstants::FieldNames::Params)
{
}

void FMixerInteractivityModule_UE::StartInteractivity()
{
	switch (GetInteractivityState())
	{
	case EMixerInteractivityState::Interactivity_Stopping:
	case EMixerInteractivityState::Not_Interactive:
		if (GetInteractiveConnectionAuthState() == EMixerLoginState::Logged_In)
		{
			FMixerReadyMessageParams Params;
			Params.bReady = true;
			SendMethodMessageObjectParams(MixerStringConstants::MethodNames::Ready, nullptr, Params);
			SetInteractivityState(EMixerInteractivityState::Interactivity_Starting);
		}
		break;

	default:
		break;
	}
}

void FMixerInteractivityModule_UE::StopInteractivity()
{
	switch (GetInteractivityState())
	{
	case EMixerInteractivityState::Interactivity_Starting:
	case EMixerInteractivityState::Interactive:
		if (GetInteractiveConnectionAuthState() == EMixerLoginState::Logged_In)
		{
			FMixerReadyMessageParams Params;
			Params.bReady = false;
			SendMethodMessageObjectParams(MixerStringConstants::MethodNames::Ready, nullptr, Params);
			SetInteractivityState(EMixerInteractivityState::Interactivity_Stopping);
		}
		break;

	default:
		break;
	}
}

void FMixerInteractivityModule_UE::SetCurrentScene(FName Scene, FName GroupName)
{
	CreateOrUpdateGroup(MixerStringConstants::MethodNames::UpdateGroups, Scene, GroupName);
}

FName FMixerInteractivityModule_UE::GetCurrentScene(FName GroupName)
{
	FName FindGroup = GroupName != NAME_None ? GroupName : NAME_DefaultMixerParticipantGroup;
	FName* Scene = ScenesByGroup.Find(FindGroup);
	return Scene != nullptr ? *Scene : NAME_None;
}

bool FMixerInteractivityModule_UE::CreateGroup(FName GroupName, FName InitialScene)
{
	// Default group is created automatically, don't send this to the service.
	if (GroupName == NAME_None || GroupName == NAME_DefaultMixerParticipantGroup)
	{
		return false;
	}

	return CreateOrUpdateGroup(MixerStringConstants::MethodNames::CreateGroups, InitialScene, GroupName);
}

bool FMixerInteractivityModule_UE::MoveParticipantToGroup(FName GroupName, uint32 ParticipantId)
{
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Logged_In)
	{
		return false;
	}

	TSharedPtr<FMixerRemoteUser> ExistingUser = GetCachedUser(ParticipantId);
	if (!ExistingUser.IsValid())
	{
		return false;
	}

	FMixerUpdateParticipantGroupParamsEntry ParamEntry;
	ParamEntry.ParticipantSessionGuid = ExistingUser->SessionGuid.ToString(EGuidFormats::DigitsWithHyphens).ToLower();
	// Special case - 'default' is used all over the place as a name, but with 'D'
	ParamEntry.GroupId = GroupName != NAME_DefaultMixerParticipantGroup ? GroupName.ToString() : TEXT("default");

	FMixerUpdateParticipantGroupParams Params;
	Params.Participants.Add(ParamEntry);
	SendMethodMessageObjectParams(MixerStringConstants::MethodNames::UpdateParticipants, nullptr, Params);

	return true;
}

void FMixerInteractivityModule_UE::CaptureSparkTransaction(const FString& TransactionId)
{
	if (GetInteractiveConnectionAuthState() == EMixerLoginState::Logged_In)
	{
		FMixerCaptureTransactionParams Params;
		Params.TransactionId = TransactionId;
		SendMethodMessageObjectParams(MixerStringConstants::MethodNames::Capture, nullptr, Params);
	}
}

void FMixerInteractivityModule_UE::CallRemoteMethod(const FString& MethodName, const TSharedRef<FJsonObject> MethodParams)
{
	SendMethodMessageObjectParams(MethodName, nullptr, MethodParams);
}

bool FMixerInteractivityModule_UE::StartInteractiveConnection()
{
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
	{
		return false;
	}

	Endpoints.Empty();
	TSharedRef<IHttpRequest> HostsRequest = FHttpModule::Get().CreateRequest();
	HostsRequest->SetVerb(TEXT("GET"));
	HostsRequest->SetURL(TEXT("https://mixer.com/api/v1/interactive/hosts"));

	HostsRequest->OnProcessRequestComplete().BindRaw(this, &FMixerInteractivityModule_UE::OnHostsRequestComplete);
	if (!HostsRequest->ProcessRequest())
	{
		return false;
	}

	SetInteractiveConnectionAuthState(EMixerLoginState::Logging_In);
	return true;
}

void FMixerInteractivityModule_UE::StopInteractiveConnection()
{
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
	{
		StopInteractivity();
		SetInteractiveConnectionAuthState(EMixerLoginState::Not_Logged_In);
		SetInteractivityState(EMixerInteractivityState::Not_Interactive);
		CleanupConnection();
		Endpoints.Empty();
		EndSession();
	}
}

void FMixerInteractivityModule_UE::OnHostsRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (bSucceeded && HttpResponse.IsValid())
	{
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			FString ResponseStr = HttpResponse->GetContentAsString();
			TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ResponseStr);
			TSharedPtr<FJsonValue> JsonPayload;
			if (FJsonSerializer::Deserialize(JsonReader, JsonPayload) && JsonPayload.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* JsonArray;
				if (JsonPayload->TryGetArray(JsonArray))
				{
					for (const TSharedPtr<FJsonValue>& HostElem : *JsonArray)
					{
						const TSharedPtr<FJsonObject> AddressObject = HostElem->AsObject();
						if (AddressObject.IsValid())
						{
							Endpoints.Add(AddressObject->GetStringField(TEXT("address")));
						}
					}
				}
			}
		}
	}

	OpenWebSocket();
}

void FMixerInteractivityModule_UE::OpenWebSocket()
{
	if (Endpoints.Num() == 0)
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("Interactive connection failed - no more endpoints available."));
		SetInteractiveConnectionAuthState(EMixerLoginState::Not_Logged_In);
		return;
	}

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();

	StartSession(Settings->bPerParticipantStateCaching);

	const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
	TMap<FString, FString> UpgradeHeaders;
	UpgradeHeaders.Add(TEXT("Authorization"), UserSettings->GetAuthZHeaderValue());
	UpgradeHeaders.Add(TEXT("X-Interactive-Version"), FString::FromInt(Settings->GameVersionId));
	UpgradeHeaders.Add(TEXT("X-Protocol-Version"), TEXT("2.0"));

	if (!Settings->ShareCode.IsEmpty())
	{
		UpgradeHeaders.Add(TEXT("X-Interactive-Sharecode"), Settings->ShareCode);
	}

	FString EndpointToUse = Endpoints[0];
	UE_LOG(LogMixerInteractivity, Verbose, TEXT("Opening web socket to %s for interactivity"), *EndpointToUse);

	Endpoints.RemoveAtSwap(0);
	InitConnection(EndpointToUse, UpgradeHeaders);
}

bool FMixerInteractivityModule_UE::CreateOrUpdateGroup(const FString& MethodName, FName Scene, FName GroupName)
{
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Logged_In)
	{
		return false;
	}

	FMixerUpdateGroupMessageParamsEntry ParamEntry;
	ParamEntry.GroupId = GroupName != NAME_None && GroupName != NAME_DefaultMixerParticipantGroup ? GroupName.ToString() : TEXT("default");
	ParamEntry.SceneId = Scene != NAME_None && Scene != NAME_DefaultMixerParticipantGroup ? Scene.ToString() : TEXT("default");
	FMixerUpdateGroupMessageParams Params;
	Params.Groups.Add(ParamEntry);

	SendMethodMessageObjectParams(MethodName, nullptr, Params);
	return true;
}

void FMixerInteractivityModule_UE::HandleSocketConnected()
{
	// No real action here - we'll wait for a hello
}

void FMixerInteractivityModule_UE::HandleSocketConnectionError()
{
	// Retry if we still have endpoints available
	OpenWebSocket();
}

void FMixerInteractivityModule_UE::HandleSocketClosed( bool bWasClean)
{
	// Attempt to reconnect
	OpenWebSocket();
}

void FMixerInteractivityModule_UE::RegisterAllServerMessageHandlers()
{
	RegisterServerMessageHandler(TEXT("hello"), &FMixerInteractivityModule_UE::HandleHello);
	RegisterServerMessageHandler(TEXT("giveInput"), &FMixerInteractivityModule_UE::HandleGiveInput);
	RegisterServerMessageHandler(TEXT("onParticipantJoin"), &FMixerInteractivityModule_UE::HandleParticipantJoin);
	RegisterServerMessageHandler(TEXT("onParticipantLeave"), &FMixerInteractivityModule_UE::HandleParticipantLeave);
	RegisterServerMessageHandler(TEXT("onParticipantUpdate"), &FMixerInteractivityModule_UE::HandleParticipantUpdate);
	RegisterServerMessageHandler(TEXT("onReady"), &FMixerInteractivityModule_UE::HandleReadyStateChange);
	RegisterServerMessageHandler(TEXT("onControlUpdate"), &FMixerInteractivityModule_UE::HandleControlUpdateMessage);
	RegisterServerMessageHandler(TEXT("onGroupCreate"), &FMixerInteractivityModule_UE::HandleGroupCreate);
	RegisterServerMessageHandler(TEXT("onGroupUpdate"), &FMixerInteractivityModule_UE::HandleGroupUpdate);
	RegisterServerMessageHandler(TEXT("onGroupDelete"), &FMixerInteractivityModule_UE::HandleGroupDelete);
}

bool FMixerInteractivityModule_UE::OnUnhandledServerMessage(const FString& MessageType, const TSharedPtr<FJsonObject> Params)
{
	OnCustomMethodCall().Broadcast(*MessageType, Params);
	return true;
}

bool FMixerInteractivityModule_UE::HandleHello(FJsonObject* JsonObj)
{
	SendMethodMessageNoParams(MixerStringConstants::MethodNames::GetScenes, &FMixerInteractivityModule_UE::HandleGetScenesReply);
	return true;
}

bool FMixerInteractivityModule_UE::HandleGiveInput(FJsonObject* JsonObj)
{
	GET_JSON_STRING_RETURN_FAILURE(ParticipantId, ParticipantGuidString);

	FGuid ParticipantGuid;
	if (!FGuid::Parse(ParticipantGuidString, ParticipantGuid))
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("%s field %s for input event was not in the expected format (guid)"), *MixerStringConstants::FieldNames::ParticipantId, *ParticipantGuidString);
		return false;
	}

	GET_JSON_OBJECT_RETURN_FAILURE(Input, InputObj);

	TSharedPtr<FMixerRemoteUser> RemoteUser = GetCachedUser(ParticipantGuid);
	return HandleGiveInput(RemoteUser, JsonObj, InputObj->ToSharedRef());
}

bool FMixerInteractivityModule_UE::HandleParticipantJoin(FJsonObject* JsonObj)
{
	return HandleParticipantEvent(JsonObj, EMixerInteractivityParticipantState::Joined);
}

bool FMixerInteractivityModule_UE::HandleParticipantLeave(FJsonObject* JsonObj)
{
	return HandleParticipantEvent(JsonObj, EMixerInteractivityParticipantState::Left);
}

bool FMixerInteractivityModule_UE::HandleParticipantUpdate(FJsonObject* JsonObj)
{
	return HandleParticipantEvent(JsonObj, EMixerInteractivityParticipantState::Input_Disabled);
}

bool FMixerInteractivityModule_UE::HandleReadyStateChange(FJsonObject* JsonObj)
{
	GET_JSON_BOOL_RETURN_FAILURE(IsReady, bIsReady);
	SetInteractivityState(bIsReady ? EMixerInteractivityState::Interactive : EMixerInteractivityState::Not_Interactive);
	return true;
}

bool FMixerInteractivityModule_UE::HandleGroupCreate(FJsonObject* JsonObj)
{
	GET_JSON_ARRAY_RETURN_FAILURE(Groups, Groups);

	for (const TSharedPtr<FJsonValue>& Group : *Groups)
	{
		FString GroupId;
		FString SceneId;
		TSharedPtr<FJsonObject> GroupObj = Group->AsObject();
		if (GroupObj.IsValid()
			&& GroupObj->TryGetStringField(MixerStringConstants::FieldNames::GroupId, GroupId)
			&& GroupObj->TryGetStringField(MixerStringConstants::FieldNames::SceneId, SceneId))
		{
			ScenesByGroup.Add(*GroupId, *SceneId);
		}
	}

	return true;
}

bool FMixerInteractivityModule_UE::HandleGroupUpdate(FJsonObject* JsonObj)
{
	GET_JSON_ARRAY_RETURN_FAILURE(Groups, Groups);
	for (const TSharedPtr<FJsonValue>& Group : *Groups)
	{
		FString GroupId;
		FString SceneId;
		TSharedPtr<FJsonObject> GroupObj = Group->AsObject();
		if (GroupObj.IsValid()
			&& GroupObj->TryGetStringField(MixerStringConstants::FieldNames::GroupId, GroupId)
			&& GroupObj->TryGetStringField(MixerStringConstants::FieldNames::SceneId, SceneId))
		{
			ScenesByGroup.FindChecked(*GroupId) = *SceneId;
		}
	}

	return true;
}

bool FMixerInteractivityModule_UE::HandleGroupDelete(FJsonObject* JsonObj)
{
	GET_JSON_STRING_RETURN_FAILURE(GroupId, GroupIdRaw);
	GET_JSON_STRING_RETURN_FAILURE(ReassignGroupId, ReassignGroupIdRaw);

	FName GroupId = *GroupIdRaw;
	FName ReassignGroupId = *ReassignGroupIdRaw;

	ScenesByGroup.Remove(GroupId);
	ReassignUsers(GroupId, ReassignGroupId);
	return true;
}

bool FMixerInteractivityModule_UE::HandleGetScenesReply(FJsonObject* JsonObj)
{
	SetInteractiveConnectionAuthState(EMixerLoginState::Logged_In);
	GET_JSON_OBJECT_RETURN_FAILURE(Result, Result);
	ParsePropertiesFromGetScenesResult(Result->Get());
	return true;
}

bool FMixerInteractivityModule_UE::HandleGiveInput(TSharedPtr<FMixerRemoteUser> Participant, FJsonObject* FullParamsJson, const TSharedRef<FJsonObject> InputObjJson)
{
	// Alias so macros work
	const FJsonObject* JsonObj = &InputObjJson.Get();

	GET_JSON_STRING_RETURN_FAILURE(ControlId, ControlIdRaw);
	GET_JSON_STRING_RETURN_FAILURE(Event, EventType);

	bool bHandled = false;
	FName ControlId = *ControlIdRaw;
	if (EventType == MixerStringConstants::EventTypes::MouseDown)
	{
		FMixerButtonPropertiesCached* ButtonProps = GetButton(ControlId);
		if (ButtonProps != nullptr)
		{
			FMixerButtonEventDetails EventDetails;
			EventDetails.Pressed = true;
			if (ButtonProps->Desc.SparkCost > 0)
			{
				FullParamsJson->TryGetStringField(MixerStringConstants::FieldNames::TransactionId, EventDetails.TransactionId);
				EventDetails.SparkCost = ButtonProps->Desc.SparkCost;
			}
			else
			{
				EventDetails.SparkCost = 0;
			}
			OnButtonEvent().Broadcast(ControlId, Participant, EventDetails);
			bHandled = true;
		}
	}
	else if (EventType == MixerStringConstants::EventTypes::MouseUp)
	{
		FMixerButtonPropertiesCached* ButtonProps = GetButton(ControlId);
		if (ButtonProps != nullptr)
		{
			FMixerButtonEventDetails EventDetails;
			EventDetails.Pressed = false;
			// Button mouseup doesn't support charging
			EventDetails.SparkCost = 0;

			OnButtonEvent().Broadcast(ControlId, Participant, EventDetails);
			bHandled = true;
		}
	}
	else if (EventType == MixerStringConstants::EventTypes::Move)
	{
		FMixerStickPropertiesCached* Stick = GetStick(ControlId);
		if (Stick != nullptr)
		{
			GET_JSON_DOUBLE_RETURN_FAILURE(X, X);
			GET_JSON_DOUBLE_RETURN_FAILURE(Y, Y);

			OnStickEvent().Broadcast(ControlId, Participant, FVector2D(static_cast<float>(X), static_cast<float>(Y)));
			bHandled = true;
		}
	}
	else if (EventType == MixerStringConstants::EventTypes::Submit)
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

			OnTextboxSubmitEvent().Broadcast(ControlId, Participant, EventDetails);
			bHandled = true;
		}
	}

	if (!bHandled)
	{
		OnCustomControlInput().Broadcast(ControlId, *EventType, Participant, InputObjJson);
	}

	return true;
}

bool FMixerInteractivityModule_UE::HandleParticipantEvent(FJsonObject* JsonObj, EMixerInteractivityParticipantState EventType)
{
	GET_JSON_ARRAY_RETURN_FAILURE(Participants, ChangingParticipants);

	bool bHandled = true;
	for (const TSharedPtr<FJsonValue>& Participant : *ChangingParticipants)
	{
		bHandled &= HandleSingleParticipantChange(Participant->AsObject().Get(), EventType);
	}
	return bHandled;
}

bool FMixerInteractivityModule_UE::HandleSingleParticipantChange(const FJsonObject* JsonObj, EMixerInteractivityParticipantState EventType)
{
	GET_JSON_STRING_RETURN_FAILURE(UserNameNoUnderscore, Username);
	GET_JSON_INT_RETURN_FAILURE(UserIdNoUnderscore, UserId);
	GET_JSON_INT_RETURN_FAILURE(Level, UserLevel);
	GET_JSON_DOUBLE_RETURN_FAILURE(LastInputAt, LastInputAtDouble);
	GET_JSON_DOUBLE_RETURN_FAILURE(ConnectedAt, ConnectedAtDouble);
	GET_JSON_STRING_RETURN_FAILURE(GroupId, GroupId);
	GET_JSON_STRING_RETURN_FAILURE(SessionId, SessionGuidString);

	FGuid SessionGuid;
	if (!FGuid::Parse(SessionGuidString, SessionGuid))
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("sessionID field %s for participant event was not in the expected format (guid)"), *SessionGuidString);
		return false;
	}

	TSharedPtr<FMixerRemoteUser> RemoteUser = GetCachedUser(UserId);
	bool bOldInputEnabled = false;
	bool bExistingUser = false;
	if (RemoteUser.IsValid())
	{
		bExistingUser = true;
		bOldInputEnabled = RemoteUser->InputEnabled;
	}
	else
	{
		RemoteUser = MakeShared<FMixerRemoteUser>();
		RemoteUser->Id = UserId;
		RemoteUser->SessionGuid = SessionGuid;
		RemoteUser->ConnectedAt = FDateTime::FromUnixTimestamp(static_cast<int64>(ConnectedAtDouble / 1000.0));

		if (EventType != EMixerInteractivityParticipantState::Left)
		{
			AddUser(RemoteUser);
		}
	}

	RemoteUser->Name = Username;
	RemoteUser->Level = UserLevel;
	RemoteUser->InputAt = FDateTime::FromUnixTimestamp(static_cast<int64>(LastInputAtDouble / 1000.0));
	RemoteUser->Group = *GroupId;

	if (EventType != EMixerInteractivityParticipantState::Input_Disabled || bOldInputEnabled != RemoteUser->InputEnabled)
	{
		OnParticipantStateChanged().Broadcast(RemoteUser, EventType);
	}

	if (bExistingUser && EventType == EMixerInteractivityParticipantState::Left)
	{
		RemoveUser(RemoteUser);
	}

	return true;
}

bool FMixerInteractivityModule_UE::ParsePropertiesFromGetScenesResult(FJsonObject *JsonObj)
{
	GET_JSON_ARRAY_RETURN_FAILURE(Scenes, Scenes);
	for (const TSharedPtr<FJsonValue>& Scene : *Scenes)
	{
		ParsePropertiesFromSingleScene(Scene->AsObject().Get());
	}

	return true;
}

bool FMixerInteractivityModule_UE::ParsePropertiesFromSingleScene(FJsonObject* JsonObj)
{
	GET_JSON_ARRAY_RETURN_FAILURE(Controls, Controls);
	GET_JSON_STRING_RETURN_FAILURE(SceneId, SceneIdRaw);

	FName SceneId = *SceneIdRaw;
	for (const TSharedPtr<FJsonValue>& Control : *Controls)
	{
		ParsePropertiesFromSingleControl(SceneId, Control->AsObject().ToSharedRef());
	}

	// Groups will be omitted if empty
	const TArray<TSharedPtr<FJsonValue>> *Groups;
	if (JsonObj->TryGetArrayField(MixerStringConstants::FieldNames::Groups, Groups))
	{
		for (const TSharedPtr<FJsonValue>& Group : *Groups)
		{
			FString GroupId;
			TSharedPtr<FJsonObject> GroupObj = Group->AsObject();
			if (GroupObj.IsValid() && GroupObj->TryGetStringField(MixerStringConstants::FieldNames::GroupId, GroupId))
			{
				ScenesByGroup.Add(*GroupId, SceneId);
			}
		}
	}

	return true;
}

bool FMixerInteractivityModule_UE::ParsePropertiesFromSingleControl(FName SceneId, TSharedRef<FJsonObject> JsonObj)
{
	GET_JSON_STRING_RETURN_FAILURE(Kind, ControlKind);
	GET_JSON_STRING_RETURN_FAILURE(ControlId, ControlId);

	if (ControlKind == FMixerInteractiveControl::ButtonKind)
	{
		FMixerButtonPropertiesCached Button;
		FString FieldValueScratch;
		JsonObj->TryGetStringField(MixerStringConstants::FieldNames::Text, FieldValueScratch);
		Button.Desc.ButtonText = FText::FromString(FieldValueScratch);
		JsonObj->TryGetStringField(MixerStringConstants::FieldNames::Tooltip, FieldValueScratch);
		Button.Desc.HelpText = FText::FromString(FieldValueScratch);
		JsonObj->TryGetNumberField(MixerStringConstants::FieldNames::Cost, Button.Desc.SparkCost);

		// Disabled and cooldown shouldn't be set initially

		Button.State.DownCount = 0;
		Button.State.UpCount = 0;
		Button.State.PressCount = 0;
		Button.State.Enabled = true;
		Button.State.RemainingCooldown = FTimespan::Zero();
		Button.State.Progress = 0.0f;
		Button.SceneId = SceneId;

		AddButton(*ControlId, Button);
	}
	else if (ControlKind == FMixerInteractiveControl::JoystickKind)
	{
		FMixerStickPropertiesCached Stick;
		Stick.State.Enabled = true;
		AddStick(*ControlId, Stick);
	}
	else if (ControlKind == FMixerInteractiveControl::LabelKind)
	{
		FMixerLabelPropertiesCached Label;
		Label.Desc.TextSize = 0;
		Label.Desc.Underline = false;
		Label.Desc.Bold = false;
		Label.Desc.Italic = false;

		FString FieldValueScratch;
		if (JsonObj->TryGetStringField(MixerStringConstants::FieldNames::Text, FieldValueScratch))
		{
			Label.Desc.Text = FText::FromString(FieldValueScratch);
		}

		if (JsonObj->TryGetStringField(MixerStringConstants::FieldNames::TextColor, FieldValueScratch))
		{
			Label.Desc.TextColor = FColor::FromHex(FieldValueScratch);
		}
		else
		{
			Label.Desc.TextColor = FColor::White;
		}

		JsonObj->TryGetStringField(MixerStringConstants::FieldNames::TextSize, Label.Desc.TextSize);
		JsonObj->TryGetBoolField(MixerStringConstants::FieldNames::Underline, Label.Desc.Underline);
		JsonObj->TryGetBoolField(MixerStringConstants::FieldNames::Bold, Label.Desc.Bold);
		JsonObj->TryGetBoolField(MixerStringConstants::FieldNames::Italic, Label.Desc.Italic);

		Label.SceneId = SceneId;
		AddLabel(*ControlId, Label);
	}
	else if (ControlKind == FMixerInteractiveControl::TextboxKind)
	{
		FMixerTextboxPropertiesCached Textbox;
		JsonObj->TryGetNumberField(MixerStringConstants::FieldNames::Cost, Textbox.Desc.SparkCost);
		JsonObj->TryGetBoolField(MixerStringConstants::FieldNames::Multiline, Textbox.Desc.Multiline);
		JsonObj->TryGetBoolField(MixerStringConstants::FieldNames::HasSubmit, Textbox.Desc.HasSubmit);
		FString FieldValueScratch;
		if (JsonObj->TryGetStringField(MixerStringConstants::FieldNames::Placeholder, FieldValueScratch))
		{
			Textbox.Desc.Placeholder = FText::FromString(FieldValueScratch);
		}
		if (JsonObj->TryGetStringField(MixerStringConstants::FieldNames::SubmitText, FieldValueScratch))
		{
			Textbox.Desc.SubmitText = FText::FromString(FieldValueScratch);
		}
		AddTextbox(*ControlId, Textbox);
	}
	else
	{
		OnCustomControlPropertyUpdate().Broadcast(*ControlId, JsonObj);
	}

	return true;
}


#endif

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
int32 MixerUELinkerHelper;