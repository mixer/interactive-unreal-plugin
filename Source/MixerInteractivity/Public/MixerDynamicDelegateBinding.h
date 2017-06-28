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

#include "UObject/Object.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "MixerInteractivityModule.h"
#include "Map.h"
#include "UObject/WeakObjectPtr.h"
#include "Delegates/Delegate.h"
#include "MixerInteractivityBlueprintLibrary.h"

#include "MixerDynamicDelegateBinding.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMixerButtonEventDynamicDelegate, FMixerButtonReference, Button, int32, ParticipantId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMixerParticipantEventDynamicDelegate, int32, ParticipantId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FMixerStickEventDynamicDelegate, FMixerStickReference, Joystick, int32, ParticipantId, float, XAxis, float, YAxis);

USTRUCT()
struct MIXERINTERACTIVITY_API FMixerButtonEventDynamicDelegateWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	FMixerButtonEventDynamicDelegate PressedDelegate;

	UPROPERTY()
	FMixerButtonEventDynamicDelegate ReleasedDelegate;
};

USTRUCT()
struct MIXERINTERACTIVITY_API FMixerStickEventDynamicDelegateWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	FMixerStickEventDynamicDelegate Delegate;
};


UCLASS()
class MIXERINTERACTIVITY_API UMixerInteractivityBlueprintEventSource : public UObject
{
	GENERATED_BODY()

public:
	UMixerInteractivityBlueprintEventSource(const FObjectInitializer& Initializer);

public:
	UPROPERTY()
	FMixerParticipantEventDynamicDelegate ParticipantJoinedDelegate;

	UPROPERTY()
	FMixerParticipantEventDynamicDelegate ParticipantLeftDelegate;

	UPROPERTY()
	FMixerParticipantEventDynamicDelegate ParticipantInputDisabledDelegate;

public:
	FMixerButtonEventDynamicDelegate* GetButtonEvent(FName ButtonName, bool Pressed);
	FMixerStickEventDynamicDelegate* GetStickEvent(FName StickName);

	void OnButtonNativeEvent(FName ButtonName, TSharedPtr<const FMixerRemoteUser> Participant, bool Pressed);
	void OnParticipantStateChangedNativeEvent(TSharedPtr<const FMixerRemoteUser> Participant, EMixerInteractivityParticipantState NewState);
	void OnStickNativeEvent(FName StickName, TSharedPtr<const FMixerRemoteUser> Participant, FVector2D StickValue);

	virtual UWorld* GetWorld() const override;

private:

	UPROPERTY()
	TMap< FName, FMixerButtonEventDynamicDelegateWrapper > ButtonDelegates;

	UPROPERTY()
	TMap< FName, FMixerStickEventDynamicDelegateWrapper > StickDelegates;

public:
	static UMixerInteractivityBlueprintEventSource* GetBlueprintEventSource(UWorld* ForWorld);

private:
	static TArray<TWeakObjectPtr<UMixerInteractivityBlueprintEventSource>> BlueprintEventSources;

};

USTRUCT()
struct FMixerButtonEventBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FName TargetFunctionName;

	UPROPERTY()
	FName ButtonId;

	UPROPERTY()
	bool Pressed;
};

USTRUCT()
struct FMixerStickEventBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FName TargetFunctionName;

	UPROPERTY()
	FName StickId;
};

UCLASS()
class MIXERINTERACTIVITY_API UMixerDelegateBinding : public UDynamicBlueprintBinding
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FName ParticipantJoinedBinding;

	UPROPERTY()
	FName ParticipantLeftBinding;

	UPROPERTY()
	FName ParticipantInputDisabledBinding;

	void AddButtonBinding(const FMixerButtonEventBinding& BindingInfo);
	void AddStickBinding(const FMixerStickEventBinding& BindingInfo);

public:
	virtual void BindDynamicDelegates(UObject* InInstance) const;
	virtual void UnbindDynamicDelegates(UObject* Instance) const;

private:

	UPROPERTY()
	TArray<FMixerButtonEventBinding> ButtonEventBindings;

	UPROPERTY()
	TArray<FMixerStickEventBinding> StickEventBindings;
};