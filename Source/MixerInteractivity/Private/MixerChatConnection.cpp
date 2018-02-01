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

namespace MixerChatStringConstants
{
	namespace MessageTypes
	{
		const FString Method = TEXT("method");
		const FString Reply = TEXT("reply");
		const FString Event = TEXT("event");
	}

	namespace MethodNames
	{
		const FString Auth = TEXT("auth");
		const FString Msg = TEXT("msg");
		const FString Whisper = TEXT("whisper");
		const FString History = TEXT("history");
	}

	namespace EventTypes
	{
		const FString Welcome = TEXT("WelcomeEvent");
		const FString ChatMessage = TEXT("ChatMessage");
		const FString UserJoin = TEXT("UserJoin");
		const FString UserLeave = TEXT("UserLeave");
		const FString DeleteMessage = TEXT("DeleteMessage");
		const FString ClearMessages = TEXT("ClearMessages");
	}

	namespace FieldNames
	{
		const FString Type = TEXT("type");
		const FString Event = TEXT("event");
		const FString Data = TEXT("data");
		const FString Message = TEXT("message");
		const FString UserNameNoUnderscore = TEXT("username");
		const FString UserNameWithUnderscore = TEXT("user_name");
		const FString Id = TEXT("id");
		const FString Meta = TEXT("meta");
		const FString Me = TEXT("me");
		const FString Whisper = TEXT("whisper");
		const FString Method = TEXT("method");
		const FString Arguments = TEXT("arguments");
		const FString Error = TEXT("error");
		const FString Text = TEXT("text");
		const FString Endpoints = TEXT("endpoints");
		const FString AuthKey = TEXT("authkey");
	}
}

namespace
{
	template <class T, class ...ArgTypes>
	void WriteRemoteMethodParams(TJsonWriter<>& Writer, T Param1, ArgTypes... AdditionalArgs)
	{
		Writer.WriteValue(Param1);
		WriteRemoteMethodParams(Writer, AdditionalArgs...);
	}

	template <class T>
	void WriteRemoteMethodParams(TJsonWriter<>& Writer, T Param1)
	{
		Writer.WriteValue(Param1);
	}

	template <class ... ArgTypes>
	FString WriteRemoteMethodPacket(const FString& MethodName, int32 MessageId, ArgTypes... Args)
	{
		FString MethodPacketString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MethodPacketString);
		Writer->WriteObjectStart();
		Writer->WriteValue(MixerChatStringConstants::FieldNames::Type, MixerChatStringConstants::MessageTypes::Method);
		Writer->WriteValue(MixerChatStringConstants::FieldNames::Method, MethodName);
		Writer->WriteArrayStart(MixerChatStringConstants::FieldNames::Arguments);
		WriteRemoteMethodParams(Writer.Get(), Args...);
		Writer->WriteArrayEnd();
		Writer->WriteValue(MixerChatStringConstants::FieldNames::Id, MessageId);
		Writer->WriteObjectEnd();
		Writer->Close();

		return MethodPacketString;
	}
}


struct FChatMessageMixerImpl : public FChatMessageMixer
{
public:
	FChatMessageMixerImpl(const FGuid& InMessageId, TSharedRef<const FUniqueNetId> InUser, const FString& InText)
		: FChatMessageMixer(InUser, InText)
		, MessageId(InMessageId)
	{
	}

	void FlagAsDeleted()
	{
		MessageText.Empty();
	}

public:
	FGuid MessageId;

	// Intrusive list to avoid double allocation for chat history
	// Would be nice to use TIntrusiveList, but that doesn't support
	// TSharedPtr as the link type, and IOnlineChat enforces that the
	// lifetime is managed by TSharedPtr
	TSharedPtr<FChatMessageMixerImpl> NextLink;
	TSharedPtr<FChatMessageMixerImpl> PrevLink;
};

