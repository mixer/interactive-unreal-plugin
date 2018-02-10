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

#include "Interfaces/OnlineChatInterface.h"

/**
* Implementation of FChatMessage for messages received via Mixer.
* See FChatMessage for interface method details.
*/
struct FChatMessageMixer : public FChatMessage
{
public:
	/** Check whether this message object represents a whisper (private message) */ 
	virtual bool IsWhisper() const = 0;

	/** Check whether this message object represents an action (entered as /me {does something}) */
	virtual bool IsAction() const = 0;

	/** Check whether a moderator has removed this message after it was originally sent */
	virtual bool IsModerated() const = 0;
};

/** Represents a vote taking place in a Mixer channel*/
struct FChatPollMixer
{
public:
	virtual ~FChatPollMixer() {}

	/** Get an object representing the user who started the poll */
	virtual TSharedRef<const FChatRoomMember> GetAskingUser() const = 0;

	/** Get the question being asked */
	virtual const FString& GetQuestion() const = 0;

	/** Get the number of available answers to the poll question that users may vote on. */
	virtual int32 GetNumAnswers() const = 0;

	/** Get one of the available answers to the poll question */
	virtual const FString& GetAnswer(int32 Index) const = 0;

	/** Get the number of votes cast so far for a specific answer to the poll question */
	virtual int32 GetNumVotersForAnswer(int32 Index) const = 0;

	/** Get the server time at which this poll will end. */
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
	/**
	* Attempt to start a poll in a given Mixer channel (room).  Note that starting polls requires
	* a different permission to normal chat usage, and there may only be one poll active at a time
	* per channel.
	*
	* @param UserId		id of the user starting the poll
	* @param RoomId		id of the room in which the poll should be started.  For Mixer chat this is the owning user name.
	* @param Question	string that should be displayed in the channel as the poll question.
	* @param Answers	array of strings representing the possible answers to the question that users can vote on.
	* @param Duration	length of time for which the poll should be active.  Once this time expires on the server the poll will automatically close and a poll end event will occur.
	*
	* @return			whether or not the poll request was sent to the server.  Will return false if the user does not have permission to start polls.
	*/
	virtual bool StartPoll(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Question, const TArray<FString>& Answers, FTimespan Duration) = 0;

	/**
	* Attempt to voote in a poll in a given Mixer channel (room).  Note that voting in polls requires
	* a different permission to normal chat usage, you may only vote in an active poll, and there may
	* only be one poll active at a time per channel.
	*
	* @param UserId			id of the user voting in the poll
	* @param RoomId			id of the room in which the poll is active  For Mixer chat this is the owning user name.
	* @param Poll			the poll in which to vote.  This should match the active poll in the channel.
	* @param AnswerIndex	index into the poll's Answers collection for which the user wishes to cast a vote.
	*
	* @return				whether or not the vote was sent to the server.  Will return false if the user does not have permission to vote in polls.
	*/
	virtual bool VoteInPoll(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FChatPollMixer& Poll, int32 AnswerIndex) = 0;

	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnChatRoomMessagesCleared, const FUniqueNetId&, const FChatRoomId&);
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnChatRoomUserPurged, const FUniqueNetId&, const FChatRoomId&, const FUniqueNetId&);
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnChatRoomPollStart, const FUniqueNetId&, const FChatRoomId&, const TSharedRef<FChatPollMixer>&);
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnChatRoomPollUpdate, const FUniqueNetId&, const FChatRoomId&, const TSharedRef<FChatPollMixer>&);
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnChatRoomPollEnd, const FUniqueNetId&, const FChatRoomId&, const TSharedRef<FChatPollMixer>&);

};