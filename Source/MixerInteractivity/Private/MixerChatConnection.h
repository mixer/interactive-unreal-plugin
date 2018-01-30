#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineChatInterface.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMixerChat, Log, All);

class FMixerChatConnection : public TSharedFromThis<FMixerChatConnection>
{
public:
	FMixerChatConnection(const FUniqueNetId& UserId, const FChatRoomId& InRoomId)
		: User(UserId.AsShared())
		, RoomId(InRoomId)
		, MessageId(0)
	{
	}

	bool Init();
	void Cleanup();

	bool SendChatMessage(const FString& MessageBody);
	bool SendWhisper(const FString& ToUser, const FString& MessageBody);

	const FChatRoomId& GetRoom() const		{ return RoomId; }
	bool IsAnonymous() const				{ return AuthKey.IsEmpty(); }

private:

	void OpenWebSocket();
	void CloseWebSocket();

	void JoinDiscoveredChatChannel();

	void OnGetChannelInfoForRoomIdComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void OnDiscoverChatServersComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	void OnChatSocketConnected();
	void OnChatConnectionError(const FString& ErrorMessage);
	void OnChatPacket(const FString& PacketJsonString);
	void OnChatSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);

	void HandleChatMessagePacket(class FJsonObject* Payload);
	void HandleChatMessageFragment(FString& MessageSoFar, class FJsonObject* Fragment);
private:
	TSharedRef<const FUniqueNetId> User;
	FChatRoomId RoomId;
	FString AuthKey;
	TArray<FString> Endpoints;
	TSharedPtr<class IWebSocket> WebSocket;
	int32 ChannelId;
	int32 MessageId;
	bool bIsReady;
};
