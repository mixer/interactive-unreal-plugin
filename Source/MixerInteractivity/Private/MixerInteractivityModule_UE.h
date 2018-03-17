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

#include "MixerInteractivityModule_WithSessionState.h"

#if MIXER_BACKEND_UE

#include "MixerWebSocketOwnerBase.h"

class FMixerInteractivityModule_UE
	: public FMixerInteractivityModule_WithSessionState
	, public TMixerWebSocketOwnerBase<FMixerInteractivityModule_UE>
{
public:
	FMixerInteractivityModule_UE();

public:
	virtual void StartInteractivity();
	virtual void StopInteractivity();
	virtual void SetCurrentScene(FName Scene, FName GroupName = NAME_None);
	virtual FName GetCurrentScene(FName GroupName = NAME_None);
	virtual bool CreateGroup(FName GroupName, FName InitialScene = NAME_None);
	virtual bool MoveParticipantToGroup(FName GroupName, uint32 ParticipantId);
	virtual void CaptureSparkTransaction(const FString& TransactionId);
	virtual void CallRemoteMethod(const FString& MethodName, const TSharedRef<FJsonObject> MethodParams);

protected:
	virtual bool StartInteractiveConnection();
	virtual void StopInteractiveConnection();

protected:
	virtual void RegisterAllServerMessageHandlers();
	virtual bool OnUnhandledServerMessage(const FString& MessageType, const TSharedPtr<FJsonObject> Params);

	virtual void HandleSocketConnected();
	virtual void HandleSocketConnectionError();
	virtual void HandleSocketClosed(bool bWasClean);

private:
	void OnHostsRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	void OpenWebSocket();

	bool CreateOrUpdateGroup(const FString& MethodName, FName Scene, FName GroupName);

	bool HandleHello(FJsonObject* JsonObj);
	bool HandleGiveInput(FJsonObject* JsonObj);
	bool HandleParticipantJoin(FJsonObject* JsonObj);
	bool HandleParticipantLeave(FJsonObject* JsonObj);
	bool HandleParticipantUpdate(FJsonObject* JsonObj);
	bool HandleReadyStateChange(FJsonObject* JsonObj);
	bool HandleGroupCreate(FJsonObject* JsonObj);
	bool HandleGroupUpdate(FJsonObject* JsonObj);
	bool HandleGroupDelete(FJsonObject* JsonObj);

	bool HandleGetScenesReply(FJsonObject* JsonObj);

	bool HandleGiveInput(TSharedPtr<FMixerRemoteUser> Participant, FJsonObject* FullParamsJson, const TSharedRef<FJsonObject> InputObjJson);
	bool HandleParticipantEvent(FJsonObject* JsonObj, EMixerInteractivityParticipantState EventType);
	bool HandleSingleParticipantChange(const FJsonObject* JsonObj, EMixerInteractivityParticipantState EventType);

	bool ParsePropertiesFromGetScenesResult(FJsonObject *JsonObj);
	bool ParsePropertiesFromSingleScene(FJsonObject* JsonObj);
	bool ParsePropertiesFromSingleControl(FName SceneId, TSharedRef<FJsonObject> JsonObj);

private:
	TArray<FString> Endpoints;
	TMap<FName, FName> ScenesByGroup;
};

#endif
