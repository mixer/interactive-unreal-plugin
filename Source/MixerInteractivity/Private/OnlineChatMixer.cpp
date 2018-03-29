//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
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
		NewConnection = DefaultChatConnection = MakeShared<FMixerChatConnection>(this, UserId, CurrentUser->Name, ChatRoomConfig);
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
					NewConnection = Connection = MakeShared<FMixerChatConnection>(this, UserId, RoomId, ChatRoomConfig);
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
			NewConnection = MakeShared<FMixerChatConnection>(this, UserId, RoomId, ChatRoomConfig);
			AdditionalChatConnections.Add(NewConnection.ToSharedRef());
		}
	}

	bool bStartedConnection = NewConnection->Init();
	if (!bStartedConnection)
	{
		UE_LOG(LogMixerChat, Warning, TEXT("Error initializing connection sequence for room %s."), *RoomId);
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
	return ExitRoomWithReason(UserId, RoomId, true, TEXT("Called ExitRoom"));
}

bool FOnlineChatMixer::SendRoomChat(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& MsgBody)
{
	TSharedPtr<FMixerChatConnection> Connection = FindConnectionForRoomId(RoomId);
	if (Connection.IsValid())
	{
		return Connection->SendChatMessage(MsgBody);
	}
	else
	{
		return false;
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

bool FOnlineChatMixer::GetMembers(const FUniqueNetId& UserId, const FChatRoomId& RoomId, TArray< TSharedRef<FChatRoomMember> >& OutMembers)
{
	TSharedPtr<FMixerChatConnection> Connection = FindConnectionForRoomId(RoomId);
	if (Connection.IsValid())
	{
		Connection->GetAllCachedUsers(OutMembers);
		return true;
	}
	else
	{
		return false;
	}
}

TSharedPtr<FChatRoomMember> FOnlineChatMixer::GetMember(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId)
{
	TSharedPtr<FMixerChatConnection> Connection = FindConnectionForRoomId(RoomId);
	return Connection.IsValid() ? Connection->FindUser(MemberId) : nullptr;
}


bool FOnlineChatMixer::GetLastMessages(const FUniqueNetId& UserId, const FChatRoomId& RoomId, int32 NumMessages, TArray< TSharedRef<FChatMessage> >& OutMessages)
{
	TSharedPtr<FMixerChatConnection> Connection = FindConnectionForRoomId(RoomId);
	if (Connection.IsValid())
	{
		Connection->GetMessageHistory(NumMessages, OutMessages);
		return true;
	}
	else
	{
		return false;
	}
}

bool FOnlineChatMixer::IsMessageFromLocalUser(const FUniqueNetId& UserId, const FChatMessage& Message, const bool bIncludeExternalInstances)
{
	return UserId == *Message.GetUserId();
}

bool FOnlineChatMixer::StartPoll(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Question, const TArray<FString>& Answers, FTimespan Duration)
{
	TSharedPtr<FMixerChatConnection> Connection = FindConnectionForRoomId(RoomId);
	if (Connection.IsValid())
	{
		return Connection->SendVoteStart(Question, Answers, Duration);
	}
	else
	{
		return false;
	}
}

bool FOnlineChatMixer::VoteInPoll(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FChatPollMixer& Poll, int32 AnswerIndex)
{
	TSharedPtr<FMixerChatConnection> Connection = FindConnectionForRoomId(RoomId);
	if (Connection.IsValid())
	{
		return Connection->SendVoteChoose(Poll, AnswerIndex);
	}
	else
	{
		return false;
	}
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

void FOnlineChatMixer::ConnectAttemptFinished(const FUniqueNetId& UserId, const FChatRoomId& RoomId, bool bSuccess, const FString& ErrorMessage)
{
	if (!bSuccess)
	{
		RemoveConnectionForRoom(RoomId);
	}

	TriggerOnChatRoomJoinPublicDelegates(UserId, RoomId, bSuccess, ErrorMessage);
}

bool FOnlineChatMixer::ExitRoomWithReason(const FUniqueNetId& UserId, const FChatRoomId& RoomId, bool bIsClean, const FString& Reason)
{
	bool bExited = RemoveConnectionForRoom(RoomId);

	if (bExited)
	{
		TriggerOnChatRoomExitDelegates(UserId, RoomId, bIsClean, Reason);
	}

	return bExited;
}

bool FOnlineChatMixer::RemoveConnectionForRoom(const FChatRoomId& RoomId)
{
	bool bFound = false;

	if (IsDefaultChatRoom(RoomId))
	{
		DefaultChatConnection.Reset();
		bFound = true;
	}
	else
	{
		for (int32 i = 0; i < AdditionalChatConnections.Num(); ++i)
		{
			if (AdditionalChatConnections[i]->GetRoom() == RoomId)
			{
				AdditionalChatConnections.RemoveAtSwap(i);
				bFound = true;
			}
		}
	}

	return bFound;
}

TSharedPtr<FMixerChatConnection> FOnlineChatMixer::FindConnectionForRoomId(const FChatRoomId& RoomId)
{
	if (IsDefaultChatRoom(RoomId))
	{
		return DefaultChatConnection;
	}
	else
	{
		for (TSharedRef<FMixerChatConnection>& Connection : AdditionalChatConnections)
		{
			if (Connection->GetRoom() == RoomId)
			{
				return Connection;
			}
		}

		return nullptr;
	}
}