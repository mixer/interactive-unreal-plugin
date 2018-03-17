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

		const FString Ready = TEXT("ready");
		const FString UpdateGroups = TEXT("updateGroups");
		const FString CreateGroups = TEXT("createGroups");
		const FString UpdateParticipants = TEXT("updateParticipants");
		const FString Capture = TEXT("capture");
		const FString GetScenes = TEXT("getScenes");
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

		const FString MouseDown = TEXT("mousedown");
		const FString MouseUp = TEXT("mouseup");
		const FString Move = TEXT("move");
		const FString Submit = TEXT("submit");
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
		const FString Params = TEXT("params");
		const FString Error = TEXT("error");
		const FString Text = TEXT("text");
		const FString Endpoints = TEXT("endpoints");
		const FString AuthKey = TEXT("authkey");
		const FString UserIdNoUnderscore = TEXT("userID");
		const FString UserIdWithUnderscore = TEXT("user_id");
		const FString UserLevel = TEXT("user_level");
		const FString Q = TEXT("q");
		const FString EndsAt = TEXT("endsAt");
		const FString Voters = TEXT("voters");
		const FString Answers = TEXT("answers");
		const FString ResponsesByIndex = TEXT("responsesByIndex");
		const FString Author = TEXT("author");
		const FString Permissions = TEXT("permissions");
		const FString Level = TEXT("level");
		const FString LastInputAt = TEXT("lastInputAt");
		const FString ConnectedAt = TEXT("connectedAt");
		const FString GroupId = TEXT("groupID");
		const FString SessionId = TEXT("sessionID");
		const FString Participants = TEXT("participants");
		const FString IsReady = TEXT("isReady");
		const FString ParticipantId = TEXT("participantID");
		const FString Input = TEXT("input");
		const FString TransactionId = TEXT("transactionID");
		const FString ControlId = TEXT("controlID");
		const FString X = TEXT("x");
		const FString Y = TEXT("y");
		const FString SceneId = TEXT("sceneID");
		const FString Scenes = TEXT("scenes");
		const FString Controls = TEXT("controls");
		const FString Kind = TEXT("kind");
		const FString Cost = TEXT("cost");
		const FString Cooldown = TEXT("cooldown");
		const FString Disabled = TEXT("disabled");
		const FString Tooltip = TEXT("tooltip");
		const FString Progress = TEXT("progress");
		const FString Result = TEXT("result");
		const FString Value = TEXT("value");
		const FString TextSize = TEXT("textSize");
		const FString TextColor = TEXT("textColor");
		const FString Underline = TEXT("underline");
		const FString Bold = TEXT("bold");
		const FString Italic = TEXT("italic");
		const FString Placeholder = TEXT("placeholder");
		const FString HasSubmit = TEXT("hasSubmit");
		const FString Multiline = TEXT("multiline");
		const FString SubmitText = TEXT("submitText");
		const FString Groups = TEXT("groups");
		const FString ReassignGroupId = TEXT("reassignGroupId");
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