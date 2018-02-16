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
#include "Engine/MemberReference.h"

#include "MixerDynamicDelegateBinding.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FMixerButtonEventDynamicDelegate, FMixerButtonReference, Button, int32, ParticipantId, FMixerTransactionId, TransactionId, int32, SparkCost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMixerParticipantEventDynamicDelegate, int32, ParticipantId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FMixerStickEventDynamicDelegate, FMixerStickReference, Joystick, int32, ParticipantId, float, XAxis, float, YAxis);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMixerBroadcastingEventDynamicDelegate);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMixerCustomGlobalEventStubDelegate);

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

USTRUCT()
struct MIXERINTERACTIVITY_API FMixerCustomGlobalEventStubDelegateWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	FMemberReference PrototypeReference;

	UPROPERTY(transient)
	UFunction* FunctionPrototype;

	UPROPERTY()
	FMixerCustomGlobalEventStubDelegate Delegate;
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

	UPROPERTY()
	FMixerBroadcastingEventDynamicDelegate BroadcastingStartedDelegate;

	UPROPERTY()
	FMixerBroadcastingEventDynamicDelegate BroadcastingStoppedDelegate;

public:
	FMixerButtonEventDynamicDelegate* GetButtonEvent(FName ButtonName, bool Pressed);
	FMixerStickEventDynamicDelegate* GetStickEvent(FName StickName);
	void AddCustomGlobalEventBinding(FName EventName, UObject* TargetObject, FName TargetFunctionName);

	void OnButtonNativeEvent(FName ButtonName, TSharedPtr<const FMixerRemoteUser> Participant, const FMixerButtonEventDetails& Details);
	void OnParticipantStateChangedNativeEvent(TSharedPtr<const FMixerRemoteUser> Participant, EMixerInteractivityParticipantState NewState);
	void OnStickNativeEvent(FName StickName, TSharedPtr<const FMixerRemoteUser> Participant, FVector2D StickValue);
	void OnBroadcastingStateChangedNativeEvent(bool NewBroadcastingState);
	void OnCustomMethodCallNativeEvent(FName MethodName, const class FJsonObject* MethodParams);
	void OnUnhandledCustomControlInputNativeEvent(FName ControlName, FName MethodName, TSharedPtr<const FMixerRemoteUser> Participant, TSharedPtr<const FMixerSimpleCustomControl> ControlObject, const FJsonObject* EventPayload);
	void OnUnhandledCustomControlPropertyUpdateNativeEvent(FName ControlName, TSharedPtr<const FMixerSimpleCustomControl> ControlObject);


	virtual UWorld* GetWorld() const override;
	virtual void PostLoad() override;

private:

	UPROPERTY()
	TMap<FName, FMixerButtonEventDynamicDelegateWrapper> ButtonDelegates;

	UPROPERTY()
	TMap<FName, FMixerStickEventDynamicDelegateWrapper> StickDelegates;

	UPROPERTY()
	TMap<FName, FMixerCustomGlobalEventStubDelegateWrapper> CustomGlobalEventDelegates;

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

USTRUCT()
struct FMixerCustomGlobalEventBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FName TargetFunctionName;

	UPROPERTY()
	FName EventName;
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

	UPROPERTY()
	FName BroadcastingStartedBinding;

	UPROPERTY()
	FName BroadcastingStoppedBinding;

	void AddButtonBinding(const FMixerButtonEventBinding& BindingInfo);
	void AddStickBinding(const FMixerStickEventBinding& BindingInfo);
	void AddCustomGlobalEventBinding(const FMixerCustomGlobalEventBinding& BindingInfo);

public:
	virtual void BindDynamicDelegates(UObject* InInstance) const;
	virtual void UnbindDynamicDelegates(UObject* Instance) const;

private:

	UPROPERTY()
	TArray<FMixerButtonEventBinding> ButtonEventBindings;

	UPROPERTY()
	TArray<FMixerStickEventBinding> StickEventBindings;

	UPROPERTY()
	TArray<FMixerCustomGlobalEventBinding> CustomGlobalEventBindings;
};