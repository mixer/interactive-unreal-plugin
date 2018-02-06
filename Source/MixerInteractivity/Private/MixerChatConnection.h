#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineChatInterface.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "OnlineChatMixerPrivate.h"

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
	bool SendVoteStart(const FString& Question, const TArray<FString>& Answers, FTimespan Duration);
	bool SendVoteChoose(const FChatPollMixer& Poll, int32 AnswerIndex);

	const FChatRoomId& GetRoom() const			{ return RoomId; }
	bool IsAnonymous() const					{ return AuthKey.IsEmpty(); }

	void SetRejoinOnDisconnect(bool bInRejoin)	{ bRejoinOnDisconnect = bInRejoin; }

	void GetMessageHistory(int32 NumMessages, TArray<TSharedRef<FChatMessage>>& OutMessages) const;

	void GetAllCachedUsers(TArray< TSharedRef<FChatRoomMember> >& OutUsers) const;

	TSharedPtr<FMixerChatUser> FindUser(const FUniqueNetId& UserId) const;

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

	bool OnChatPacketInternal(class FJsonObject* JsonObj);

	bool HandleWelcomeEvent(class FJsonObject* JsonObj);
	bool HandleChatMessageEvent(class FJsonObject* JsonObj);
	bool HandleUserJoinEvent(class FJsonObject* JsonObj);
	bool HandleUserLeaveEvent(class FJsonObject* JsonObj);
	bool HandleDeleteMessageEvent(class FJsonObject* JsonObj);
	bool HandleClearMessagesEvent(class FJsonObject* JsonObj);
	bool HandlePurgeMessageEvent(class FJsonObject* JsonObj);
	bool HandlePollStartEvent(class FJsonObject* JsonObj);
	bool HandlePollEndEvent(class FJsonObject* JsonObj);

	bool HandleChatMessageEventInternal(class FJsonObject* JsonObj, TSharedPtr<FChatMessageMixerImpl>& OutMessage);
	bool HandleChatMessageEventMessageObject(class FJsonObject* JsonObj, FChatMessageMixerImpl* ChatMessage);
	bool HandleChatMessageEventMessageArrayEntry(class FJsonObject* JsonObj, FChatMessageMixerImpl* ChatMessage);
	bool HandlePollEndEventInternal(class FJsonObject* JsonObj);
	bool UpdateActivePollFromServer(class FJsonObject* JsonObj, bool& bOutAnythingChanged);

	void AddMessageToChatHistory(TSharedRef<struct FChatMessageMixerImpl> ChatMessage);
	void DeleteFromChatHistoryIf(TFunctionRef<bool(TSharedPtr<FChatMessageMixerImpl>)> Predicate);

private:
	typedef bool (FMixerChatConnection::*FServerMessageHandler)(class FJsonObject*);

	void SendAuth(int32 ChannelId, const FMixerLocalUser* User, const FString& AuthKey);
	void SendHistory(int32 MessageCount);

	bool HandleAuthReply(class FJsonObject* JsonObj);
	bool HandleHistoryReply(class FJsonObject* JsonObj);

	void SendMethodPacket(const FString& Payload, FServerMessageHandler Handler);

	FServerMessageHandler GetEventHandler(const FString& EventType);

private:
	class FOnlineChatMixer* ChatInterface;
	TSharedRef<const FUniqueNetId> User;
	FChatRoomId RoomId;
	FString AuthKey;
	TArray<FString> Endpoints;
	TSharedPtr<class IWebSocket> WebSocket;
	TMap<FUniqueNetIdMixer, TSharedPtr<FMixerChatUser>> CachedUsers;
	TMap<int32, FServerMessageHandler> ReplyHandlers;
	TSharedPtr<struct FChatPollMixerImpl> ActivePoll;
	TSharedPtr<struct FChatMessageMixerImpl> ChatHistoryNewest;
	TSharedPtr<struct FChatMessageMixerImpl> ChatHistoryOldest;
	int32 ChatHistoryNum;
	int32 ChatHistoryMax;
	int32 ChannelId;
	int32 MessageId;
	bool bIsReady;
	bool bRejoinOnDisconnect;
};
