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

struct FMixerButtonPropertiesCached
{
	FMixerButtonDescription Desc;
	FMixerButtonState State;
	TSet<uint32> HoldingParticipants;
	FName SceneId;
};

struct FMixerStickPropertiesCached
{
	FMixerStickDescription Desc;
	FMixerStickState State;
	TMap<uint32, FVector2D> PerParticipantStickValue;
};

struct FMixerLabelPropertiesCached
{
	FMixerLabelDescription Desc;
	FName SceneId;
};

struct FMixerTextboxPropertiesCached
{
	FMixerTextboxDescription Desc;
};

class FMixerInteractivityModule_WithSessionState : public FMixerInteractivityModule
{
public:
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
	virtual bool GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants);

public:
	virtual bool Tick(float DeltaTime) override;

protected:
	virtual bool HandleSingleControlUpdate(FName ControlId, const TSharedRef<FJsonObject> ControlData) override;

protected:
	void StartSession(bool bCachePerParticipantState);
	void EndSession();

	bool CachePerParticipantState();

	void AddButton(FName ControlId, const FMixerButtonPropertiesCached& Props);
	FMixerButtonPropertiesCached* GetButton(FName ControlId);

	void AddStick(FName ControlId, const FMixerStickPropertiesCached& Props);
	FMixerStickPropertiesCached* GetStick(FName ControlId);

	void AddLabel(FName ControlId, const FMixerLabelPropertiesCached& Props);
	FMixerLabelPropertiesCached* GetLabel(FName ControlId);

	void AddTextbox(FName ControlId, const FMixerTextboxPropertiesCached& Props);
	FMixerTextboxPropertiesCached* GetTextbox(FName ControlId);

	void AddUser(TSharedPtr<FMixerRemoteUser> User);
	void RemoveUser(TSharedPtr<FMixerRemoteUser> User);
	void RemoveUser(FGuid ParticipantSessionId);
	TSharedPtr<FMixerRemoteUser> GetCachedUser(uint32 ParticipantId);
	TSharedPtr<FMixerRemoteUser> GetCachedUser(FGuid ParticipantSessionId);
	void ReassignUsers(FName FromGroup, FName ToGroup);

private:
	TMap<FGuid, TSharedPtr<FMixerRemoteUser>> RemoteParticipantCacheByGuid;
	TMap<uint32, TSharedPtr<FMixerRemoteUser>> RemoteParticipantCacheByUint;

	TMap<FName, FMixerButtonPropertiesCached> Buttons;
	TMap<FName, FMixerStickPropertiesCached> Sticks;
	TMap<FName, FMixerLabelPropertiesCached> Labels;
	TMap<FName, FMixerTextboxPropertiesCached> Textboxes;

	bool bPerParticipantState;
};