FMixerChatConnection::~FMixerChatConnection()
{
	CloseWebSocket();
}

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
		ChatInterface->ConnectAttemptFinished(*User, RoomId, false, TEXT("Failed to send request for chat web socket connection info."));

		// Note: we have probably self-destructed at this point
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
				JsonObject->TryGetNumberField(MixerChatStringConstants::FieldNames::Id, ChannelId);
			}
		}
	}

	if (ChannelId != 0)
	{
		JoinDiscoveredChatChannel();
	}
	else
	{
		ChatInterface->ConnectAttemptFinished(*User, RoomId, false, TEXT("Could not find Mixer chat channel for room id."));

		// Note: we have probably self-destructed at this point
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
				if (JsonObject->TryGetArrayField(MixerChatStringConstants::FieldNames::Endpoints, JsonEndpoints))
				{
					for (const TSharedPtr<FJsonValue>& Endpoint : *JsonEndpoints)
					{
						Endpoints.Add(Endpoint->AsString());
					}

					JsonObject->TryGetStringField(MixerChatStringConstants::FieldNames::AuthKey, AuthKey);
					OpenWebSocket();
				}
			}
		}
	}

	// Should have a web socket going by now.
	if (!WebSocket.IsValid())
	{
		ChatInterface->ConnectAttemptFinished(*User, RoomId, false, TEXT("Failed to create web socket"));

		// Note: we have probably self-destructed at this point
	}
}

void FMixerChatConnection::OnChatSocketConnected()
{
	TSharedPtr<const FMixerLocalUser> CurrentUser = IMixerInteractivityModule::Get().GetCurrentUser();
	SendAuth(ChannelId, CurrentUser.Get(), AuthKey);
}

void FMixerChatConnection::OnChatConnectionError(const FString& ErrorMessage)
{
	UE_LOG(LogMixerChat, Warning, TEXT("Failed to connect chat web socket for room %s with error '%s'"), *RoomId, *ErrorMessage);
	ChatInterface->ConnectAttemptFinished(*User, RoomId, false, ErrorMessage);

	// Note: we have probably self-destructed at this point
}

void FMixerChatConnection::OnChatSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	// This should be a remote close since we unhook event handlers before closing on our end.
	// Do a full close and re-open of the websocket so as to (potentially) hit a different endpoint, per Mixer guidance.

	UE_LOG(LogMixerChat, Warning, TEXT("Chat websocket closed with reason '%s'."), *Reason);

	bool bWasReady = bIsReady;

	CloseWebSocket();

	if (bRejoinOnDisconnect)
	{
		UE_LOG(LogMixerChat, Warning, TEXT("Attempting automatic reconnect to %s."), *RoomId);
		OpenWebSocket();
	}
	else if (bWasReady)
	{
		ChatInterface->ExitRoomWithReason(*User, RoomId, bWasClean, Reason);

		// Note: we have probably self-destructed at this point
	}
	else 
	{
		ChatInterface->ConnectAttemptFinished(*User, RoomId, false, Reason);

		// Note: we have probably self-destructed at this point
	}
}

