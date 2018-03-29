//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "MixerXboxOneWebSocket.h"

FMixerXboxOneWebSocket::FMixerXboxOneWebSocket(const FString& InUri, const TArray<FString>& InProtocols, const TMap<FString, FString>& InHeaders)
	: Uri(InUri)
	, Socket(ref new Windows::Networking::Sockets::MessageWebSocket())
	, bUserClose(false)
{
	Windows::Networking::Sockets::MessageWebSocketControl^ SocketControl = Socket->Control;
	SocketControl->MessageType = Windows::Networking::Sockets::SocketMessageType::Utf8;
	for (const FString& Protocol : InProtocols)
	{
		SocketControl->SupportedProtocols->Append(ref new Platform::String(*Protocol));
	}

	for (TMap<FString, FString>::TConstIterator It(InHeaders); It; ++It)
	{
		Socket->SetRequestHeader(ref new Platform::String(*It->Key), ref new Platform::String(*It->Value));
	}
}

void FMixerXboxOneWebSocket::Connect()
{
	using namespace Windows::Networking::Sockets;

	TWeakPtr<FMixerXboxOneWebSocket> WeakThis = SharedThis(this);
	Socket->MessageReceived += ref new Windows::Foundation::TypedEventHandler<MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^>(
		[WeakThis](MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^ EventArgs)
		{
			TSharedPtr<FMixerXboxOneWebSocket> PinnedThis = WeakThis.Pin();
			if (PinnedThis.IsValid())
			{
				Windows::Storage::Streams::DataReader^ Reader = EventArgs->GetDataReader();
				FString MessageText = FString(Reader->ReadString(Reader->UnconsumedBufferLength)->Data());
				PinnedThis->GameThreadWork.Enqueue(
					[WeakThis, MessageText]()
				{
					TSharedPtr<FMixerXboxOneWebSocket> PinnedThis = WeakThis.Pin();
					if (PinnedThis.IsValid())
					{
						PinnedThis->OnMessage().Broadcast(MessageText);
					}
				});
			}
		});

	Socket->Closed += ref new Windows::Foundation::TypedEventHandler<Windows::Networking::Sockets::IWebSocket^, WebSocketClosedEventArgs^>(
		[WeakThis](Windows::Networking::Sockets::IWebSocket^, WebSocketClosedEventArgs^ EventArgs)
		{
			TSharedPtr<FMixerXboxOneWebSocket> PinnedThis = WeakThis.Pin();
			if (PinnedThis.IsValid())
			{
				int32 Code = EventArgs->Code;
				FString Reason = EventArgs->Reason->Data();
				PinnedThis->GameThreadWork.Enqueue(
					[WeakThis, Code, Reason]()
				{
					TSharedPtr<FMixerXboxOneWebSocket> PinnedThis = WeakThis.Pin();
					if (PinnedThis.IsValid())
					{
						PinnedThis->FinishClose(Code, Reason);
					}
				});
			}
		});

	try
	{
		ConnectAction = Socket->ConnectAsync(ref new Windows::Foundation::Uri(ref new Platform::String(*Uri)));
	}
	catch (Platform::Exception^ Ex)
	{
		FString ErrorMessage = Ex->Message->Data();
		GameThreadWork.Enqueue(
			[this, ErrorMessage]()
		{
			OnConnectionError().Broadcast(ErrorMessage);
		});
	}
}

void FMixerXboxOneWebSocket::Close(int32 Code, const FString& Reason)
{
	bUserClose = true;
	try
	{
		Socket->Close(static_cast<uint16>(Code), ref new Platform::String(*Reason));
	}
	catch (...)
	{

	}
}

bool FMixerXboxOneWebSocket::IsConnected()
{
	return Writer != nullptr;
}

void FMixerXboxOneWebSocket::Send(const FString& Data)
{
	if (Writer != nullptr)
	{
		try
		{
			Writer->WriteString(ref new Platform::String(*Data));
			SendOperations.Add(Writer->StoreAsync());
		}
		catch (...)
		{
		}
	}
}

void FMixerXboxOneWebSocket::Send(const void* Utf8Data, SIZE_T Size, bool bIsBinary)
{

}

bool FMixerXboxOneWebSocket::Tick(float DeltaTime)
{
	if (ConnectAction != nullptr)
	{
		switch (ConnectAction->Status)
		{
		case Windows::Foundation::AsyncStatus::Started:
			break;

		case Windows::Foundation::AsyncStatus::Canceled:
			ConnectAction = nullptr;
			break;

		case Windows::Foundation::AsyncStatus::Error:
			OnConnectionError().Broadcast(FString::FromInt(ConnectAction->ErrorCode.Value));
			ConnectAction = nullptr;
			break;

		case Windows::Foundation::AsyncStatus::Completed:
			Writer = ref new Windows::Storage::Streams::DataWriter(Socket->OutputStream);
			Writer->UnicodeEncoding = Windows::Storage::Streams::UnicodeEncoding::Utf8;
			OnConnected().Broadcast();
			ConnectAction = nullptr;
			break;

		default:
			break;
		}
	}

	for (int32 i = 0; i < SendOperations.Num(); ++i)
	{
		if (SendOperations[i]->Status != Windows::Foundation::AsyncStatus::Started)
		{
			SendOperations.RemoveAtSwap(i);
		}
	}

	TFunction<void()> WorkItem;
	while (GameThreadWork.Dequeue(WorkItem))
	{
		WorkItem();
	}

	return true;
}

void FMixerXboxOneWebSocket::FinishClose(int32 Code, const FString& Reason)
{
	try
	{
		Writer->DetachStream();
	}
	catch (...)
	{

	}
	Writer = nullptr;

	OnClosed().Broadcast(Code, Reason, bUserClose);
}