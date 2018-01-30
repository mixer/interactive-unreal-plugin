#include "MixerChatConnection.h"

#include "MixerInteractivityModule.h"
#include "MixerInteractivityTypes.h"
#include "MixerInteractivityUserSettings.h"
#include "OnlineChatMixer.h"

#include "HttpModule.h"
#include "PlatformHttp.h"
#include "JsonTypes.h"
#include "JsonPrintPolicy.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "WebsocketsModule.h"
#include "IWebSocket.h"
#include "OnlineSubsystemTypes.h"

DEFINE_LOG_CATEGORY(LogMixerChat);

bool FMixerChatConnection::Init()
{
#if WITH_WEBSOCKETS
	TSharedRef<IHttpRequest> ChannelRequest = FHttpModule::Get().CreateRequest();
	ChannelRequest->SetVerb(TEXT("GET"));
	ChannelRequest->SetURL(FString::Printf(TEXT("https://mixer.com/api/v1/channels/%s"), *RoomId));

	ChannelRequest->OnProcessRequestComplete().BindSP(this, &FMixerChatConnection::OnGetChannelInfoForRoomIdComplete);
	return ChannelRequest->ProcessRequest();
#else
	UE_LOG(LogMixerChat, Warning, TEXT("Mixer chat requires websockets which are not available on this platform."));
	return false;
#endif
}

void FMixerChatConnection::Cleanup()
{
	CloseWebSocket();
}

void FMixerChatConnection::JoinDiscoveredChatChannel()
{
	TSharedRef<IHttpRequest> ChatRequest = FHttpModule::Get().CreateRequest();
	ChatRequest->SetVerb(TEXT("GET"));
	ChatRequest->SetURL(FString::Printf(TEXT("https://mixer.com/api/v1/chats/%d?fields=id"), ChannelId));

	// Setting Authorization header to an empty string will just fail rather than perform anonymous auth.
	const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
	FString AuthZHeaderValue = UserSettings->GetAuthZHeaderValue();
	if (AuthZHeaderValue.Len() > 0)
	{
		ChatRequest->SetHeader(TEXT("Authorization"), AuthZHeaderValue);
	}
	else
	{
		UE_LOG(LogMixerChat, Warning, TEXT("No auth token found.  Chat connection will be anonymous and will not allow sending messages.  Sign in to Mixer to enable."));
	}

	ChatRequest->OnProcessRequestComplete().BindSP(this, &FMixerChatConnection::OnDiscoverChatServersComplete);
	if (!ChatRequest->ProcessRequest())
	{
		FString ErrorMessage = TEXT("Failed to send request for chat web socket connection info.");
		UE_LOG(LogMixerChat, Warning, TEXT("%s for room %s"), *ErrorMessage, *RoomId);
		IMixerInteractivityModule::Get().GetChatInterface()->TriggerOnChatRoomJoinPublicDelegates(*User, RoomId, false, ErrorMessage);
	}
}

void FMixerChatConnection::OnGetChannelInfoForRoomIdComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (bSucceeded && HttpResponse.IsValid())
	{
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			FString ResponseStr = HttpResponse->GetContentAsString();
			TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ResponseStr);
			TSharedPtr<FJsonObject> JsonObject;
			if (FJsonSerializer::Deserialize(JsonReader, JsonObject) &&
				JsonObject.IsValid())
			{
				JsonObject->TryGetNumberField(TEXT("id"), ChannelId);
			}
		}
	}

	if (ChannelId != 0)
	{
		JoinDiscoveredChatChannel();
	}
	else
	{
		OnChatConnectionError(TEXT("Could not find Mixer chat channel for room id."));
	}
}

void FMixerChatConnection::OnDiscoverChatServersComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (bSucceeded && HttpResponse.IsValid())
	{
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			FString ResponseStr = HttpResponse->GetContentAsString();
			TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ResponseStr);
			TSharedPtr<FJsonObject> JsonObject;
			if (FJsonSerializer::Deserialize(JsonReader, JsonObject) &&
				JsonObject.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>> *JsonEndpoints;
				if (JsonObject->TryGetArrayField(TEXT("endpoints"), JsonEndpoints))
				{
					for (const TSharedPtr<FJsonValue>& Endpoint : *JsonEndpoints)
					{
						Endpoints.Add(Endpoint->AsString());
					}

					JsonObject->TryGetStringField(TEXT("authkey"), AuthKey);
					OpenWebSocket();
				}
			}
		}
	}

	// Should have a web socket going by now.
	if (!WebSocket.IsValid())
	{
		OnChatConnectionError(TEXT("Failed to create web socket"));
	}
}

