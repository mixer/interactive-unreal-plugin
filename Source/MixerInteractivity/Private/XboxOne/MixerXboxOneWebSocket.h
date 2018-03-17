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

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Containers/Queue.h"
#include "IWebSocket.h"

class FMixerXboxOneWebSocket :
	public IWebSocket,
	public FTickerObjectBase,
	public TSharedFromThis<FMixerXboxOneWebSocket>
{
public:
	FMixerXboxOneWebSocket(const FString& Uri, const TArray<FString>& Protocols, const TMap<FString, FString>& Headers);

	virtual void Connect() override;
	virtual void Close(int32 Code = 1000, const FString& Reason = FString()) override;
	virtual bool IsConnected() override;
	virtual void Send(const FString& Data) override;
	virtual void Send(const void* Utf8Data, SIZE_T Size, bool bIsBinary) override;

	virtual FWebSocketConnectedEvent& OnConnected() override { return ConnectedEvent; }
	virtual FWebSocketConnectionErrorEvent& OnConnectionError() override { return ConnectionErrorEvent; }
	virtual FWebSocketClosedEvent& OnClosed() override { return ClosedEvent; }
	virtual FWebSocketMessageEvent& OnMessage() override { return MessageEvent; }
	virtual FWebSocketRawMessageEvent& OnRawMessage() override { return RawMessageEvent; }

public:
	virtual bool Tick(float DeltaTime) override;

private:
	void FinishClose(int32 Code, const FString& Reason);

	TQueue<TFunction<void()>, EQueueMode::Mpsc> GameThreadWork;

	Windows::Networking::Sockets::MessageWebSocket^ Socket;
	Windows::Storage::Streams::DataWriter^ Writer;

	Windows::Foundation::IAsyncAction^ ConnectAction;
	TArray<Windows::Storage::Streams::DataWriterStoreOperation^> SendOperations;
	FString Uri;

	FWebSocketConnectedEvent ConnectedEvent;
	FWebSocketConnectionErrorEvent ConnectionErrorEvent;
	FWebSocketClosedEvent ClosedEvent;
	FWebSocketMessageEvent MessageEvent;
	FWebSocketRawMessageEvent RawMessageEvent;
	bool bUserClose;
};