#pragma once

#include "Interfaces/OnlineChatInterface.h"

/**
* Implementation of FChatMessage for messages received via Mixer.
* See FChatMessage for interface method details.
*/
struct FChatMessageMixer : public FChatMessage
{
public:
	virtual bool IsWhisper() const = 0;
	virtual bool IsAction() const = 0;
	virtual bool IsModerated() const = 0;
};

struct FChatPollMixer
{
public:
	virtual ~FChatPollMixer() {}

	virtual TSharedRef<const FChatRoomMember> GetAskingUser() const = 0;
	virtual const FString& GetQuestion() const = 0;
	virtual int32 GetNumAnswers() const = 0;
	virtual const FString& GetAnswer(int32 Index) const = 0;
	virtual int32 GetNumVotersForAnswer(int32 Index) const = 0;
	virtual FDateTime GetEndTime() const = 0;
};

/**
* Delegate used when all messages in a chat room are deleted
*
* @param UserId user currently in the room
* @param RoomId room that member is in
*/
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnChatRoomMessagesCleared, const FUniqueNetId& /*UserId*/, const FChatRoomId& /*RoomId*/);
typedef FOnChatRoomMessagesCleared::FDelegate FOnChatRoomMessagesClearedDelegate;

/**
* Delegate used when a user is purged from a chat room (all messages deleted)
*
* @param UserId user currently in the room
* @param RoomId room that member is in
* @param PurgedId user that has been purged
*/
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnChatRoomUserPurged, const FUniqueNetId& /*UserId*/, const FChatRoomId& /*RoomId*/, const FUniqueNetId& /*PurgedId*/);
typedef FOnChatRoomUserPurged::FDelegate FOnChatRoomUserPurgedDelegate;

/**
* Delegate used when a poll starts in a Mixer chat channel
*
* @param UserId user currently in the room
* @param RoomId room that member is in
* @param ChatPoll object representing the poll that is starting
*/
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnChatRoomPollStart, const FUniqueNetId& /*UserId*/, const FChatRoomId& /*RoomId*/, const TSharedRef<FChatPollMixer>& /*ChatPoll*/);
typedef FOnChatRoomPollStart::FDelegate FOnChatRoomPollStartDelegate;

/**
* Delegate used when updated data is received for a poll in a Mixer chat channel
*
* @param UserId user currently in the room
* @param RoomId room that member is in
* @param ChatPoll object representing the poll that has been updated
*/
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnChatRoomPollUpdate, const FUniqueNetId& /*UserId*/, const FChatRoomId& /*RoomId*/, const TSharedRef<FChatPollMixer>& /*ChatPoll*/);
typedef FOnChatRoomPollUpdate::FDelegate FOnChatRoomPollUpdateDelegate;

/**
* Delegate used when a poll ends in a Mixer chat channel
*
* @param UserId user currently in the room
* @param RoomId room that member is in
* @param ChatPoll object representing the poll that is ending
*/
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnChatRoomPollEnd, const FUniqueNetId& /*UserId*/, const FChatRoomId& /*RoomId*/, const TSharedRef<FChatPollMixer>& /*ChatPoll*/);
typedef FOnChatRoomPollEnd::FDelegate FOnChatRoomPollEndDelegate;

/**
* Extension of IOnlineChat for Mixer.
* Rooms map to Mixer channels and are identified by the username of their owner.
*/
class IOnlineChatMixer : public IOnlineChat
{
public:
	virtual bool StartPoll(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Question, const TArray<FString>& Answers, FTimespan Duration) = 0;
	virtual bool VoteInPoll(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FChatPollMixer& Poll, int32 AnswerIndex) = 0;


	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnChatRoomMessagesCleared, const FUniqueNetId&, const FChatRoomId&);
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnChatRoomUserPurged, const FUniqueNetId&, const FChatRoomId&, const FUniqueNetId&);
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnChatRoomPollStart, const FUniqueNetId&, const FChatRoomId&, const TSharedRef<FChatPollMixer>&);
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnChatRoomPollUpdate, const FUniqueNetId&, const FChatRoomId&, const TSharedRef<FChatPollMixer>&);
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnChatRoomPollEnd, const FUniqueNetId&, const FChatRoomId&, const TSharedRef<FChatPollMixer>&);

};