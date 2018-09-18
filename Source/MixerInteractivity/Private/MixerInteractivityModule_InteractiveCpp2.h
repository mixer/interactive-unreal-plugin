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

#if MIXER_BACKEND_INTERACTIVE_CPP_2

#include "MixerInteractivityTypes.h"
#include <interactive-cpp-v2/interactivity.h>

class FMixerInteractivityModule_InteractiveCpp2
	: public FMixerInteractivityModule_WithSessionState
{
public:
	virtual void StartInteractivity();
	virtual void StopInteractivity();
	virtual void SetCurrentScene(FName Scene, FName GroupName = NAME_None);
	virtual FName GetCurrentScene(FName GroupName = NAME_None);
	virtual void TriggerButtonCooldown(FName Button, FTimespan CooldownTime) override;
	virtual bool CreateGroup(FName GroupName, FName InitialScene = NAME_None);
	virtual bool MoveParticipantToGroup(FName GroupName, uint32 ParticipantId);
	virtual void CaptureSparkTransaction(const FString& TransactionId);
	virtual void CallRemoteMethod(const FString& MethodName, const TSharedRef<FJsonObject> MethodParams);

public:
	virtual bool Tick(float DeltaTime) override;

protected:
	virtual bool StartInteractiveConnection();
	virtual void StopInteractiveConnection();

private:

	static void OnSessionStateChanged(void* Context, interactive_session Session, interactive_state PreviousState, interactive_state NewState);
	static void OnSessionError(void* Context, interactive_session Session, int ErrorCode, const char* ErrorMessage, size_t ErrorMessageLength);
	static void OnSessionInput(void* Context, interactive_session Session, const interactive_input* Input);
	static void OnSessionParticipantsChanged(void* Context, interactive_session Session, interactive_participant_action Action, const interactive_participant* Participant);
	static void OnUnhandledMethod(void* Context, interactive_session Session, const char* MethodJson, size_t MethodJsonLength);
	static void OnTransactionComplete(void *Context, interactive_session Session, const char* TransactionId, size_t TransactionIdLength, unsigned int ErrorCode, const char* ErrorMessage, size_t ErrorMessageLength);

	void OnSessionButtonInput(TSharedPtr<const FMixerRemoteUser> User, const interactive_input* Input);
	void OnSessionCoordinateInput(TSharedPtr<const FMixerRemoteUser> User, const interactive_input* Input);
	bool OnSessionCustomInput(TSharedPtr<const FMixerRemoteUser> User, const interactive_input* Input);

	struct FGetCurrentSceneEnumContext
	{
		FName GroupName;
		FName OutSceneName;
	};

	static void OnEnumerateForGetCurrentScene(void* Context, interactive_session Session, interactive_group* Group);

	static void OnEnumerateScenesForInit(void* Context, interactive_session Session, interactive_scene* Scene);
	static void OnEnumerateControlsForInit(void* Context, interactive_session Session, interactive_control* Control);

	interactive_session InteractiveSession;
	TFuture<interactive_session> ConnectOperation;
};

#endif