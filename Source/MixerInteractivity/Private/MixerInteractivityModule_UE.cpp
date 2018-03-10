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
#include "MixerJsonHelpers.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "WebsocketsModule.h"
#include "IWebSocket.h"

#if !WITH_WEBSOCKETS
#error "UE backend requires UE websockets"
#endif

IMPLEMENT_MODULE(FMixerInteractivityModule_UE, MixerInteractivity);

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

	// Explicitly list protocols for the benefit of Xbox
	TArray<FString> Protocols;
	Protocols.Add(TEXT("wss"));
	Protocols.Add(TEXT("ws"));
	WebSocket = FModuleManager::LoadModuleChecked<FWebSocketsModule>("WebSockets").CreateWebSocket(EndpointWithAuth, Protocols);
	Endpoints.RemoveAtSwap(0);

	if (WebSocket.IsValid())
	{
		WebSocket->OnConnected().AddRaw(this, &FMixerInteractivityModule_UE::OnSocketConnected);
		WebSocket->OnConnectionError().AddRaw(this, &FMixerInteractivityModule_UE::OnSocketConnectionError);
		WebSocket->OnMessage().AddRaw(this, &FMixerInteractivityModule_UE::OnSocketMessage);
		WebSocket->OnClosed().AddRaw(this, &FMixerInteractivityModule_UE::OnSocketClosed);

		WebSocket->Connect();
	}
}

void FMixerInteractivityModule_UE::CloseWebSocket()
{
	if (WebSocket.IsValid())
	{
		WebSocket->OnConnected().RemoveAll(this);
		WebSocket->OnConnectionError().RemoveAll(this);
		WebSocket->OnMessage().RemoveAll(this);
		WebSocket->OnClosed().RemoveAll(this);

		if (WebSocket->IsConnected())
		{
			WebSocket->Close();
		}

		WebSocket.Reset();
	}
}

void FMixerInteractivityModule_UE::OnSocketConnected()
{
	UE_LOG(LogMixerInteractivity, Verbose, TEXT("Interactivity socket connected!"));

	// No real action here - we'll wait for a hello
}

void FMixerInteractivityModule_UE::OnSocketConnectionError(const FString& ErrorMessage)
{
	UE_LOG(LogMixerInteractivity, Warning, TEXT("Failed to connect web socket for interactivity with error %s"), *ErrorMessage);
	CloseWebSocket();
	OpenWebSocket();
}

void FMixerInteractivityModule_UE::OnSocketMessage(const FString& MessageJsonString)
{
	UE_LOG(LogMixerInteractivity, Verbose, TEXT("Interactivity message %s"), *MessageJsonString);

	bool bHandled = false;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(MessageJsonString);
	TSharedPtr<FJsonObject> JsonObj;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObj) && JsonObj.IsValid())
	{
		bHandled = OnSocketMessageInternal(JsonObj.Get());
	}

	if (!bHandled)
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("Failed to handle interactivity message from server: %s"), *MessageJsonString);
	}
}

void FMixerInteractivityModule_UE::OnSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG(LogMixerInteractivity, Warning, TEXT("Interactivity websocket closed with reason '%s'."), *Reason);

	CloseWebSocket();

	// Attempt to reconnect
	OpenWebSocket();
}

bool FMixerInteractivityModule_UE::OnSocketMessageInternal(FJsonObject* JsonObj)
{
	bool bHandled = false;
	GET_JSON_STRING_RETURN_FAILURE(Type, MessageType);
	if (MessageType == MixerStringConstants::MessageTypes::Reply)
	{
	}
	else if (MessageType == MixerStringConstants::MessageTypes::Method)
	{
		GET_JSON_STRING_RETURN_FAILURE(Method, MethodType);
		if (MethodType == TEXT("hello"))
		{
			SetInteractiveConnectionAuthState(EMixerLoginState::Logged_In);
			bHandled = true;
		}
	}

	return bHandled;
}

#endif