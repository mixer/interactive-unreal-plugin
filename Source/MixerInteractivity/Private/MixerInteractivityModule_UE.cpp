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
			SendMethodMessageObjectParams(TEXT("ready"), nullptr, Params);
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
			SendMethodMessageObjectParams(TEXT("ready"), nullptr, Params);
			SetInteractivityState(EMixerInteractivityState::Interactivity_Stopping);
		}
		break;

	default:
		break;
	}
}

void FMixerInteractivityModule_UE::SetCurrentScene(FName Scene, FName GroupName)
{
	CreateOrUpdateGroup(TEXT("updateGroups"), Scene, GroupName);
}

void FMixerInteractivityModule_UE::TriggerButtonCooldown(FName Button, FTimespan CooldownTime)
{
	double NewCooldownTime = static_cast<double>((FDateTime::UtcNow() + CooldownTime).ToUnixTimestamp() * 1000);
	TSharedRef<FJsonObject> UpdatedProps = MakeShared<FJsonObject>();
	UpdatedProps->SetNumberField(TEXT("cooldown"), NewCooldownTime);
	UpdateRemoteControl(FName("default"), Button, UpdatedProps);
}

TSharedPtr<const FMixerRemoteUser> FMixerInteractivityModule_UE::GetParticipant(uint32 ParticipantId)
{
	TSharedPtr<FMixerRemoteUserCached>* ExistingUser = RemoteParticipantCacheByUint.Find(ParticipantId);
	return ExistingUser != nullptr ? *ExistingUser : nullptr;
}

bool FMixerInteractivityModule_UE::CreateGroup(FName GroupName, FName InitialScene)
{
	return CreateOrUpdateGroup(TEXT("createGroups"), InitialScene, GroupName);
}

bool FMixerInteractivityModule_UE::GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants)
{
	for (TMap<uint32, TSharedPtr<FMixerRemoteUserCached>>::TConstIterator It(RemoteParticipantCacheByUint); It; ++It)
	{
		if (It->Value->Group == GroupName)
		{
			OutParticipants.Add(It->Value);
		}
	}

	return true;
}

bool FMixerInteractivityModule_UE::MoveParticipantToGroup(FName GroupName, uint32 ParticipantId)
{
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Logged_In)
	{
		return false;
	}

	TSharedPtr<FMixerRemoteUserCached>* ExistingUser = RemoteParticipantCacheByUint.Find(ParticipantId);
	if (ExistingUser == nullptr)
	{
		return false;
	}

	FMixerUpdateParticipantGroupParamsEntry ParamEntry;
	ParamEntry.ParticipantSessionGuid = (*ExistingUser)->SessionGuid.ToString(EGuidFormats::DigitsWithHyphens).ToLower();
	ParamEntry.GroupId = GroupName.ToString();

	FMixerUpdateParticipantGroupParams Params;
	Params.Participants.Add(ParamEntry);
	SendMethodMessageObjectParams(TEXT("updateParticipants"), nullptr, Params);

	return true;
}

void FMixerInteractivityModule_UE::CaptureSparkTransaction(const FString& TransactionId)
{
	if (GetInteractiveConnectionAuthState() == EMixerLoginState::Logged_In)
	{
		FMixerCaptureTransactionParams Params;
		Params.TransactionId = TransactionId;
		SendMethodMessageObjectParams(TEXT("capture"), nullptr, Params);
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
		RemoteParticipantCacheByGuid.Empty();
		RemoteParticipantCacheByUint.Empty();
	}
}

bool FMixerInteractivityModule_UE::HandleSingleControlUpdate(FName ControlId, const TSharedRef<FJsonObject> ControlData)
{

	return false;
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
	const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
	FString SharecodeParam;
	if (!Settings->ShareCode.IsEmpty())
	{
		SharecodeParam = FString::Printf(TEXT("&x-interactive-sharecode=%s"), *Settings->ShareCode);
	}

	FString EndpointWithAuth = FString::Printf(
		TEXT("%s?authorization=%s&x-interactive-version=%u&x-protocol-version=2.0%s"),
		*Endpoints[0],
		*FPlatformHttp::UrlEncode(UserSettings->GetAuthZHeaderValue()),
		Settings->GameVersionId,
		*SharecodeParam);
	UE_LOG(LogMixerInteractivity, Verbose, TEXT("Opening web socket to %s for interactivity"), *Endpoints[0]);

	Endpoints.RemoveAtSwap(0);
	InitConnection(EndpointWithAuth);
}

bool FMixerInteractivityModule_UE::CreateOrUpdateGroup(const FString& MethodName, FName Scene, FName GroupName)
{
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Logged_In)
	{
		return false;
	}

	FMixerUpdateGroupMessageParamsEntry ParamEntry;
	ParamEntry.GroupId = GroupName.ToString();
	ParamEntry.SceneId = Scene != NAME_None ? Scene.ToString() : TEXT("default");
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
}

bool FMixerInteractivityModule_UE::OnUnhandledServerMessage(const FString& MessageType, const TSharedPtr<FJsonObject> Params)
{
	OnCustomMethodCall().Broadcast(*MessageType, Params);
	return true;
}

