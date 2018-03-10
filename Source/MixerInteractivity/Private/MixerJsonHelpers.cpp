//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "MixerJsonHelpers.h"

namespace MixerStringConstants
{
	namespace MessageTypes
	{
		const FString Method = TEXT("method");
		const FString Reply = TEXT("reply");
		const FString Event = TEXT("event");
	}

	namespace MethodNames
	{
		const FString Auth = TEXT("auth");
		const FString Msg = TEXT("msg");
		const FString Whisper = TEXT("whisper");
		const FString History = TEXT("history");
		const FString VoteStart = TEXT("vote:start");
		const FString VoteChoose = TEXT("vote:choose");
	}

	namespace EventTypes
	{
		const FString Welcome = TEXT("WelcomeEvent");
		const FString ChatMessage = TEXT("ChatMessage");
		const FString UserJoin = TEXT("UserJoin");
		const FString UserLeave = TEXT("UserLeave");
		const FString DeleteMessage = TEXT("DeleteMessage");
		const FString ClearMessages = TEXT("ClearMessages");
		const FString PurgeMessage = TEXT("PurgeMessage");
		const FString PollStart = TEXT("PollStart");
		const FString PollEnd = TEXT("PollEnd");
	}

	namespace FieldNames
	{
		const FString Type = TEXT("type");
		const FString Event = TEXT("event");
		const FString Data = TEXT("data");
		const FString Message = TEXT("message");
		const FString UserNameNoUnderscore = TEXT("username");
		const FString UserNameWithUnderscore = TEXT("user_name");
		const FString Id = TEXT("id");
		const FString Meta = TEXT("meta");
		const FString Me = TEXT("me");
		const FString Whisper = TEXT("whisper");
		const FString Method = TEXT("method");
		const FString Arguments = TEXT("arguments");
		const FString Error = TEXT("error");
		const FString Text = TEXT("text");
		const FString Endpoints = TEXT("endpoints");
		const FString AuthKey = TEXT("authkey");
		const FString UserId = TEXT("user_id");
		const FString UserLevel = TEXT("user_level");
		const FString Q = TEXT("q");
		const FString EndsAt = TEXT("endsAt");
		const FString Voters = TEXT("voters");
		const FString Answers = TEXT("answers");
		const FString ResponsesByIndex = TEXT("responsesByIndex");
		const FString Author = TEXT("author");
		const FString Permissions = TEXT("permissions");
	}

	namespace Permissions
	{
		const FString Connect = TEXT("connect");
		const FString Chat = TEXT("chat");
		const FString Whisper = TEXT("whisper");
		const FString PollStart = TEXT("poll_start");
		const FString PollVote = TEXT("poll_vote");
		const FString ClearMessages = TEXT("clear_messages");
		const FString Purge = TEXT("purge");
		const FString GiveawayStart = TEXT("giveaway_start");
	}
}