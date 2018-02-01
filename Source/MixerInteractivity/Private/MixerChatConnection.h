#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineChatInterface.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMixerChat, Log, All);

class FMixerChatConnection : public TSharedFromThis<FMixerChatConnection>
{
public:
	FMixerChatConnection(class FOnlineChatMixer* InChatInterface, const FUniqueNetId& UserId, const FChatRoomId& InRoomId, const FChatRoomConfig& Config)
		: ChatInterface(InChatInterface)
		, User(UserId.AsShared())
		, RoomId(InRoomId)
		, ChannelId(0)
		, MessageId(0)
		, ChatHistoryNum(0)
		, ChatHistoryMax(10) // @TODO: pull from config once available
		, bIsReady(false)
		, bRejoinOnDisconnect(Config.bRejoinOnDisconnect)
	{
	}

	virtual ~FMixerChatConnection();

	bool Init();

	bool SendChatMessage(const FString& MessageBody);
	bool SendWhisper(const FString& ToUser, const FString& MessageBody);

	const FChatRoomId& GetRoom() const			{ return RoomId; }
	bool IsAnonymous() const					{ return AuthKey.IsEmpty(); }

	void SetRejoinOnDisconnect(bool bInRejoin)	{ bRejoinOnDisconnect = bInRejoin; }

	void GetMessageHistory(int32 NumMessages, TArray<TSharedRef<FChatMessage>>& OutMessages);

private:

	void OpenWebSocket();
	void CloseWebSocket();

	void JoinDiscoveredChatChannel();

	void OnGetChannelInfoForRoomIdComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void OnDiscoverChatServersComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	void OnChatSocketConnected();
	void OnChatConnectionError(const FString& ErrorMessage);
	void OnChatPacket(const FString& PacketJsonString);
	void OnChatSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);

	void HandleChatEventDataObject(class FJsonObject* Payload, bool bSendEvents);
	void HandleChatEventMessageFragment(FString& MessageSoFar, class FJsonObject* Fragment);

	void DeleteFromChatHistoryIf(TFunctionRef<bool(TSharedPtr<struct FChatMessageMixerImpl>)> Predicate);

private:
	typedef void (FMixerChatConnection::*FReplyHandler)(class FJsonObject*);

	void SendAuth(int32 ChannelId, const struct FMixerLocalUser* User, const FString& AuthKey);
	void HandleAuthReply(class FJsonObject* Payload);

	void SendHistory(int32 MessageCount);
	void HandleHistoryReply(class FJsonObject* Payload);

	void SendMethodPacket(const FString& Payload, FReplyHandler Handler);

private:
	class FOnlineChatMixer* ChatInterface;
	TSharedRef<const FUniqueNetId> User;
	FChatRoomId RoomId;
	FString AuthKey;
	TArray<FString> Endpoints;
	TSharedPtr<class IWebSocket> WebSocket;
	TMap<int32, FReplyHandler> ReplyHandlers;
	TSharedPtr<struct FChatMessageMixerImpl> ChatHistoryNewest;
	TSharedPtr<struct FChatMessageMixerImpl> ChatHistoryOldest;
	int32 ChatHistoryNum;
	int32 ChatHistoryMax;
	int32 ChannelId;
	int32 MessageId;
	bool bIsReady;
	bool bRejoinOnDisconnect;
};
