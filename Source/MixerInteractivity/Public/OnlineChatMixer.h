#pragma once

#include "Interfaces/OnlineChatInterface.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

/**
* Implementation of FChatMessage for messages received via Mixer.
* See FChatMessage for interface method details.
*/
struct FChatMessageMixer : public FChatMessage
{
public:
	virtual bool IsWhisper() = 0;
	virtual bool IsAction() = 0;
	virtual bool IsModerated() = 0;
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
* Extension of IOnlineChat for Mixer.
* Rooms map to Mixer channels and are identified by the username of their owner.
*/
class IOnlineChatMixer : public IOnlineChat
{
public:
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnChatRoomMessagesCleared, const FUniqueNetId&, const FChatRoomId&);
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnChatRoomUserPurged, const FUniqueNetId&, const FChatRoomId&, const FUniqueNetId&);

};