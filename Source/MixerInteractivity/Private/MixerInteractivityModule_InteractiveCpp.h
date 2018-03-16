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

#if MIXER_BACKEND_INTERACTIVE_CPP

namespace Microsoft
{
	namespace mixer
	{
		class interactive_button_control;
		class interactive_joystick_control;
		class interactive_participant;
		enum interactivity_state : int;
	}
}

struct FMixerRemoteUserCached : public FMixerRemoteUser
{
public:
	FMixerRemoteUserCached(std::shared_ptr<Microsoft::mixer::interactive_participant> InParticipant);

	void UpdateFromSourceParticipant();

	std::shared_ptr<Microsoft::mixer::interactive_participant> GetSourceParticipant() { return SourceParticipant; }
private:
	std::shared_ptr<Microsoft::mixer::interactive_participant> SourceParticipant;
};


class FMixerInteractivityModule_InteractiveCpp : public FMixerInteractivityModule
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
	virtual void SetLabelText(FName Label, const FText& DisplayText);
	virtual bool GetLabelDescription(FName Label, FMixerLabelDescription& OutDesc);
	virtual bool GetTextboxDescription(FName Textbox, FMixerTextboxDescription& OutDesc);
	virtual TSharedPtr<const FMixerRemoteUser> GetParticipant(uint32 ParticipantId);
	virtual bool CreateGroup(FName GroupName, FName InitialScene = NAME_None);
	virtual bool GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants);
	virtual bool MoveParticipantToGroup(FName GroupName, uint32 ParticipantId);
	virtual void CaptureSparkTransaction(const FString& TransactionId);
	virtual void CallRemoteMethod(const FString& MethodName, const TSharedRef<FJsonObject> MethodParams);

public:
	virtual bool Tick(float DeltaTime) override;

protected:
	virtual bool StartInteractiveConnection();
	virtual void StopInteractiveConnection();

private:
	std::shared_ptr<Microsoft::mixer::interactive_button_control> FindButton(FName Name);
	std::shared_ptr<Microsoft::mixer::interactive_joystick_control> FindStick(FName Name);
	TSharedPtr<FMixerRemoteUserCached> CreateOrUpdateCachedParticipant(std::shared_ptr<Microsoft::mixer::interactive_participant> Participant);

	void TickParticipantCacheMaintenance();

private:
	TMap<uint32, TSharedPtr<FMixerRemoteUserCached>> RemoteParticipantCache;
};

#endif // MIXER_BACKEND_INTERACTIVE_CPP