bool FMixerInteractivityModule_UE::HandleHello(FJsonObject* JsonObj)
{
	SendMethodMessageNoParams(TEXT("getScenes"), &FMixerInteractivityModule_UE::HandleGetScenesReply);
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

	TSharedPtr<FMixerRemoteUserCached>* RemoteUser = RemoteParticipantCacheByGuid.Find(ParticipantGuid);
	return HandleGiveInput(RemoteUser ? *RemoteUser : nullptr, JsonObj, InputObj->ToSharedRef());
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

	GET_JSON_STRING_RETURN_FAILURE(ControlId, ControlId);
	GET_JSON_STRING_RETURN_FAILURE(Event, EventType);

	// @TODO -
	// Need additional checks in case multiple types support
	// the same event.  Requires that we maintain scene structure 
	// so we can lookup control kind.
	FString ControlKind;
	if (EventType == TEXT("mousedown")) // && ControlKind == TEXT("button"))
	{
		FMixerButtonEventDetails EventDetails;
		EventDetails.Pressed = true;
		if (FullParamsJson->TryGetStringField(MixerStringConstants::FieldNames::TransactionId, EventDetails.TransactionId))
		{
			// @TODO
			// Lookup actual Spark cost.  Requires scene structure. 
		}
		else
		{
			EventDetails.SparkCost = 0;
		}
		OnButtonEvent().Broadcast(*ControlId, Participant, EventDetails);
		return true;
	}
	else if (EventType == TEXT("mouseup")) // && ControlKind == TEXT("button"))
	{
		FMixerButtonEventDetails EventDetails;
		EventDetails.Pressed = false;
		if (FullParamsJson->TryGetStringField(MixerStringConstants::FieldNames::TransactionId, EventDetails.TransactionId))
		{
			// @TODO
			// Lookup actual Spark cost.  Requires scene structure. 
		}
		else
		{
			EventDetails.SparkCost = 0;
		}
		EventDetails.SparkCost = 0; //?
		OnButtonEvent().Broadcast(*ControlId, Participant, EventDetails);
		return true;
	}
	else if (EventType == TEXT("move")) // && ControlKind == TEXT("joystick"))
	{
		GET_JSON_DOUBLE_RETURN_FAILURE(X, X);
		GET_JSON_DOUBLE_RETURN_FAILURE(Y, Y);

		OnStickEvent().Broadcast(*ControlId, Participant, FVector2D(static_cast<float>(X), static_cast<float>(Y)));
		return true;
	}
	else
	{
		OnCustomControlInput().Broadcast(*ControlId, *EventType, Participant, InputObjJson);
		return true;
	}
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

	TSharedPtr<FMixerRemoteUserCached> RemoteUser;
	TSharedPtr<FMixerRemoteUserCached>* ExistingUser = RemoteParticipantCacheByUint.Find(UserId);
	bool bOldInputEnabled = false;
	if (ExistingUser != nullptr)
	{
		RemoteUser = *ExistingUser;
		bOldInputEnabled = RemoteUser->InputEnabled;
	}
	else
	{
		RemoteUser = MakeShared<FMixerRemoteUserCached>();
		RemoteUser->Id = UserId;
		RemoteUser->SessionGuid = SessionGuid;
		RemoteUser->ConnectedAt = FDateTime::FromUnixTimestamp(static_cast<int64>(ConnectedAtDouble / 1000.0));

		if (EventType != EMixerInteractivityParticipantState::Left)
		{
			RemoteParticipantCacheByGuid.Add(SessionGuid, RemoteUser);
			RemoteParticipantCacheByUint.Add(RemoteUser->Id, RemoteUser);
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

	if (ExistingUser != nullptr && EventType == EMixerInteractivityParticipantState::Left)
	{
		RemoteParticipantCacheByGuid.Remove(SessionGuid);
		RemoteParticipantCacheByUint.Remove(UserId);
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

	for (const TSharedPtr<FJsonValue>& Control : *Controls)
	{
		ParsePropertiesFromSingleControl(Control->AsObject().Get());
	}

	return true;
}

bool FMixerInteractivityModule_UE::ParsePropertiesFromSingleControl(FJsonObject* JsonObj)
{
	GET_JSON_STRING_RETURN_FAILURE(Kind, ControlKind);
	GET_JSON_STRING_RETURN_FAILURE(ControlId, ControlId);

	if (ControlKind == FMixerInteractiveControl::ButtonKind)
	{
		GET_JSON_STRING_RETURN_FAILURE(Text, Text);
		GET_JSON_STRING_RETURN_FAILURE(Tooltip, Tooltip);
		GET_JSON_INT_RETURN_FAILURE(Cost, Cost);
		GET_JSON_BOOL_RETURN_FAILURE(Disabled, bDisabled);
		GET_JSON_DOUBLE_RETURN_FAILURE(Cooldown, Cooldown);

		FMixerButtonPropertiesCached Button;
		Button.Desc.ButtonText = FText::FromString(Text);
		Button.Desc.HelpText = FText::FromString(Tooltip);
		Button.Desc.SparkCost = Cost;

		Button.State.DownCount = 0;
		Button.State.UpCount = 0;
		Button.State.PressCount = 0;
		Button.State.Enabled = !bDisabled;
		uint64 TimeNowInMixerUnits = FDateTime::UtcNow().ToUnixTimestamp() * 1000;
		if (Cooldown > TimeNowInMixerUnits)
		{
			Button.State.RemainingCooldown = FTimespan::FromMilliseconds(static_cast<double>(static_cast<uint64>(Cooldown) - TimeNowInMixerUnits));
		}
		else
		{
			Button.State.RemainingCooldown = FTimespan(0);
		}
		Button.State.Progress = 0.0f;

		return true;
	}
	else if (ControlKind == FMixerInteractiveControl::JoystickKind)
	{

		return true;
	}

	return false;
}


#endif