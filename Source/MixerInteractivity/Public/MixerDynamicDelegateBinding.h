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
#include "Containers/Map.h"
#include "UObject/WeakObjectPtr.h"
#include "Delegates/Delegate.h"
#include "MixerInteractivityBlueprintLibrary.h"
#include "Engine/MemberReference.h"

#include "MixerDynamicDelegateBinding.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FMixerButtonEventDynamicDelegate, FMixerButtonReference, Button, int32, ParticipantId, FMixerTransactionId, TransactionId, int32, SparkCost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMixerParticipantEventDynamicDelegate, int32, ParticipantId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FMixerStickEventDynamicDelegate, FMixerStickReference, Joystick, int32, ParticipantId, float, XAxis, float, YAxis);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMixerBroadcastingEventDynamicDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FMixerTextSubmittedEventDynamicDelegate, FMixerTextboxReference, Textbox, int32, ParticipantId, FText, SubmittedText, FMixerTransactionId, TransactionId, int32, SparkCost);

// Note: these are for 'simple' custom controls only.  UObject-style controls get these events via method calls.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMixerCustomControlInputDynamicDelegate, FMixerCustomControlReference, Control, FName, Event, int32, ParticipantId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMixerCustomControlUpdateDynamicDelegate, FMixerCustomControlReference, Control);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMixerCustomMethodStubDelegate);

USTRUCT()
struct MIXERINTERACTIVITY_API FMixerButtonEventDynamicDelegateWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	FMixerButtonEventDynamicDelegate PressedDelegate;

	UPROPERTY()
	FMixerButtonEventDynamicDelegate ReleasedDelegate;

	bool IsBound()
	{
		return PressedDelegate.IsBound() || ReleasedDelegate.IsBound();
	}
};

USTRUCT()
struct MIXERINTERACTIVITY_API FMixerStickEventDynamicDelegateWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	FMixerStickEventDynamicDelegate Delegate;

	bool IsBound()
	{
		return Delegate.IsBound();
	}
};

USTRUCT()
struct MIXERINTERACTIVITY_API FMixerTextboxEventDynamicDelegateWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	FMixerTextSubmittedEventDynamicDelegate SubmittedDelegate;

	bool IsBound()
	{
		return SubmittedDelegate.IsBound();
	}
};

USTRUCT()
struct MIXERINTERACTIVITY_API FMixerCustomControlDelegateWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	UMixerCustomControl* MappedControl;

	UPROPERTY()
	FMixerCustomControlUpdateDynamicDelegate UpdateDelegate;

	UPROPERTY()
	FMixerCustomControlInputDynamicDelegate InputDelegate;

	TSharedPtr<FJsonObject> UnmappedControl;

	bool IsBound()
	{
		return MappedControl != nullptr || UpdateDelegate.IsBound() || InputDelegate.IsBound();
	}
};

USTRUCT()
struct MIXERINTERACTIVITY_API FMixerCustomMethodStubDelegateWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	FMemberReference PrototypeReference;

	UPROPERTY(transient)
	UFunction* FunctionPrototype;

	UPROPERTY()
	FMixerCustomMethodStubDelegate Delegate;

	bool IsBound()
	{
		return Delegate.IsBound();
	}
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
	FMixerCustomControlInputDynamicDelegate& GetCustomControlInputEvent(FName ControlName);
	FMixerCustomControlUpdateDynamicDelegate& GetCustomControlUpdateEvent(FName ControlName);
	void AddCustomMethodBinding(FName EventName, UObject* TargetObject, FName TargetFunctionName);
	void RemoveCustomMethodBinding(FName EventName, UObject* TargetObject, FName TargetFunctionName);
	void AddTextSubmittedBinding(FName TextboxName, UObject* TargetObject, FName TargetFunctionName);
	void RemoveTextSubmittedBinding(FName TextboxName, UObject* TargetObject, FName TargetFunctionName);

	void OnButtonNativeEvent(FName ButtonName, TSharedPtr<const FMixerRemoteUser> Participant, const FMixerButtonEventDetails& Details);
	void OnParticipantStateChangedNativeEvent(TSharedPtr<const FMixerRemoteUser> Participant, EMixerInteractivityParticipantState NewState);
	void OnStickNativeEvent(FName StickName, TSharedPtr<const FMixerRemoteUser> Participant, FVector2D StickValue);
	void OnBroadcastingStateChangedNativeEvent(bool NewBroadcastingState);
	void OnCustomMethodCallNativeEvent(FName MethodName, const TSharedPtr<FJsonObject> MethodParams);
	void OnCustomControlInputNativeEvent(FName ControlName, FName EventType, TSharedPtr<const FMixerRemoteUser> Participant, const TSharedRef<FJsonObject> EventPayload);
	void OnCustomControlPropertyUpdateNativeEvent(FName ControlName, const TSharedRef<FJsonObject> UpdatedProperties);
	void OnTextboxSubmitNativeEvent(FName TextboxName, TSharedPtr<const FMixerRemoteUser> Participant, const FMixerTextboxEventDetails& Details);

#if WITH_EDITORONLY_DATA
	void RefreshCustomControls();
	void OnCustomControlCompiled(class UBlueprint* CompiledBP);
#endif

	virtual UWorld* GetWorld() const override;
	virtual void PostLoad() override;

	UMixerCustomControl* GetMappedCustomControl(FName ControlName);
	TSharedPtr<FJsonObject> GetUnmappedCustomControl(FName ControlName);

private:

	UPROPERTY()
	TMap<FName, FMixerButtonEventDynamicDelegateWrapper> ButtonDelegates;

	UPROPERTY()
	TMap<FName, FMixerStickEventDynamicDelegateWrapper> StickDelegates;

	UPROPERTY()
	TMap<FName, FMixerCustomMethodStubDelegateWrapper> CustomMethodDelegates;

	UPROPERTY()
	TMap<FName, FMixerCustomControlDelegateWrapper> CustomControlDelegates;

	UPROPERTY()
	TMap<FName, FMixerTextboxEventDynamicDelegateWrapper> TextboxDelegates;

public:
	static UMixerInteractivityBlueprintEventSource* GetBlueprintEventSource(UWorld* ForWorld);

	void RegisterForMixerEvents();
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

UENUM()
enum class EMixerGenericEventBindingType : uint8
{
	Stick,
	CustomMethod,
	TextSubmitted,
};

USTRUCT()
struct FMixerGenericEventBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FName TargetFunctionName;

	UPROPERTY()
	FName NameParam;

	UPROPERTY()
	EMixerGenericEventBindingType BindingType;
};

USTRUCT()
struct FMixerCustomControlEventBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FName TargetFunctionName;

	UPROPERTY()
	FName ControlId;
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
	void AddCustomControlInputBinding(const FMixerCustomControlEventBinding& BindingInfo);
	void AddCustomControlUpdateBinding(const FMixerCustomControlEventBinding& BindingInfo);
	void AddGenericBinding(const FMixerGenericEventBinding& BindingInfo);

public:
	virtual void BindDynamicDelegates(UObject* InInstance) const;
	virtual void UnbindDynamicDelegates(UObject* Instance) const;

private:

	UPROPERTY()
	TArray<FMixerButtonEventBinding> ButtonEventBindings;

	UPROPERTY()
	TArray<FMixerCustomControlEventBinding> CustomControlInputBindings;

	UPROPERTY()
	TArray<FMixerCustomControlEventBinding> CustomControlUpdateBindings;

	UPROPERTY()
	TArray<FMixerGenericEventBinding> GenericBindings;
};