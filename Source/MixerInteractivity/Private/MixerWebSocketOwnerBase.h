#pragma once

#include "Dom/JsonObject.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "MixerInteractivityLog.h"
#include "MixerJsonHelpers.h"
#include "Policies/JsonPrintPolicy.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializerMacros.h"

#if PLATFORM_XBOXONE
#include "XboxOne/MixerXboxOneWebSocket.h"
#endif

template <class T>
class TMixerWebSocketOwnerBase
{
protected:
	TMixerWebSocketOwnerBase(const FString& InServerInitiatedMessageType, const FString& InServerInitiatedMessageSubtypeName, const FString& InServerInitiatedMessageParamsName);
	virtual ~TMixerWebSocketOwnerBase();

	void InitConnection(const FString& Url, const TMap<FString,FString>& UpgradeHeaders);
	void CleanupConnection();

	typedef bool (T::*FServerMessageHandler)(FJsonObject*);

	void RegisterServerMessageHandler(const FString& MessageType, FServerMessageHandler Handler);
	virtual bool OnUnhandledServerMessage(const FString& MessageType, const TSharedPtr<FJsonObject> Params) = 0;

	void SendMethodMessageNoParams(const FString& MethodName, FServerMessageHandler Handler);
	void SendMethodMessageObjectParams(const FString& MethodName, FServerMessageHandler Handler, const FJsonSerializable& ObjectStyleParams);
	void SendMethodMessageObjectParams(const FString& MethodName, FServerMessageHandler Handler, const TSharedRef<FJsonObject> ObjectStyleParams);

	template <class ... ArgTypes>
	void SendMethodMessageArrayParams(const FString& MethodName, FServerMessageHandler Handler, ArgTypes... ArrayStyleParams);

	virtual void HandleSocketConnected() = 0;
	virtual void HandleSocketConnectionError() = 0;
	virtual void HandleSocketClosed(bool bWasClean) = 0;

	virtual void RegisterAllServerMessageHandlers() = 0;

private:
	void OnSocketConnected();
	void OnSocketConnectionError(const FString& ErrorMessage);
	void OnSocketMessage(const FString& MessageJsonString);
	void OnSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);

	bool OnSocketMessage(FJsonObject* JsonObj);

	typedef TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> CondensedWriterType;

	TSharedRef<CondensedWriterType> StartMethodMessage(const FString& MethodName, FString& PayloadString);
	void FinishMethodMessage(TSharedRef<CondensedWriterType> Writer);
	void ActuallySendMethodMessage(FServerMessageHandler Handler, const FString& PayloadString);

private:
	template <class PARAM>
	void WriteSingleRemoteMethodParam(CondensedWriterType& Writer, PARAM Param1);

	template <class PARAM>
	void WriteSingleRemoteMethodParam(CondensedWriterType& Writer, const TArray<PARAM>& Param1);

	template <class PARAM, class ...ArgTypes>
	void WriteRemoteMethodParams(CondensedWriterType& Writer, PARAM Param1, ArgTypes... AdditionalArgs);

	template <class PARAM>
	void WriteRemoteMethodParams(CondensedWriterType& Writer, PARAM Param1);

private:
	TSharedPtr<IWebSocket> WebSocket;
	FString ServerInitiatedMessageType;
	FString ServerInitiatedMessageSubtypeName;
	FString ServerInitiatedMessageParamsName;
	TMap<int32, FServerMessageHandler> ReplyHandlers;
	TMap<FString, FServerMessageHandler> ServerInitiatedMessageHandlers;
	int32 MessageId;
	int32 SequenceId;
};

template <class T>
TMixerWebSocketOwnerBase<T>::TMixerWebSocketOwnerBase(const FString& InServerInitiatedMessageType, const FString& InServerInitiatedMessageSubtypeName, const FString& InServerInitiatedMessageParamsName)
	: ServerInitiatedMessageType(InServerInitiatedMessageType)
	, ServerInitiatedMessageSubtypeName(InServerInitiatedMessageSubtypeName)
	, ServerInitiatedMessageParamsName(InServerInitiatedMessageParamsName)
	, MessageId(0)
	, SequenceId(0)
{

}

template <class T>
TMixerWebSocketOwnerBase<T>::~TMixerWebSocketOwnerBase()
{
	CleanupConnection();
}

template <class T>
void TMixerWebSocketOwnerBase<T>::InitConnection(const FString& Url, const TMap<FString, FString>& UpgradeHeaders)
{
	ServerInitiatedMessageHandlers.Empty();
	RegisterAllServerMessageHandlers();

	// Explicitly list protocols for the benefit of Xbox
	TArray<FString> Protocols;
	Protocols.Add(TEXT("wss"));
	Protocols.Add(TEXT("ws"));
#if PLATFORM_XBOXONE
	WebSocket = MakeShared<FMixerXboxOneWebSocket>(Url, Protocols, UpgradeHeaders);
#else
	WebSocket = FModuleManager::LoadModuleChecked<FWebSocketsModule>("WebSockets").CreateWebSocket(Url, Protocols, UpgradeHeaders);
#endif

	if (WebSocket.IsValid())
	{
		WebSocket->OnConnected().AddRaw(this, &TMixerWebSocketOwnerBase::OnSocketConnected);
		WebSocket->OnConnectionError().AddRaw(this, &TMixerWebSocketOwnerBase::OnSocketConnectionError);
		WebSocket->OnMessage().AddRaw(this, &TMixerWebSocketOwnerBase::OnSocketMessage);
		WebSocket->OnClosed().AddRaw(this, &TMixerWebSocketOwnerBase::OnSocketClosed);

		WebSocket->Connect();
	}
}

