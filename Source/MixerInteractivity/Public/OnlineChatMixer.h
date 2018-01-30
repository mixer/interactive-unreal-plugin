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
	virtual const TSharedRef<const FUniqueNetId>& GetUserId() const override { return FromUser; }
	virtual const FString& GetNickname() const override { return FromUserName; }
	virtual const FString& GetBody() const override { return MessageText; }
	virtual const FDateTime& GetTimestamp() const override { return ReceivedAt; }

	FChatMessageMixer(TSharedRef<const FUniqueNetId> InUser, const FString& InText)
		: FromUser(InUser)
		, FromUserName(InUser->ToString())
		, ReceivedAt(FDateTime::Now())
		, MessageText(InText)
	{

	}

private:
	TSharedRef<const FUniqueNetId> FromUser;
	FString FromUserName;
	FString MessageText;
	FDateTime ReceivedAt;
};

/**
* Implementation of IOnlineChat for Mixer.
* Rooms map to Mixer channels and are identified by the username of their owner.
* See IOnlineChat for interface method details.
*/
class FOnlineChatMixer : public IOnlineChat, public TSharedFromThis<FOnlineChatMixer>
{
public:
	virtual bool CreateRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nickname, const FChatRoomConfig& ChatRoomConfig) override { return false; }

	virtual bool ConfigureRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FChatRoomConfig& ChatRoomConfig) override { return false; }

	virtual bool JoinPublicRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nickname, const FChatRoomConfig& ChatRoomConfig) override;

	virtual bool JoinPrivateRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nickname, const FChatRoomConfig& ChatRoomConfig) override { return false; }

	virtual bool ExitRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId) override { return false; }

	virtual bool SendRoomChat(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& MsgBody) override;

	virtual bool SendPrivateChat(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId, const FString& MsgBody) override;

	virtual bool IsChatAllowed(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId) const override;

	virtual void GetJoinedRooms(const FUniqueNetId& UserId, TArray<FChatRoomId>& OutRooms) override;

	virtual TSharedPtr<FChatRoomInfo> GetRoomInfo(const FUniqueNetId& UserId, const FChatRoomId& RoomId) override;

	virtual bool GetMembers(const FUniqueNetId& UserId, const FChatRoomId& RoomId, TArray< TSharedRef<FChatRoomMember> >& OutMembers) override { return false; }

	virtual TSharedPtr<FChatRoomMember> GetMember(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId) override { return false; }

	virtual bool GetLastMessages(const FUniqueNetId& UserId, const FChatRoomId& RoomId, int32 NumMessages, TArray< TSharedRef<FChatMessage> >& OutMessages) { return false; }

	virtual void DumpChatState() const override {}

private:

	bool IsDefaultChatRoom(const FChatRoomId& RoomId) const;
	bool WillJoinAnonymously() const;

	/**
	* Connection to the default chat channel for the current Mixer session -
	* that is, the channel owned by the current Mixer user.
	*/
	TSharedPtr<class FMixerChatConnection> DefaultChatConnection;

	/** Connection to additional chat channels that we may want to interact with. */
	TArray<TSharedRef<class FMixerChatConnection>> AdditionalChatConnections;
};