void FMixerChatConnection::OnChatSocketConnected()
{
	FString AuthPacketString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&AuthPacketString);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("type"), TEXT("method"));
	Writer->WriteValue(TEXT("method"), TEXT("auth"));
	Writer->WriteArrayStart(TEXT("arguments"));
	Writer->WriteValue(ChannelId);
	TSharedPtr<const FMixerLocalUser> CurrentUser = IMixerInteractivityModule::Get().GetCurrentUser();
	if (CurrentUser.IsValid() && !AuthKey.IsEmpty())
	{
		UE_LOG(LogMixerChat, Log, TEXT("Authenticating to chat room %s as user '%s'"), *RoomId, *CurrentUser->Name);
		Writer->WriteValue(CurrentUser->Id);
		Writer->WriteValue(AuthKey);
	}
	else
	{
		UE_LOG(LogMixerChat, Log, TEXT("Authenticating to chat room %s anonymously"), *RoomId);
	}
	Writer->WriteArrayEnd();
	Writer->WriteValue(TEXT("id"), MessageId++);
	Writer->WriteObjectEnd();
	Writer->Close();

	WebSocket->Send(AuthPacketString);
}

void FMixerChatConnection::OnChatConnectionError(const FString& ErrorMessage)
{
	UE_LOG(LogMixerChat, Warning, TEXT("Failed to connect chat web socket for room %s with error '%s'"), *RoomId, *ErrorMessage);
	IMixerInteractivityModule::Get().GetChatInterface()->TriggerOnChatRoomJoinPublicDelegates(*User, RoomId, false, ErrorMessage);
}

void FMixerChatConnection::OnChatSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	// This should be a remote close since we unhook event handlers before closing on our end.
	// Do a full close and re-open of the websocket so as to (potentially) hit a different endpoint, per Mixer guidance.

	UE_LOG(LogMixerChat, Warning, TEXT("Chat websocket closed with reason '%s'.  Attempting to reconnect."), *Reason);

	CloseWebSocket();

	OpenWebSocket();
}

void FMixerChatConnection::OnChatPacket(const FString& PacketJsonString)
{
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(PacketJsonString);
	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) &&
		JsonObject.IsValid())
	{
		FString MessageType;
		if (JsonObject->TryGetStringField(TEXT("type"), MessageType))
		{
			if (MessageType == TEXT("reply"))
			{
				// @TODO - Response to a method we called.  Verify success, handle results, etc.
			}
			else if (MessageType == TEXT("event"))
			{
				FString EventType;
				if (JsonObject->TryGetStringField(TEXT("event"), EventType))
				{
					if (EventType == TEXT("WelcomeEvent"))
					{
						// Welcomed by the server.  We are now fully connected.
						bIsReady = true;
						IMixerInteractivityModule::Get().GetChatInterface()->TriggerOnChatRoomJoinPublicDelegates(*User, RoomId, true, TEXT(""));
					}
					else
					{
						// Data field is important for all the following event types
						const TSharedPtr<FJsonObject>* Data;
						if (!JsonObject->TryGetObjectField(TEXT("data"), Data))
						{
							UE_LOG(LogMixerChat, Error, TEXT("Missing data field for chat event of type %s (full payload %s)"), *EventType, *PacketJsonString);
							return;
						}

						check(Data != nullptr);
						check(Data->IsValid());

						if (EventType == TEXT("ChatMessage"))
						{
							HandleChatMessagePacket(Data->Get());
						}
						else if (EventType == TEXT("UserJoin"))
						{
							FString JoiningUser;
							if ((*Data)->TryGetStringField(TEXT("username"), JoiningUser))
							{
								UE_LOG(LogMixerChat, Log, TEXT("%s is joining %s's chat channel"), *JoiningUser, *RoomId);

								// @TODO: we should probably create a derived FUniqueNetId for Mixer.
								TSharedRef<FUniqueNetId> JoiningId = MakeShared<FUniqueNetIdString>(JoiningUser);
								IMixerInteractivityModule::Get().GetChatInterface()->TriggerOnChatRoomMemberJoinDelegates(*User, RoomId, *JoiningId);
							}
						}
						else if (EventType == TEXT("UserLeave"))
						{
							FString LeavingUser;
							if ((*Data)->TryGetStringField(TEXT("username"), LeavingUser))
							{
								UE_LOG(LogMixerChat, Log, TEXT("%s is leaving %s's chat channel"), *LeavingUser, *RoomId);

								// @TODO: we should probably create a derived FUniqueNetId for Mixer.
								TSharedRef<FUniqueNetId> LaavingId = MakeShared<FUniqueNetIdString>(LeavingUser);
								IMixerInteractivityModule::Get().GetChatInterface()->TriggerOnChatRoomMemberExitDelegates(*User, RoomId, *LaavingId);
							}
						}
					}
				}
				else
				{
					UE_LOG(LogMixerChat, Error, TEXT("Missing event (type) field for chat event (full payload %s)"), *PacketJsonString);
				}
			}
			else
			{
				// Unknown message type
				UE_LOG(LogMixerChat, Error, TEXT("Unknown message of type %s on chat socket (full payload %s)"), *MessageType, *PacketJsonString);
			}
		}
		else
		{
			UE_LOG(LogMixerChat, Error, TEXT("Missing type field for chat socket message (full payload %s)"), *PacketJsonString);
		}
	}
}