template <class T>
void TMixerWebSocketOwnerBase<T>::CleanupConnection()
{
	if (WebSocket.IsValid())
	{
		WebSocket->OnConnected().RemoveAll(this);
		WebSocket->OnConnectionError().RemoveAll(this);
		WebSocket->OnMessage().RemoveAll(this);
		WebSocket->OnClosed().RemoveAll(this);

		if (WebSocket->IsConnected())
		{
			WebSocket->Close();
		}

		WebSocket.Reset();
	}
}

template <class T>
void TMixerWebSocketOwnerBase<T>::RegisterServerMessageHandler(const FString& MessageType, FServerMessageHandler Handler)
{
	ServerInitiatedMessageHandlers.Add(MessageType, Handler);
}

template <class T>
TSharedRef<typename TMixerWebSocketOwnerBase<T>::CondensedWriterType> TMixerWebSocketOwnerBase<T>::StartMethodMessage(const FString& MethodName, FString& PayloadString)
{
	TSharedRef<CondensedWriterType> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&PayloadString);
	Writer->WriteObjectStart();
	Writer->WriteValue(MixerStringConstants::FieldNames::Type, MixerStringConstants::MessageTypes::Method);
	Writer->WriteValue(MixerStringConstants::FieldNames::Method, MethodName);
	Writer->WriteValue(MixerStringConstants::FieldNames::Id, MessageId);
	return Writer;
}

template <class T>
void TMixerWebSocketOwnerBase<T>::FinishMethodMessage(TSharedRef<CondensedWriterType> Writer)
{
	Writer->WriteObjectEnd();
	Writer->Close();
}

template <class T>
void TMixerWebSocketOwnerBase<T>::ActuallySendMethodMessage(FServerMessageHandler Handler, const FString& PayloadString)
{
	check(!ReplyHandlers.Contains(MessageId));
	ReplyHandlers.Add(MessageId, Handler);
	++MessageId;

	WebSocket->Send(PayloadString);
}

template <class T>
void TMixerWebSocketOwnerBase<T>::SendMethodMessageNoParams(const FString& MethodName, FServerMessageHandler Handler)
{
	FString PayloadString;
	TSharedRef<CondensedWriterType> Writer = StartMethodMessage(MethodName, PayloadString);
	FinishMethodMessage(Writer);
	ActuallySendMethodMessage(Handler, PayloadString);
}

template <class T>
void TMixerWebSocketOwnerBase<T>::SendMethodMessageObjectParams(const FString& MethodName, FServerMessageHandler Handler, const FJsonSerializable& ObjectStyleParams)
{
	FString PayloadString;
	TSharedRef<CondensedWriterType> Writer = StartMethodMessage(MethodName, PayloadString);
	Writer->WriteIdentifierPrefix(MixerStringConstants::FieldNames::Params);
	ObjectStyleParams.ToJson(Writer, false);
	FinishMethodMessage(Writer);
	ActuallySendMethodMessage(Handler, PayloadString);
}

template <class T>
void TMixerWebSocketOwnerBase<T>::SendMethodMessageObjectParams(const FString& MethodName, FServerMessageHandler Handler, const TSharedRef<FJsonObject> ObjectStyleParams)
{
	FString PayloadString;
	TSharedRef<CondensedWriterType> Writer = StartMethodMessage(MethodName, PayloadString);
	Writer->WriteIdentifierPrefix(MixerStringConstants::FieldNames::Params);
	FJsonSerializer::Serialize(ObjectStyleParams, Writer, false);
	FinishMethodMessage(Writer);
	ActuallySendMethodMessage(Handler, PayloadString);
}

template <class T>
template <class ... ArgTypes>
void TMixerWebSocketOwnerBase<T>::SendMethodMessageArrayParams(const FString& MethodName, typename TMixerWebSocketOwnerBase<T>::FServerMessageHandler Handler, ArgTypes... ArrayStyleParams)
{
	FString PayloadString;
	TSharedRef<CondensedWriterType> Writer = StartMethodMessage(MethodName, PayloadString);
	Writer->WriteArrayStart(MixerStringConstants::FieldNames::Arguments);
	WriteRemoteMethodParams(Writer.Get(), ArrayStyleParams...);
	Writer->WriteArrayEnd();
	FinishMethodMessage(Writer);
	ActuallySendMethodMessage(Handler, PayloadString);
}

