#include "OnlineChatMixer.h"

#include "MixerInteractivityModule.h"
#include "MixerInteractivityTypes.h"
#include "MixerInteractivityUserSettings.h"
#include "MixerChatConnection.h"

bool FOnlineChatMixer::CreateRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nickname, const FChatRoomConfig& ChatRoomConfig)
{
	// Based on the usage in UChatroom::CreateOrJoinChatRoom it appears that the expectation is that this falls back to a join operation
	UE_LOG(LogMixerChat, Warning, TEXT("Creation of new rooms not supported.  Treating as JoinPublicRoom"));
	return JoinPublicRoom(UserId, RoomId, Nickname, ChatRoomConfig);
}

bool FOnlineChatMixer::JoinPublicRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nickname, const FChatRoomConfig& ChatRoomConfig)
{
	TSharedPtr<FMixerChatConnection> NewConnection;

	if (IsDefaultChatRoom(RoomId))
	{
		if (DefaultChatConnection.IsValid())
		{
			if (DefaultChatConnection->IsAnonymous() && !WillJoinAnonymously())
			{
				// Allow upgrade to an auth'd connection.
				DefaultChatConnection->Cleanup();
			}
			else
			{
				// Already joined, no point in rejoining.
				UE_LOG(LogMixerChat, Warning, TEXT("A connection to room %s already exists."), *DefaultChatConnection->GetRoom());
				return false;
			}
		}

		TSharedPtr<const FMixerLocalUser> CurrentUser = IMixerInteractivityModule::Get().GetCurrentUser();
		check(CurrentUser.IsValid());
		NewConnection = DefaultChatConnection = MakeShared<FMixerChatConnection>(UserId, CurrentUser->Name);
	}
	else
	{
		if (RoomId.IsEmpty())
		{
			UE_LOG(LogMixerChat, Warning, TEXT("Joining chat without logging in requires explicitly specifying a room id (use the owner's Mixer user name)."))
			return false;
		}

		for (TSharedRef<FMixerChatConnection>& Connection : AdditionalChatConnections)
		{
			if (Connection->GetRoom() == RoomId)
			{
				if (Connection->IsAnonymous() && !WillJoinAnonymously())
				{
					// Allow upgrade to an auth'd connection.
					Connection->Cleanup();
					NewConnection = Connection = MakeShared<FMixerChatConnection>(UserId, RoomId);
					break;
				}
				else
				{
					UE_LOG(LogMixerChat, Warning, TEXT("A connection to room %s already exists."), *RoomId);
					return false;
				}
			}
		}

		if (!NewConnection.IsValid())
		{
			NewConnection = MakeShared<FMixerChatConnection>(UserId, RoomId);
			AdditionalChatConnections.Add(NewConnection.ToSharedRef());
		}
	}

	bool bStartedConnection = NewConnection->Init();
	if (!bStartedConnection)
	{
		UE_LOG(LogMixerChat, Warning, TEXT("Error initializing connection sequence for room %s."), *RoomId);
		NewConnection->Cleanup();
		if (NewConnection == DefaultChatConnection)
		{
			DefaultChatConnection.Reset();
		}
		else
		{
			AdditionalChatConnections.Remove(NewConnection.ToSharedRef());
		}
	}

	return bStartedConnection;
}

bool FOnlineChatMixer::ExitRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId)
{
	bool bExited = false;

	if (IsDefaultChatRoom(RoomId))
	{
		DefaultChatConnection->Cleanup();
		DefaultChatConnection.Reset();
		return true;
	}
	else
	{
		for (int32 i = 0; i < AdditionalChatConnections.Num(); ++i)
		{
			if (AdditionalChatConnections[i]->GetRoom() == RoomId)
			{
				AdditionalChatConnections[i]->Cleanup();
				AdditionalChatConnections.RemoveAtSwap(i);
				return true;
			}
		}
	}

	return false;
}

bool FOnlineChatMixer::SendRoomChat(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& MsgBody)
{
	if (IsDefaultChatRoom(RoomId))
	{
		if (DefaultChatConnection.IsValid())
		{
			return DefaultChatConnection->SendChatMessage(MsgBody);
		}
		else
		{
			return false;
		}
	}

	for (TSharedRef<FMixerChatConnection>& Connection : AdditionalChatConnections)
	{
		if (Connection->GetRoom() == RoomId)
		{
			return Connection->SendChatMessage(MsgBody);
		}
	}

	return false;
}

bool FOnlineChatMixer::SendPrivateChat(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId, const FString& MsgBody)
{
	// Currently the recipient must be in the default chat room
	if (DefaultChatConnection.IsValid())
	{
		return DefaultChatConnection->SendWhisper(RecipientId.ToString(), MsgBody);
	}

	return false;
}

bool FOnlineChatMixer::IsChatAllowed(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId) const
{
	// We can only send private messages in the default chat room.
	// We can't send messages if logged in anonymously.
	// @TODO: permissions check.
	return DefaultChatConnection.IsValid() && !DefaultChatConnection->IsAnonymous();
}

void FOnlineChatMixer::GetJoinedRooms(const FUniqueNetId& UserId, TArray<FChatRoomId>& OutRooms)
{
	if (DefaultChatConnection.IsValid())
	{
		OutRooms.Add(DefaultChatConnection->GetRoom());
	}

	for (TSharedRef<FMixerChatConnection>& Connection : AdditionalChatConnections)
	{
		OutRooms.Add(Connection->GetRoom());
	}
}

TSharedPtr<FChatRoomInfo> FOnlineChatMixer::GetRoomInfo(const FUniqueNetId& UserId, const FChatRoomId& RoomId)
{
	return nullptr;
}

bool FOnlineChatMixer::IsDefaultChatRoom(const FChatRoomId& RoomId) const
{
	TSharedPtr<const FMixerLocalUser> CurrentUser = IMixerInteractivityModule::Get().GetCurrentUser();
	if (!CurrentUser.IsValid())
	{
		return false;
	}

	if (RoomId.IsEmpty())
	{
		return true;
	}

	return RoomId == CurrentUser->Name;
}

bool FOnlineChatMixer::WillJoinAnonymously() const
{
	TSharedPtr<const FMixerLocalUser> CurrentUser = IMixerInteractivityModule::Get().GetCurrentUser();
	return !CurrentUser.IsValid();
}
