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

#define MIXER_BACKEND_INTERACTIVE_CPP_2 1
#if MIXER_BACKEND_INTERACTIVE_CPP_2

#include "MixerInteractivityTypes.h"
#include <interactive-cpp-2/Mixer.h>

struct FMixerRemoteUserCached : public FMixerRemoteUser
{
public:
	FGuid SessionGuid;
};

class FMixerInteractivityModule_InteractiveCpp2 : public FMixerInteractivityModule
{
public:
	virtual void StartInteractivity();
	virtual void StopInteractivity();
	virtual void SetCurrentScene(FName Scene, FName GroupName = NAME_None);
	virtual FName GetCurrentScene(FName GroupName = NAME_None);
	virtual void TriggerButtonCooldown(FName Button, FTimespan CooldownTime);
	virtual bool GetButtonDescription(FName Button, FMixerButtonDescription& OutDesc);
	virtual bool GetButtonState(FName Button, FMixerButtonState& OutState);
	virtual bool GetButtonState(FName Button, uint32 ParticipantId, FMixerButtonState& OutState);
	virtual bool GetStickDescription(FName Stick, FMixerStickDescription& OutDesc);
	virtual bool GetStickState(FName Stick, FMixerStickState& OutState);
	virtual bool GetStickState(FName Stick, uint32 ParticipantId, FMixerStickState& OutState);
	virtual TSharedPtr<const FMixerRemoteUser> GetParticipant(uint32 ParticipantId);
	virtual bool CreateGroup(FName GroupName, FName InitialScene = NAME_None);
	virtual bool GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants);
	virtual bool MoveParticipantToGroup(FName GroupName, uint32 ParticipantId);
	virtual void CaptureSparkTransaction(const FString& TransactionId);

public:
	virtual bool Tick(float DeltaTime) override;

protected:
	virtual bool StartInteractiveConnection();

private:

	static void OnSessionStateChanged(void* Context, mixer::interactive_session Session, mixer::interactive_state PreviousState, mixer::interactive_state NewState);
	static void OnSessionError(void* Context, mixer::interactive_session Session, int ErrorCode, const char* ErrorMessage, size_t ErrorMessageLength);
	static void OnSessionButtonInput(void* Context, mixer::interactive_session Session, const mixer::interactive_button_input* Input);
	static void OnSessionCoordinateInput(void* Context, mixer::interactive_session Session, const mixer::interactive_coordinate_input* Input);
	static void OnSessionParticipantsChanged(void* Context, mixer::interactive_session Session, mixer::participant_action Action, const mixer::interactive_participant* Participant);

	struct FGetCurrentSceneEnumContext
	{
		FName GroupName;
		FName OutSceneName;
	};

	static void OnEnumerateForGetCurrentScene(void* Context, mixer::interactive_session Session, mixer::interactive_group* Group);

	struct FGetParticipantsInGroupEnumContext
	{
		FMixerInteractivityModule_InteractiveCpp2* MixerModule;
		FName GroupName;
		TArray<TSharedPtr<const FMixerRemoteUser>>* OutParticipants;
	};

	static void OnEnumerateForGetParticipantsInGroup(void* Context, mixer::interactive_session Session, mixer::interactive_participant* Participant);

	mixer::interactive_session InteractiveSession;
	TMap<FGuid, TSharedPtr<FMixerRemoteUserCached>> RemoteParticipantCacheByGuid;
	TMap<uint32, TSharedPtr<FMixerRemoteUserCached>> RemoteParticipantCacheByUint;
};

#endif