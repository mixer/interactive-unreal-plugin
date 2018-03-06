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

#include "Templates/SharedPointer.h"
#include "MixerCustomControl.generated.h"

UCLASS(Blueprintable, BlueprintType)
class MIXERINTERACTIVITY_API UMixerCustomControl : public UObject
{
	GENERATED_BODY()
public:
	/**
	* Interval (seconds) at which instances of this control are checked for updates.
	* During an update pass changed properties that are flagged as client-writable
	* will be batched up and transmitted to the Mixer Interactive service.
	*/
	UPROPERTY(EditAnywhere, Category="Property Replication", meta=(UIMin=0, ClampMin=0))
	float ClientPropertyUpdateInterval;

	/**
	* Opportunity for C++ code to receive a notification when property updates
	* have been received from the Mixer Interactive service.  At the point that
	* this method is called the new values will have already been applied to
	* matching UPROPERTY fields on the control instance.
	* Default implementation forwards the event to Blueprint.
	*/
	virtual void NativeOnServerPropertiesUpdated();

protected:

	/**
	* Collect the set of UProperties that may be written by the client and should
	* be transmitted to the server.  If this collection is non-empty the control
	* instance will be ticked every ClientPropertyUpdateInterval seconds.
	* Default implementation collects all properties that are BlueprintReadWrite.
	*
	* @param	OutProperties		Out parameter to be filled with UProperty instances for which updates should be sent client->server
	*/
	virtual void GetClientWritableProperties(TArray<UProperty*>& OutProperties);

	// Implementation details
public:
	UPROPERTY()
	FName SceneName;

	UPROPERTY()
	FName ControlName;

public:
	virtual ~UMixerCustomControl();

	virtual void PostLoad() override;

public:
	bool Tick(float DeltaTime);

	UFUNCTION(BlueprintImplementableEvent)
	void OnServerPropertiesUpdated();

private:
	void InitClientWrittenPropertyMaintenance();

private:
	TArray<UProperty*> ClientWritableProperties;
	TArray<uint8> LastSentPropertyData;
};