void FMixerChatConnection::HandleChatMessagePacket(FJsonObject* Payload)
{
	FString FromUser;
	if (!Payload->TryGetStringField(TEXT("user_name"), FromUser))
	{
		UE_LOG(LogMixerChat, Error, TEXT("Missing user_name field for chat event"));
		return;
	}

	const TSharedPtr<FJsonObject>* MessageObject;
	if (!Payload->TryGetObjectField(TEXT("message"), MessageObject))
	{
		UE_LOG(LogMixerChat, Error, TEXT("Missing message field for chat event"));
		return;
	}

	const TSharedPtr<FJsonObject>* Metadata;
	bool bIsWhisper = false;
	bool bIsAction = false;
	if ((*MessageObject)->TryGetObjectField(TEXT("meta"), Metadata))
	{
		(*Metadata)->TryGetBoolField(TEXT("whisper"), bIsWhisper);
		(*Metadata)->TryGetBoolField(TEXT("me"), bIsAction);
	}

	FString MessageString;
	if (bIsAction)
	{
		MessageString = FromUser + TEXT(" ");
	}

	const TArray<TSharedPtr<FJsonValue>>* MessageFragmentArray;
	if (!(*MessageObject)->TryGetArrayField(TEXT("message"), MessageFragmentArray))
	{
		UE_LOG(LogMixerChat, Error, TEXT("Missing message.message array for chat event"));
		return;
	}

	for (const TSharedPtr<FJsonValue>& Fragment : *MessageFragmentArray)
	{
		const TSharedPtr<FJsonObject>* FragmentObj;
		if (Fragment->TryGetObject(FragmentObj))
		{
			HandleChatMessageFragment(MessageString, FragmentObj->Get());
		}
	}

	// @TODO: we should probably create a derived FUniqueNetId for Mixer.
	TSharedRef<FChatMessageMixer> FinalMessage = MakeShared<FChatMessageMixer>(MakeShared<FUniqueNetIdString>(FromUser), MessageString);
	if (bIsWhisper)
	{
		UE_LOG(LogMixerChat, Verbose, TEXT("Private message from %s: %s"), *FromUser, *MessageString);
		IMixerInteractivityModule::Get().GetChatInterface()->TriggerOnChatPrivateMessageReceivedDelegates(*User, FinalMessage);
	}
	else
	{
		UE_LOG(LogMixerChat, Verbose, TEXT("Chat message from %s: %s"), *FromUser, *MessageString);
		IMixerInteractivityModule::Get().GetChatInterface()->TriggerOnChatRoomMessageReceivedDelegates(*User, RoomId, FinalMessage);
	}
}