void FMixerChatConnection::OnChatPacket(const FString& PacketJsonString)
{
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(PacketJsonString);
	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) &&
		JsonObject.IsValid())
	{
		FString MessageType;
		if (JsonObject->TryGetStringField(MixerChatStringConstants::FieldNames::Type, MessageType))
		{
			if (MessageType == MixerChatStringConstants::MessageTypes::Reply)
			{
				int32 ReplyingToMessageId;
				if (!JsonObject->TryGetNumberField(MixerChatStringConstants::FieldNames::Id, ReplyingToMessageId))
				{
					UE_LOG(LogMixerChat, Error, TEXT("Missing id field for chat reply (full payload %s)"), *PacketJsonString);
					return;
				}

				FReplyHandler Handler;
				if (ReplyHandlers.RemoveAndCopyValue(ReplyingToMessageId, Handler))
				{
					check(Handler != nullptr);
					(this->*Handler)(JsonObject.Get());
				}
				else
				{
					UE_LOG(LogMixerChat, Error, TEXT("Received unexpected reply for unknown message id %d (full payload %s)"), ReplyingToMessageId, *PacketJsonString);
				}
			}
			else if (MessageType == MixerChatStringConstants::MessageTypes::Event)
			{
				FString EventType;
				if (JsonObject->TryGetStringField(MixerChatStringConstants::FieldNames::Event, EventType))
				{
					if (EventType == MixerChatStringConstants::EventTypes::Welcome)
					{
						// Welcomed by the server.  We are now fully connected.
						// But we have not necessarily completed auth.  That means we should use the
						// reply to the auth method call (which occurs even for anonymous connections)
						// to trigger the join event, otherwise callers might initially see operations
						// that require auth fail.
					}
					else
					{
						// Data field is important for all the following event types
						const TSharedPtr<FJsonObject>* Data;
						if (!JsonObject->TryGetObjectField(MixerChatStringConstants::FieldNames::Data, Data))
						{
							UE_LOG(LogMixerChat, Error, TEXT("Missing data field for chat event of type %s (full payload %s)"), *EventType, *PacketJsonString);
							return;
						}

						check(Data != nullptr);
						check(Data->IsValid());

						if (EventType == MixerChatStringConstants::EventTypes::ChatMessage)
						{
							HandleChatEventDataObject(Data->Get(), true);
						}
						else if (EventType == MixerChatStringConstants::EventTypes::UserJoin)
						{
							FString JoiningUser;
							if ((*Data)->TryGetStringField(MixerChatStringConstants::FieldNames::UserNameNoUnderscore, JoiningUser))
							{
								UE_LOG(LogMixerChat, Log, TEXT("%s is joining %s's chat channel"), *JoiningUser, *RoomId);

								// @TODO: we should probably create a derived FUniqueNetId for Mixer.
								TSharedRef<FUniqueNetId> JoiningId = MakeShared<FUniqueNetIdString>(JoiningUser);
								ChatInterface->TriggerOnChatRoomMemberJoinDelegates(*User, RoomId, *JoiningId);
							}
						}
						else if (EventType == MixerChatStringConstants::EventTypes::UserLeave)
						{
							FString LeavingUser;
							if ((*Data)->TryGetStringField(MixerChatStringConstants::FieldNames::UserNameNoUnderscore, LeavingUser))
							{
								UE_LOG(LogMixerChat, Log, TEXT("%s is leaving %s's chat channel"), *LeavingUser, *RoomId);

								// @TODO: we should probably create a derived FUniqueNetId for Mixer.
								TSharedRef<FUniqueNetId> LaavingId = MakeShared<FUniqueNetIdString>(LeavingUser);
								ChatInterface->TriggerOnChatRoomMemberExitDelegates(*User, RoomId, *LaavingId);
							}
						}
						else if (EventType == MixerChatStringConstants::EventTypes::DeleteMessage)
						{
							FString IdString;
							if (!(*Data)->TryGetStringField(MixerChatStringConstants::FieldNames::Id, IdString))
							{
								UE_LOG(LogMixerChat, Error, TEXT("Missing id field for delete message event"));
								return;
							}

							FGuid MessageId;
							if (!FGuid::Parse(IdString, MessageId))
							{
								UE_LOG(LogMixerChat, Error, TEXT("id field %s for delete message event was not in the expected format (guid)"), *IdString);
								return;
							}

							DeleteFromChatHistoryIf([&MessageId](TSharedPtr<FChatMessageMixerImpl> ChatMessage)
							{
								return ChatMessage->MessageId == MessageId;
							});
						}
						else if (EventType == MixerChatStringConstants::EventTypes::ClearMessages)
						{
							DeleteFromChatHistoryIf([](TSharedPtr<FChatMessageMixerImpl>)
							{
								return true;
							});

							check(ChatHistoryNum == 0);
							check(!ChatHistoryNewest.IsValid());
							check(!ChatHistoryOldest.IsValid());
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

void FMixerChatConnection::HandleChatEventDataObject(FJsonObject* Payload, bool bSendEvents)
{
	FString FromUser;
	if (!Payload->TryGetStringField(MixerChatStringConstants::FieldNames::UserNameWithUnderscore, FromUser))
	{
		UE_LOG(LogMixerChat, Error, TEXT("Missing user_name field for chat event"));
		return;
	}

	const TSharedPtr<FJsonObject>* MessageObject;
	if (!Payload->TryGetObjectField(MixerChatStringConstants::FieldNames::Message, MessageObject))
	{
		UE_LOG(LogMixerChat, Error, TEXT("Missing message field for chat event"));
		return;
	}

	FString IdString;
	if (!Payload->TryGetStringField(MixerChatStringConstants::FieldNames::Id, IdString))
	{
		UE_LOG(LogMixerChat, Error, TEXT("Missing id field for chat event"));
		return;
	}

	FGuid MessageId;
	if (!FGuid::Parse(IdString, MessageId))
	{
		UE_LOG(LogMixerChat, Error, TEXT("id field %s for chat event was not in the expected format (guid)"), *IdString);
		return;
	}

	const TSharedPtr<FJsonObject>* Metadata;
	bool bIsWhisper = false;
	bool bIsAction = false;
	if ((*MessageObject)->TryGetObjectField(MixerChatStringConstants::FieldNames::Meta, Metadata))
	{
		(*Metadata)->TryGetBoolField(MixerChatStringConstants::FieldNames::Whisper, bIsWhisper);
		(*Metadata)->TryGetBoolField(MixerChatStringConstants::FieldNames::Me, bIsAction);
	}

	FString MessageString;
	if (bIsAction)
	{
		MessageString = FromUser + TEXT(" ");
	}

	const TArray<TSharedPtr<FJsonValue>>* MessageFragmentArray;
	if (!(*MessageObject)->TryGetArrayField(MixerChatStringConstants::FieldNames::Message, MessageFragmentArray))
	{
		UE_LOG(LogMixerChat, Error, TEXT("Missing message.message array for chat event"));
		return;
	}

	for (const TSharedPtr<FJsonValue>& Fragment : *MessageFragmentArray)
	{
		const TSharedPtr<FJsonObject>* FragmentObj;
		if (Fragment->TryGetObject(FragmentObj))
		{
			HandleChatEventMessageFragment(MessageString, FragmentObj->Get());
		}
	}

	// @TODO: we should probably create a derived FUniqueNetId for Mixer.
	TSharedRef<FChatMessageMixerImpl> ChatMessageObject = MakeShared<FChatMessageMixerImpl>(MessageId, MakeShared<FUniqueNetIdString>(FromUser), MessageString);
	if (bIsWhisper && bSendEvents)
	{
		UE_LOG(LogMixerChat, Verbose, TEXT("Private message from %s: %s"), *FromUser, *MessageString);
		ChatInterface->TriggerOnChatPrivateMessageReceivedDelegates(*User, ChatMessageObject);
	}
	else
	{
		UE_LOG(LogMixerChat, Verbose, TEXT("Chat message from %s: %s"), *FromUser, *MessageString);

		if (ChatHistoryMax > 0)
		{
			ChatMessageObject->NextLink = ChatHistoryNewest;
			if (ChatHistoryNewest.IsValid())
			{
				ChatHistoryNewest->PrevLink = ChatMessageObject;
			}
			ChatHistoryNewest = ChatMessageObject;
			++ChatHistoryNum;
			if (!ChatHistoryOldest.IsValid())
			{
				ChatHistoryOldest = ChatMessageObject;
			}
			else if (ChatHistoryNum > ChatHistoryMax)
			{
				ChatHistoryOldest = ChatHistoryOldest->PrevLink;
				if (ChatHistoryOldest.IsValid())
				{
					check(ChatHistoryOldest->NextLink.IsValid());
					ChatHistoryOldest->NextLink->PrevLink.Reset();
					ChatHistoryOldest->NextLink.Reset();				
				}
				--ChatHistoryNum;
			}
		}

		if (bSendEvents)
		{
			ChatInterface->TriggerOnChatRoomMessageReceivedDelegates(*User, RoomId, ChatMessageObject);
		}
	}
}

void FMixerChatConnection::HandleChatEventMessageFragment(FString& MessageSoFar, FJsonObject* Fragment)
{
	FString FragmentType;
	if (!Fragment->TryGetStringField(MixerChatStringConstants::FieldNames::Type, FragmentType))
	{
		UE_LOG(LogMixerChat, Error, TEXT("Missing type field for chat message fragment."));
		return;
	}

	// For now just always append the fragment text no matter the type.
	// In the future we could perhaps add markup?
	FString FragmentText;
	if (!Fragment->TryGetStringField(MixerChatStringConstants::FieldNames::Text, FragmentText))
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

	FString MethodPacket = WriteRemoteMethodPacket(MixerChatStringConstants::MethodNames::Msg, MessageId, MessageBody);
	SendMethodPacket(MethodPacket, nullptr);

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

	FString MethodPacket = WriteRemoteMethodPacket(MixerChatStringConstants::MethodNames::Whisper, MessageId, ToUser, MessageBody);
	SendMethodPacket(MethodPacket, nullptr);

	return true;
}

void FMixerChatConnection::SendAuth(int32 ChannelId, const FMixerLocalUser* User, const FString& AuthKey)
{
	FString MethodPacket;
	if (User != nullptr && !AuthKey.IsEmpty())
	{
		UE_LOG(LogMixerChat, Log, TEXT("Authenticating to chat room %s as user '%s'"), *RoomId, *User->Name);
		MethodPacket = WriteRemoteMethodPacket(MixerChatStringConstants::MethodNames::Auth, MessageId, ChannelId, User->Id, AuthKey);
	}
	else
	{
		UE_LOG(LogMixerChat, Log, TEXT("Authenticating to chat room %s anonymously"), *RoomId);
		MethodPacket = WriteRemoteMethodPacket(MixerChatStringConstants::MethodNames::Auth, MessageId, ChannelId);
	}
	SendMethodPacket(MethodPacket, &FMixerChatConnection::HandleAuthReply);
}

void FMixerChatConnection::SendHistory(int32 MessageCount)
{
	FString MethodPacket = WriteRemoteMethodPacket(MixerChatStringConstants::MethodNames::History, MessageId, MessageCount);
	SendMethodPacket(MethodPacket, &FMixerChatConnection::HandleHistoryReply);
}

void FMixerChatConnection::SendMethodPacket(const FString& Payload, FReplyHandler Handler)
{
	check(!ReplyHandlers.Contains(MessageId));
	ReplyHandlers.Add(MessageId, Handler);
	++MessageId;

	WebSocket->Send(Payload);
}

void FMixerChatConnection::GetMessageHistory(int32 NumMessages, TArray< TSharedRef<FChatMessage> >& OutMessages)
{
	TSharedPtr<FChatMessageMixerImpl> ChatMessage = ChatHistoryNewest;
	while (ChatMessage.IsValid() &&
		(NumMessages == -1 || OutMessages.Num() < NumMessages))
	{
		OutMessages.Add(ChatMessage.ToSharedRef());
		ChatMessage = ChatMessage->NextLink;
	}
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
		WebSocket->OnConnected().AddRaw(this, &FMixerChatConnection::OnChatSocketConnected);
		WebSocket->OnConnectionError().AddRaw(this, &FMixerChatConnection::OnChatConnectionError);
		WebSocket->OnMessage().AddRaw(this, &FMixerChatConnection::OnChatPacket);
		WebSocket->OnClosed().AddRaw(this, &FMixerChatConnection::OnChatSocketClosed);

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

void FMixerChatConnection::DeleteFromChatHistoryIf(TFunctionRef<bool(TSharedPtr<FChatMessageMixerImpl>)> Predicate)
{
	// @TODO - pass moderator here when available
	TSharedPtr<FChatMessageMixerImpl> ChatMessage = ChatHistoryNewest;
	while (ChatMessage.IsValid())
	{
		if (Predicate(ChatMessage))
		{
			ChatMessage->FlagAsDeleted();
			if (ChatMessage == ChatHistoryNewest)
			{
				ChatHistoryNewest = ChatMessage->NextLink;
			}
			if (ChatMessage == ChatHistoryOldest)
			{
				ChatHistoryOldest = ChatMessage->PrevLink;
			}
			if (ChatMessage->NextLink.IsValid())
			{
				ChatMessage->NextLink->PrevLink = ChatMessage->PrevLink;
			}
			if (ChatMessage->PrevLink.IsValid())
			{
				ChatMessage->PrevLink->NextLink = ChatMessage->NextLink;
			}
			ChatMessage->NextLink.Reset();
			ChatMessage->PrevLink.Reset();
			--ChatHistoryNum;
			break;
		}
		ChatMessage = ChatMessage->NextLink;
	}
}


void FMixerChatConnection::HandleAuthReply(FJsonObject* Payload)
{
	const TSharedPtr<FJsonObject>* Error;
	if (Payload->TryGetObjectField(MixerChatStringConstants::FieldNames::Error, Error))
	{
		FString ErrorMessage;
		(*Error)->TryGetStringField(MixerChatStringConstants::FieldNames::Message, ErrorMessage);
		ChatInterface->ConnectAttemptFinished(*User, RoomId, false, ErrorMessage);

		// Note: we have probably self-destructed at this point
	}
	else
	{
		bIsReady = true;
		if (ChatHistoryMax > 0)
		{
			SendHistory(FMath::Min(ChatHistoryMax, 100));
		}
		// Maybe we have some interest in roles?

		ChatInterface->ConnectAttemptFinished(*User, RoomId, true, FString());
	}
}

void FMixerChatConnection::HandleHistoryReply(FJsonObject* Payload)
{
	const TArray<TSharedPtr<FJsonValue>>* Data;
	if (Payload->TryGetArrayField(MixerChatStringConstants::FieldNames::Data, Data))
	{
		// Stash the current history and then clear member pointers.
		// We'll splice what we have back on the front of the history
		// reported by the server.
		TSharedPtr<FChatMessageMixerImpl> LocalHistoryNewest = ChatHistoryNewest;
		TSharedPtr<FChatMessageMixerImpl> LocalHistoryOldest = ChatHistoryOldest;
		int32 LocalHistoryNum = ChatHistoryNum;
		int32 LocalHistoryMax = ChatHistoryMax;
		ChatHistoryNewest.Reset();
		ChatHistoryOldest.Reset();
		ChatHistoryNum = 0;
		ChatHistoryMax = LocalHistoryMax - LocalHistoryNum;

		// Oldest entry is at index 0 as reported by Mixer
		// Whereas we keep the newest entry at the head of the list,
		// which is where HandleChatMessagePacket pushes.
		for (const TSharedPtr<FJsonValue> HistoryEntry : *Data)
		{
			HandleChatEventDataObject(HistoryEntry->AsObject().Get(), false);
		}

		// Relink the history we'd already accumulated.
		// Possibly our history request crossed paths with some
		// new messages and we could have some dupes?
		if (!ChatHistoryNewest.IsValid())
		{
			ChatHistoryNewest = LocalHistoryNewest;
			ChatHistoryOldest = LocalHistoryOldest;
		}
		else if (LocalHistoryOldest.IsValid())
		{
			int32 DupeCount = 0;
			FGuid IdToCheckForDupes = LocalHistoryOldest->MessageId;
			TSharedPtr<FChatMessageMixerImpl> ServerHistoryMessage = ChatHistoryNewest;
			while (ServerHistoryMessage.IsValid())
			{
				++DupeCount;
				if (ServerHistoryMessage->MessageId == IdToCheckForDupes)
				{
					break;
				}
				ServerHistoryMessage = ServerHistoryMessage->NextLink;
			}

			if (ServerHistoryMessage.IsValid())
			{
				LocalHistoryOldest->NextLink = ServerHistoryMessage->NextLink;
				if (ServerHistoryMessage->NextLink.IsValid())
				{
					ServerHistoryMessage->NextLink->PrevLink = LocalHistoryOldest;
				}
				ChatHistoryNum -= DupeCount;
			}
			else
			{
				LocalHistoryOldest->NextLink = ChatHistoryNewest;
				ChatHistoryNewest->PrevLink = LocalHistoryOldest;
			}

			ChatHistoryNewest = LocalHistoryNewest;
			ChatHistoryNum += LocalHistoryNum;
			ChatHistoryMax = LocalHistoryMax;
		}
	}
	else
	{
	}
}


