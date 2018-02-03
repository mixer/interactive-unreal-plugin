#pragma once

#include "OnlineChatMixer.h"
#include "MixerInteractivityTypes.h"
#include "Guid.h"

class FUniqueNetIdMixer : public FUniqueNetId
{
public:
	FUniqueNetIdMixer(int32 InMixerId)
		: MixerId(InMixerId)
	{

	}

	explicit FUniqueNetIdMixer(const FUniqueNetId& Src)
	{
		if (Src.GetSize() == sizeof(MixerId))
		{
			MixerId = static_cast<const FUniqueNetIdMixer&>(Src).MixerId;
		}
	}

	virtual const uint8* GetBytes() const override
	{
		return reinterpret_cast<const uint8*>(&MixerId);
	}

	virtual int32 GetSize() const override
	{
		return sizeof(MixerId);
	}

	virtual bool IsValid() const override
	{
		return MixerId != 0;
	}

	virtual FString ToString() const
	{
		return FString::Printf(TEXT("%d"), MixerId);
	}

	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("MixerId: %d"), MixerId);
	}

	friend uint32 GetTypeHash(const FUniqueNetIdMixer& Unid)
	{
		return GetTypeHash(Unid.MixerId);
	}

private:
	int32 MixerId;
};

struct FMixerChatUser : public FMixerUser, public FChatRoomMember
{
public:
	FMixerChatUser(const FString& InName, int32 InId)
		: NetId(MakeShared<FUniqueNetIdMixer>(Id))
	{
		Name = InName;
		Id = InId;
		Level = 0;
	}

	// FChatRoomMember interface
	virtual const TSharedRef<const FUniqueNetId>& GetUserId() const		{ return NetId; }
	virtual const FString& GetNickname() const							{ return Name; }

	const FUniqueNetIdMixer& GetUniqueNetId() const						{ return static_cast<const FUniqueNetIdMixer&>(NetId.Get()); }

private:
	TSharedRef<const FUniqueNetId> NetId;
};

struct FChatMessageMixerImpl : public FChatMessageMixer
{
public:
	FChatMessageMixerImpl(const FGuid& InMessageId, TSharedRef<const FMixerChatUser> InFromUser)
		: MessageId(InMessageId)
		, FromUser(InFromUser)
		, Timestamp(FDateTime::Now())
		, bIsWhisper(false)
		, bIsAction(false)
		, bIsModerated(false)
	{
	}

	// FChatMessage methods
	virtual const TSharedRef<const FUniqueNetId>& GetUserId() const override	{ return FromUser->GetUserId(); }
	virtual const FString& GetNickname() const override							{ return FromUser->Name; }
	virtual const FString& GetBody() const override								{ return Body; }
	virtual const FDateTime& GetTimestamp() const override						{ return Timestamp; }

	// FChatMessageMixer methods
	virtual bool IsWhisper() override											{ return bIsWhisper; }
	virtual bool IsAction() override											{ return bIsAction; }
	virtual bool IsModerated() override											{ return bIsModerated; }

	const FMixerChatUser& GetSender()											{ return FromUser.Get(); }
	const FGuid& GetMessageId()													{ return MessageId; }

	void FlagAsDeleted()
	{
		Body.Empty();
		bIsModerated = true;
	}

	void AppendBodyFragment(const FString& InBodyFragment)
	{
		Body += InBodyFragment;
	}

	void FlagAsWhisper()
	{
		bIsWhisper = true;
	}

	void FlagAsAction()
	{
		if (!bIsAction)
		{
			bIsAction = true;
			Body = FromUser->Name + TEXT(" ") + Body;
		}
	}

private:
	FGuid MessageId;
	TSharedRef<const FMixerChatUser> FromUser;
	FString Body;
	FDateTime Timestamp;
	bool bIsWhisper;
	bool bIsAction;
	bool bIsModerated;

public:
	// Intrusive list to avoid double allocation for chat history
	// Would be nice to use TIntrusiveList, but that doesn't support
	// TSharedPtr as the link type, and IOnlineChat enforces that the
	// lifetime is managed by TSharedPtr
	TSharedPtr<FChatMessageMixerImpl> NextLink;
	TSharedPtr<FChatMessageMixerImpl> PrevLink;
};

class FOnlineChatMixer : public IOnlineChatMixer, public TSharedFromThis<FOnlineChatMixer>
{
public:
	virtual bool CreateRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nickname, const FChatRoomConfig& ChatRoomConfig) override;

	virtual bool ConfigureRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FChatRoomConfig& ChatRoomConfig) override { return false; }

	virtual bool JoinPublicRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nickname, const FChatRoomConfig& ChatRoomConfig) override;

	virtual bool JoinPrivateRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nickname, const FChatRoomConfig& ChatRoomConfig) override { return false; }

	virtual bool ExitRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId) override;

	virtual bool SendRoomChat(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& MsgBody) override;

	virtual bool SendPrivateChat(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId, const FString& MsgBody) override;

	virtual bool IsChatAllowed(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId) const override;

	virtual void GetJoinedRooms(const FUniqueNetId& UserId, TArray<FChatRoomId>& OutRooms) override;

	virtual TSharedPtr<FChatRoomInfo> GetRoomInfo(const FUniqueNetId& UserId, const FChatRoomId& RoomId) override;

	virtual bool GetMembers(const FUniqueNetId& UserId, const FChatRoomId& RoomId, TArray< TSharedRef<FChatRoomMember> >& OutMembers) override;

	virtual TSharedPtr<FChatRoomMember> GetMember(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId) override;

	virtual bool GetLastMessages(const FUniqueNetId& UserId, const FChatRoomId& RoomId, int32 NumMessages, TArray< TSharedRef<FChatMessage> >& OutMessages) override;

	virtual void DumpChatState() const override {}

public:
	void ConnectAttemptFinished(const FUniqueNetId& UserId, const FChatRoomId& RoomId, bool bSuccess, const FString& ErrorMessage);
	bool ExitRoomWithReason(const FUniqueNetId& UserId, const FChatRoomId& RoomId, bool bIsClean, const FString& Reason);

private:

	bool IsDefaultChatRoom(const FChatRoomId& RoomId) const;
	bool WillJoinAnonymously() const;
	bool RemoveConnectionForRoom(const FChatRoomId& RoomId);

	TSharedPtr<class FMixerChatConnection> FindConnectionForRoomId(const FChatRoomId& RoomId);

	/**
	* Connection to the default chat channel for the current Mixer session -
	* that is, the channel owned by the current Mixer user.
	*/
	TSharedPtr<class FMixerChatConnection> DefaultChatConnection;

	/** Connection to additional chat channels that we may want to interact with. */
	TArray<TSharedRef<class FMixerChatConnection>> AdditionalChatConnections;
};