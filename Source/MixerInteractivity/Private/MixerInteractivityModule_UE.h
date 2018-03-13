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

#include "MixerInteractivityModulePrivate.h"


#define MIXER_BACKEND_UE 1
#if MIXER_BACKEND_UE

#include "MixerWebSocketOwnerBase.h"


class FMixerInteractivityModule_UE
	: public FMixerInteractivityModule
	, public TMixerWebSocketOwnerBase<FMixerInteractivityModule_UE>
{
public:
	FMixerInteractivityModule_UE();

public:
	virtual void StartInteractivity();
	virtual void StopInteractivity();
	virtual void SetCurrentScene(FName Scene, FName GroupName = NAME_None);
	virtual FName GetCurrentScene(FName GroupName = NAME_None) { return NAME_None; }
	virtual void TriggerButtonCooldown(FName Button, FTimespan CooldownTime) {}
	virtual bool GetButtonDescription(FName Button, FMixerButtonDescription& OutDesc) { return false; }
	virtual bool GetButtonState(FName Button, FMixerButtonState& OutState) { return false; }
	virtual bool GetButtonState(FName Button, uint32 ParticipantId, FMixerButtonState& OutState) { return false; }
	virtual bool GetStickDescription(FName Stick, FMixerStickDescription& OutDesc) { return false; }
	virtual bool GetStickState(FName Stick, FMixerStickState& OutState) { return false; }
	virtual bool GetStickState(FName Stick, uint32 ParticipantId, FMixerStickState& OutState) { return false; }
	virtual TSharedPtr<const FMixerRemoteUser> GetParticipant(uint32 ParticipantId) { return nullptr; }
	virtual bool CreateGroup(FName GroupName, FName InitialScene = NAME_None) { return false; }
	virtual bool GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants) { return false; }
	virtual bool MoveParticipantToGroup(FName GroupName, uint32 ParticipantId) { return false; }
	virtual void CaptureSparkTransaction(const FString& TransactionId) {}

protected:
	virtual bool StartInteractiveConnection();

protected:
	virtual void RegisterAllServerMessageHandlers();

	virtual void HandleSocketConnected();
	virtual void HandleSocketConnectionError();
	virtual void HandleSocketClosed(bool bWasClean);

private:
	void OnHostsRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	void OpenWebSocket();

	bool HandleHello(FJsonObject* JsonObj);
	bool HandleGiveInput(FJsonObject* JsonObj);
	bool HandleParticipantJoin(FJsonObject* JsonObj);
	bool HandleParticipantLeave(FJsonObject* JsonObj);
	bool HandleParticipantUpdate(FJsonObject* JsonObj);
	bool HandleReadyStateChange(FJsonObject* JsonObj);

	bool HandleGetScenesReply(FJsonObject* JsonObj);

	bool HandleGiveInput(TSharedPtr<FMixerRemoteUser> Participant, FJsonObject* FullParamsJson, FJsonObject* InputObjJson);
	bool HandleParticipantEvent(FJsonObject* JsonObj, EMixerInteractivityParticipantState EventType);
	bool HandleSingleParticipantChange(const FJsonObject* JsonObj, EMixerInteractivityParticipantState EventType);

private:
	TArray<FString> Endpoints;
	TMap<FGuid, TSharedPtr<FMixerRemoteUser>> RemoteParticipantCacheByGuid;
	TMap<uint32, TSharedPtr<FMixerRemoteUser>> RemoteParticipantCacheByUint;
};

#endif
