//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineChatInterface.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "OnlineChatMixerPrivate.h"
#include "MixerWebSocketOwnerBase.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMixerChat, Log, All);

class FMixerChatConnection
	: public TMixerWebSocketOwnerBase<FMixerChatConnection>
	, public TSharedFromThis<FMixerChatConnection>
{
public:
	FMixerChatConnection(class FOnlineChatMixer* InChatInterface, const FUniqueNetId& UserId, const FChatRoomId& InRoomId, const FChatRoomConfig& Config);
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

protected:
	virtual void RegisterAllServerMessageHandlers();
	virtual bool OnUnhandledServerMessage(const FString& MessageType, const TSharedPtr<FJsonObject> Params) { return false; }

	virtual void HandleSocketConnected();
	virtual void HandleSocketConnectionError();
	virtual void HandleSocketClosed(bool bWasClean);

private:

	void JoinDiscoveredChatChannel();

	void OnGetChannelInfoForRoomIdComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void OnDiscoverChatServersComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

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
	bool HandleAuthReply(class FJsonObject* JsonObj);
	bool HandleHistoryReply(class FJsonObject* JsonObj);

private:
	class FOnlineChatMixer* ChatInterface;
	TSharedRef<const FUniqueNetId> User;
	FChatRoomId RoomId;
	FString AuthKey;
	TArray<FString> Endpoints;
	TMap<FUniqueNetIdMixer, TSharedPtr<FMixerChatUser>> CachedUsers;
	TSharedPtr<struct FChatPollMixerImpl> ActivePoll;
	TSharedPtr<struct FChatMessageMixerImpl> ChatHistoryNewest;
	TSharedPtr<struct FChatMessageMixerImpl> ChatHistoryOldest;
	int32 ChatHistoryNum;
	int32 ChatHistoryMax;
	int32 ChannelId;
	bool bIsReady;
	bool bRejoinOnDisconnect;

	struct
	{
		bool bConnect;
		bool bChat;
		bool bWhisper;
		bool bPollStart;
		bool bPollVote;
		bool bClearMessages;
		bool bPurge;
		bool bGiveawayStart;
	} Permissions;
};