template <class T>
void TMixerWebSocketOwnerBase<T>::OnSocketConnected()
{
	HandleSocketConnected();
}

template <class T>
void TMixerWebSocketOwnerBase<T>::OnSocketConnectionError(const FString& ErrorMessage)
{
	UE_LOG(LogMixerInteractivity, Warning, TEXT("Failed to connect web socket with error %s"), *ErrorMessage);
	CleanupConnection();
	HandleSocketConnectionError();
}

template <class T>
void TMixerWebSocketOwnerBase<T>::OnSocketMessage(const FString& MessageJsonString)
{
	UE_LOG(LogMixerInteractivity, Verbose, TEXT("WebSocket message %s"), *MessageJsonString);

	bool bHandled = false;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(MessageJsonString);
	TSharedPtr<FJsonObject> JsonObj;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObj) && JsonObj.IsValid())
	{
		bHandled = OnSocketMessage(JsonObj.Get());
	}

	if (!bHandled)
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("Failed to handle websocket message from server: %s"), *MessageJsonString);
	}
}

template <class T>
void TMixerWebSocketOwnerBase<T>::OnSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG(LogMixerInteractivity, Warning, TEXT("WebSocket closed with reason '%s'."), *Reason);

	CleanupConnection();

	HandleSocketClosed(bWasClean);
}

template <class T>
bool TMixerWebSocketOwnerBase<T>::OnSocketMessage(FJsonObject* JsonObj)
{
	bool bHandled = false;
	GET_JSON_STRING_RETURN_FAILURE(Type, MessageType);
	if (MessageType == MixerStringConstants::MessageTypes::Reply)
	{
		GET_JSON_INT_RETURN_FAILURE(Id, ReplyingToMessageId);

		FServerMessageHandler Handler;
		if (ReplyHandlers.RemoveAndCopyValue(ReplyingToMessageId, Handler))
		{
			if (Handler != nullptr)
			{
				(static_cast<T*>(this)->*Handler)(JsonObj);
			}
			bHandled = true;
		}
		else
		{
			UE_LOG(LogMixerInteractivity, Error, TEXT("Received unexpected reply for unknown message id %d"), ReplyingToMessageId);
		}
	}
	else if (MessageType == ServerInitiatedMessageType)
	{
		FString Subtype;
		if (!JsonObj->TryGetStringField(ServerInitiatedMessageSubtypeName, Subtype))
		{
			UE_LOG(LogMixerInteractivity, Error, TEXT("Missing message subtype (%s)"), *ServerInitiatedMessageSubtypeName);
			return false;
		}

		TSharedPtr<FJsonValue> RawParams = JsonObj->TryGetField(ServerInitiatedMessageParamsName);
		if (!RawParams.IsValid())
		{
			UE_LOG(LogMixerInteractivity, Error, TEXT("Missing message params (%s)"), *ServerInitiatedMessageParamsName);
			return false;
		}

		const TSharedPtr<FJsonObject>* Params = nullptr;
		if (!RawParams->IsNull())
		{
			if (!RawParams->TryGetObject(Params))
			{
				UE_LOG(LogMixerInteractivity, Error, TEXT("Unexpected value %s for message params (%s)"), *RawParams->AsString(), *ServerInitiatedMessageParamsName);
				return false;
			}
		}

		FServerMessageHandler* Handler = ServerInitiatedMessageHandlers.Find(Subtype);
		if (Handler != nullptr)
		{
			if (*Handler != nullptr)
			{
				(static_cast<T*>(this)->**Handler)(Params != nullptr ? Params->Get() : nullptr);
			}
			bHandled = true;
		}
		else
		{
			bHandled = OnUnhandledServerMessage(Subtype, Params != nullptr ? *Params : nullptr);
		}
	}

	return bHandled;
}

template <class T>
template <class PARAM>
void TMixerWebSocketOwnerBase<T>::WriteSingleRemoteMethodParam(CondensedWriterType& Writer, PARAM Param1)
{
	Writer.WriteValue(Param1);
}

template <class T>
template <class PARAM>
void TMixerWebSocketOwnerBase<T>::WriteSingleRemoteMethodParam(CondensedWriterType& Writer, const TArray<PARAM>& Param1)
{
	Writer.WriteArrayStart();
	for (const PARAM& Val : Param1)
	{
		Writer.WriteValue(Val);
	}
	Writer.WriteArrayEnd();
}

template <class T>
template <class PARAM, class ...ArgTypes>
void TMixerWebSocketOwnerBase<T>::WriteRemoteMethodParams(CondensedWriterType& Writer, PARAM Param1, ArgTypes... AdditionalArgs)
{
	WriteSingleRemoteMethodParam(Writer, Param1);
	WriteRemoteMethodParams(Writer, AdditionalArgs...);
}

template <class T>
template <class PARAM>
void TMixerWebSocketOwnerBase<T>::WriteRemoteMethodParams(CondensedWriterType& Writer, PARAM Param1)
{
	WriteSingleRemoteMethodParam(Writer, Param1);
}