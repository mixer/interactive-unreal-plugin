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

#if MIXER_BACKEND_NULL

class FMixerInteractivityModule_Null : public FMixerInteractivityModule
{
public:
	virtual void StartInteractivity() {}
	virtual void StopInteractivity() {}
	virtual void SetCurrentScene(FName Scene, FName GroupName = NAME_None) {}
	virtual FName GetCurrentScene(FName GroupName = NAME_None) { return NAME_None; }
	virtual void TriggerButtonCooldown(FName Button, FTimespan CooldownTime) {}
	virtual bool GetButtonDescription(FName Button, FMixerButtonDescription& OutDesc) { return false; }
	virtual bool GetButtonState(FName Button, FMixerButtonState& OutState) { return false; }
	virtual bool GetButtonState(FName Button, uint32 ParticipantId, FMixerButtonState& OutState) { return false; }
	virtual bool GetStickDescription(FName Stick, FMixerStickDescription& OutDesc) { return false; }
	virtual bool GetStickState(FName Stick, FMixerStickState& OutState) { return false; }
	virtual bool GetStickState(FName Stick, uint32 ParticipantId, FMixerStickState& OutState) { return false; }
	virtual void SetLabelText(FName Label, const FText& DisplayText) {}
	virtual bool GetLabelDescription(FName Label, FMixerLabelDescription& OutDesc) { return false; }
	virtual bool GetTextboxDescription(FName Textbox, FMixerTextboxDescription& OutDesc) { return false; }
	virtual TSharedPtr<const FMixerRemoteUser> GetParticipant(uint32 ParticipantId) { return nullptr; }
	virtual bool CreateGroup(FName GroupName, FName InitialScene = NAME_None) { return false; }
	virtual bool GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants) { return false; }
	virtual bool MoveParticipantToGroup(FName GroupName, uint32 ParticipantId) { return false; }
	virtual void CaptureSparkTransaction(const FString& TransactionId) {}
	virtual void CallRemoteMethod(const FString& MethodName, const TSharedRef<FJsonObject> MethodParams) {}

protected:
	virtual bool StartInteractiveConnection() { return false; }
	virtual void StopInteractiveConnection() {}
};

#endif // MIXER_BACKEND_NULL
