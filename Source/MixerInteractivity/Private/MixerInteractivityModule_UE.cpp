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
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "WebsocketsModule.h"
#include "IWebSocket.h"

#if !WITH_WEBSOCKETS
#error "UE backend requires UE websockets"
#endif

IMPLEMENT_MODULE(FMixerInteractivityModule_UE, MixerInteractivity);

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
			TSharedPtr<FJsonObject> ReadyParams = MakeShared<FJsonObject>();
			ReadyParams->SetBoolField(MixerStringConstants::FieldNames::IsReady, true);
			SendMethodMessage(TEXT("ready"), nullptr, ReadyParams);
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
			TSharedPtr<FJsonObject> ReadyParams = MakeShared<FJsonObject>();
			ReadyParams->SetBoolField(MixerStringConstants::FieldNames::IsReady, false);
			SendMethodMessage(TEXT("ready"), nullptr, ReadyParams);
			SetInteractivityState(EMixerInteractivityState::Interactivity_Stopping);
		}
		break;

	default:
		break;
	}
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
		SharecodeParam = FString::Printf(TEXT("&x-interactive-sharecode=%s"), &Settings->ShareCode);
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
	RegisterServerMessageHandler(TEXT("onReady"), &FMixerInteractivityModule_UE::HandleReadyStateChange);
}

bool FMixerInteractivityModule_UE::HandleHello(FJsonObject* JsonObj)
{
	SendMethodMessage(TEXT("getScenes"), &FMixerInteractivityModule_UE::HandleGetScenesReply, nullptr);
	return true;
}

bool FMixerInteractivityModule_UE::HandleGiveInput(FJsonObject* JsonObj)
{
	return true;
}

bool FMixerInteractivityModule_UE::HandleParticipantJoin(FJsonObject* JsonObj)
{
	GET_JSON_ARRAY_RETURN_FAILURE(Participants, JoiningParticipants);

	bool bHandled = true;
	for (const TSharedPtr<FJsonValue>& JoiningParticipant : *JoiningParticipants)
	{
		bHandled &= HandleSingleJoiningParticipant(JoiningParticipant->AsObject().Get());
	}
	return bHandled;
}

bool FMixerInteractivityModule_UE::HandleReadyStateChange(FJsonObject* JsonObj)
{
	GET_JSON_BOOL_RETURN_FAILURE(IsReady, bIsReady);
	SetInteractivityState(bIsReady ? EMixerInteractivityState::Interactive : EMixerInteractivityState::Not_Interactive);
	return true;
}

bool FMixerInteractivityModule_UE::HandleGetScenesReply(FJsonObject* JsonObj)
{
	// @TODO - parse scenes
	SetInteractiveConnectionAuthState(EMixerLoginState::Logged_In);
	return true;
}

bool FMixerInteractivityModule_UE::HandleSingleJoiningParticipant(const FJsonObject* JsonObj)
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
		UE_LOG(LogMixerInteractivity, Error, TEXT("sessionID field %s for onParticipantJoin was not in the expected format (guid)"), *SessionGuidString);
		return false;
	}

	TSharedPtr<FMixerRemoteUser> RemoteUser = MakeShared<FMixerRemoteUser>();
	RemoteUser->Name = Username;
	RemoteUser->Id = UserId;
	RemoteUser->Level = UserLevel;
	RemoteUser->ConnectedAt = FDateTime::FromUnixTimestamp(static_cast<int64>(ConnectedAtDouble / 1000.0));
	RemoteUser->InputAt = FDateTime::FromUnixTimestamp(static_cast<int64>(LastInputAtDouble / 1000.0));
	RemoteUser->Group = *GroupId;
	RemoteParticipantCacheByGuid.Add(SessionGuid, RemoteUser);
	RemoteParticipantCachedByUint.Add(RemoteUser->Id, RemoteUser);
	OnParticipantStateChanged().Broadcast(RemoteUser, EMixerInteractivityParticipantState::Joined);

	return true;
}

#endif