void FMixerChatConnection::HandleChatMessageFragment(FString& MessageSoFar, FJsonObject* Fragment)
{
	FString FragmentType;
	if (!Fragment->TryGetStringField(TEXT("type"), FragmentType))
	{
		UE_LOG(LogMixerChat, Error, TEXT("Missing type field for chat message fragment."));
		return;
	}

	// For now just always append the fragment text no matter the type.
	// In the future we could perhaps add markup?
	FString FragmentText;
	if (!Fragment->TryGetStringField(TEXT("text"), FragmentText))
	{
		UE_LOG(LogMixerChat, Error, TEXT("Missing text field for chat message fragment."));
		return;
	}

	MessageSoFar += FragmentText;
}

bool FMixerChatConnection::SendChatMessage(const FString& MessageBody)
{
	if (!bIsReady)
	{
		UE_LOG(LogMixerChat, Warning, TEXT("Attempt to send chat to room %s before connection has been established.  Wait for OnChatRoomJoin event."), *RoomId);
		return false;
	}

	if (IsAnonymous())
	{
		UE_LOG(LogMixerChat, Warning, TEXT("Attempt to send chat to room %s when connected anonymously."), *RoomId);
		return false;
	}

	check(WebSocket.IsValid());
	check(WebSocket->IsConnected());

	FString MessagePacketString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MessagePacketString);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("type"), TEXT("method"));
	Writer->WriteValue(TEXT("method"), TEXT("msg"));
	Writer->WriteArrayStart(TEXT("arguments"));
	Writer->WriteValue(MessageBody);
	Writer->WriteArrayEnd();
	Writer->WriteValue(TEXT("id"), MessageId++);
	Writer->WriteObjectEnd();
	Writer->Close();

	WebSocket->Send(MessagePacketString);

	return true;
}

bool FMixerChatConnection::SendWhisper(const FString& ToUser, const FString& MessageBody)
{
	if (!bIsReady)
	{
		UE_LOG(LogMixerChat, Warning, TEXT("Attempt to send chat to room %s before connection has been established.  Wait for OnChatRoomJoin event."), *RoomId);
		return false;
	}

	if (IsAnonymous())
	{
		UE_LOG(LogMixerChat, Warning, TEXT("Attempt to send chat to room %s when connected anonymously."), *RoomId);
		return false;
	}

	FString WhisperPacketString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&WhisperPacketString);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("type"), TEXT("method"));
	Writer->WriteValue(TEXT("method"), TEXT("whisper"));
	Writer->WriteArrayStart(TEXT("arguments"));
	Writer->WriteValue(ToUser);
	Writer->WriteValue(MessageBody);
	Writer->WriteArrayEnd();
	Writer->WriteValue(TEXT("id"), MessageId++);
	Writer->WriteObjectEnd();
	Writer->Close();

	WebSocket->Send(WhisperPacketString);

	return true;
}

void FMixerChatConnection::OpenWebSocket()
{
	// Shouldn't ever get this far if we don't have a websocket implementation.
	check(WITH_WEBSOCKETS != 0);
	const FString& SelectedEndpoint = Endpoints[FMath::RandRange(0, Endpoints.Num() - 1)];
	UE_LOG(LogMixerChat, Verbose, TEXT("Opening web socket to %s for chat room %s"), *SelectedEndpoint, *RoomId);

	// Regardless, CreateWebSocket won't compile on all platforms.
#if WITH_WEBSOCKETS
	// Explicitly list protocols for the benefit of Xbox
	TArray<FString> Protocols;
	Protocols.Add(TEXT("wss"));
	Protocols.Add(TEXT("ws"));
	WebSocket = FModuleManager::LoadModuleChecked<FWebSocketsModule>("WebSockets").CreateWebSocket(SelectedEndpoint, Protocols);
#endif

	if (WebSocket.IsValid())
	{
		WebSocket->OnConnected().AddSP(this, &FMixerChatConnection::OnChatSocketConnected);
		WebSocket->OnConnectionError().AddSP(this, &FMixerChatConnection::OnChatConnectionError);
		WebSocket->OnMessage().AddSP(this, &FMixerChatConnection::OnChatPacket);
		WebSocket->OnClosed().AddSP(this, &FMixerChatConnection::OnChatSocketClosed);

		WebSocket->Connect();
	}
}

void FMixerChatConnection::CloseWebSocket()
{
	if (WebSocket.IsValid())
	{
		bIsReady = false;

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
