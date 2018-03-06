//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MixerDynamicDelegateBinding.h"
#include "MixerInteractivityLog.h"
#include "MixerBindingUtils.h"
#include "MixerInteractivitySettings.h"
#include "MixerInteractivityProjectAsset.h"
#include "Engine/World.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Engine/BlueprintGeneratedClass.h"

TArray<TWeakObjectPtr<UMixerInteractivityBlueprintEventSource>> UMixerInteractivityBlueprintEventSource::BlueprintEventSources;

UMixerInteractivityBlueprintEventSource::UMixerInteractivityBlueprintEventSource(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	UWorld* World = GetWorld();
	if (World)
	{
		BlueprintEventSources.Add(TWeakObjectPtr<UMixerInteractivityBlueprintEventSource>(this));
		World->ExtraReferencedObjects.Add(this);
	}
}

void UMixerInteractivityBlueprintEventSource::RegisterForMixerEvents()
{
	// Use GetModuleChecked here to avoid unsafe non-game thread warning
	IMixerInteractivityModule& InteractivityModule = FModuleManager::GetModuleChecked<IMixerInteractivityModule>("MixerInteractivity");
	InteractivityModule.OnButtonEvent().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnButtonNativeEvent);
	InteractivityModule.OnParticipantStateChanged().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnParticipantStateChangedNativeEvent);
	InteractivityModule.OnStickEvent().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnStickNativeEvent);
	InteractivityModule.OnBroadcastingStateChanged().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnBroadcastingStateChangedNativeEvent);
	InteractivityModule.OnCustomControlInput().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnCustomControlInputNativeEvent);
	InteractivityModule.OnCustomControlPropertyUpdate().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnCustomControlPropertyUpdateNativeEvent);
	InteractivityModule.OnCustomMethodCall().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnCustomMethodCallNativeEvent);
}


UMixerInteractivityBlueprintEventSource* UMixerInteractivityBlueprintEventSource::GetBlueprintEventSource(UWorld* ForWorld)
{
	for (int32 i = 0; i < BlueprintEventSources.Num(); ++i)
	{
		UMixerInteractivityBlueprintEventSource* ExistingSource = BlueprintEventSources[i].Get();
		if (ExistingSource && ExistingSource->GetWorld() == ForWorld)
		{
			return ExistingSource;
		}
	}

	return NewObject<UMixerInteractivityBlueprintEventSource>(ForWorld);
}

FMixerButtonEventDynamicDelegate* UMixerInteractivityBlueprintEventSource::GetButtonEvent(FName ButtonName, bool Pressed)
{
	FMixerButtonEventDynamicDelegateWrapper& DelegateWrapper = ButtonDelegates.FindOrAdd(ButtonName);
	return Pressed ? &DelegateWrapper.PressedDelegate : &DelegateWrapper.ReleasedDelegate;
}

void UMixerInteractivityBlueprintEventSource::AddCustomMethodBinding(FName EventName, UObject* TargetObject, FName TargetFunctionName)
{
	FMixerCustomMethodStubDelegateWrapper& DelegateWrapper = CustomMethodDelegates.FindOrAdd(EventName);
	FScriptDelegate SingleDelegate;
	SingleDelegate.BindUFunction(TargetObject, TargetFunctionName);
	DelegateWrapper.Delegate.AddUnique(SingleDelegate);

	if (DelegateWrapper.FunctionPrototype == nullptr)
	{
		DelegateWrapper.PrototypeReference.SetExternalMember(TargetFunctionName, TargetObject->GetClass());
		DelegateWrapper.FunctionPrototype = DelegateWrapper.PrototypeReference.ResolveMember<UFunction>(static_cast<UClass*>(nullptr));
	}
}

FMixerStickEventDynamicDelegate* UMixerInteractivityBlueprintEventSource::GetStickEvent(FName StickName)
{
	FMixerStickEventDynamicDelegateWrapper& DelegateWrapper = StickDelegates.FindOrAdd(StickName);
	return &DelegateWrapper.Delegate;
}

FMixerCustomControlInputDynamicDelegate& UMixerInteractivityBlueprintEventSource::GetCustomControlInputEvent(FName ControlName)
{
	return CustomControlDelegates.FindOrAdd(ControlName).InputDelegate;
}

FMixerCustomControlUpdateDynamicDelegate& UMixerInteractivityBlueprintEventSource::GetCustomControlUpdateEvent(FName ControlName)
{
	return CustomControlDelegates.FindOrAdd(ControlName).UpdateDelegate;
}

void UMixerInteractivityBlueprintEventSource::OnButtonNativeEvent(FName ButtonName, TSharedPtr<const FMixerRemoteUser> Participant, const FMixerButtonEventDetails& Details)
{
	FMixerButtonEventDynamicDelegateWrapper* DelegateWrapper = ButtonDelegates.Find(ButtonName);
	if (DelegateWrapper)
	{
		FMixerButtonReference ButtonRef;
		ButtonRef.Name = ButtonName;
		FMixerButtonEventDynamicDelegate& DelegateToFire = Details.Pressed ? DelegateWrapper->PressedDelegate : DelegateWrapper->ReleasedDelegate;
		FMixerTransactionId TransactionId;
		TransactionId.Id = Details.TransactionId;
		DelegateToFire.Broadcast(ButtonRef, static_cast<int32>(Participant->Id), TransactionId, static_cast<int32>(Details.SparkCost));
	}
}

void UMixerInteractivityBlueprintEventSource::OnStickNativeEvent(FName StickName, TSharedPtr<const FMixerRemoteUser> Participant, FVector2D StickValue)
{
	FMixerStickEventDynamicDelegateWrapper* DelegateWrapper = StickDelegates.Find(StickName);
	if (DelegateWrapper)
	{
		FMixerStickReference StickRef;
		StickRef.Name = StickName;
		DelegateWrapper->Delegate.Broadcast(StickRef, static_cast<int32>(Participant->Id), StickValue.X, StickValue.Y);
	}
}

void UMixerInteractivityBlueprintEventSource::OnParticipantStateChangedNativeEvent(TSharedPtr<const FMixerRemoteUser> Participant, EMixerInteractivityParticipantState NewState)
{
	check(Participant.IsValid());
	switch (NewState)
	{
	case EMixerInteractivityParticipantState::Joined:
		ParticipantJoinedDelegate.Broadcast(static_cast<int32>(Participant->Id));
		break;
	case EMixerInteractivityParticipantState::Left:
		ParticipantLeftDelegate.Broadcast(static_cast<int32>(Participant->Id));
		break;
	case EMixerInteractivityParticipantState::Input_Disabled:
		ParticipantInputDisabledDelegate.Broadcast(static_cast<int32>(Participant->Id));
		break;
	default:
		UE_LOG(LogMixerInteractivity, Warning, TEXT("Received participant state changed event with unknown state %d"), static_cast<uint8>(NewState));
		break;
	}
}

void UMixerInteractivityBlueprintEventSource::OnBroadcastingStateChangedNativeEvent(bool NewBroadcastingState)
{
	if (NewBroadcastingState)
	{
		BroadcastingStartedDelegate.Broadcast();
	}
	else
	{
		BroadcastingStoppedDelegate.Broadcast();
	}
}

void UMixerInteractivityBlueprintEventSource::OnCustomMethodCallNativeEvent(FName MethodName, const TSharedRef<FJsonObject> MethodParams)
{
	UFunction* FunctionPrototype = nullptr;
	const FMulticastScriptDelegate* BlueprintEvent = nullptr;
	const FMulticastScriptDelegate* NativeEvent = nullptr;
	FMixerCustomMethodStubDelegateWrapper* DelegateWrapper = CustomMethodDelegates.Find(MethodName);
	if (DelegateWrapper != nullptr)
	{
		FunctionPrototype = DelegateWrapper->FunctionPrototype;
		BlueprintEvent = &DelegateWrapper->Delegate;
	}

	if (FunctionPrototype != nullptr && BlueprintEvent != nullptr)
	{
		void* ParamStorage = FMemory_Alloca(FunctionPrototype->ParmsSize);
		if (ParamStorage != nullptr)
		{
			MixerBindingUtils::ExtractCustomEventParamsFromMessage(&MethodParams.Get(), FunctionPrototype, ParamStorage, FunctionPrototype->ParmsSize);
			BlueprintEvent->ProcessMulticastDelegate<UObject>(ParamStorage);
			MixerBindingUtils::DestroyCustomEventParams(FunctionPrototype, ParamStorage, FunctionPrototype->ParmsSize);
		}
	}
}

void UMixerInteractivityBlueprintEventSource::OnCustomControlInputNativeEvent(FName ControlName, FName EventType, TSharedPtr<const FMixerRemoteUser> Participant, const TSharedRef<FJsonObject> EventPayload)
{
	FMixerCustomControlDelegateWrapper* Wrapper = CustomControlDelegates.Find(ControlName);
	if (Wrapper != nullptr)
	{
		if (Wrapper->MappedControl != nullptr)
		{
			UFunction* HandlerMethod = Wrapper->MappedControl->FindFunction(EventType);
			if (HandlerMethod != nullptr)
			{
				void* ParamStorage = FMemory_Alloca(HandlerMethod->ParmsSize);
				if (ParamStorage != nullptr)
				{
					MixerBindingUtils::ExtractCustomEventParamsFromMessage(&EventPayload.Get(), HandlerMethod, ParamStorage, HandlerMethod->ParmsSize);
					Wrapper->MappedControl->ProcessEvent(HandlerMethod, ParamStorage);
					MixerBindingUtils::DestroyCustomEventParams(HandlerMethod, ParamStorage, HandlerMethod->ParmsSize);
				}
			}
			else
			{
				UE_LOG(LogMixerInteractivity, Warning, TEXT("Received unknown input event %s for custom control %s.  Event will be ignored.  Provide a UFUNCTION matching the event name in order to handle this event."), *EventType.ToString(), *ControlName.ToString());
			}
		}
		else if (Wrapper->InputDelegate.IsBound())
		{
			FMixerCustomControlReference ControlRef;
			ControlRef.Name = ControlName;
			Wrapper->InputDelegate.Broadcast(ControlRef, EventType, Participant.IsValid() ? static_cast<int32>(Participant->Id) : 0);
		}
	}
}

void UMixerInteractivityBlueprintEventSource::OnCustomControlPropertyUpdateNativeEvent(FName ControlName, const TSharedRef<FJsonObject> UpdatedProperties)
{
	FMixerCustomControlDelegateWrapper* Wrapper = CustomControlDelegates.Find(ControlName);
	if (Wrapper != nullptr)
	{
		if (Wrapper->MappedControl != nullptr)
		{
			FJsonObjectConverter::JsonObjectToUStruct(UpdatedProperties, Wrapper->MappedControl->GetClass(), Wrapper->MappedControl);
			Wrapper->MappedControl->NativeOnServerPropertiesUpdated();
		}
		else if (Wrapper->UpdateDelegate.IsBound())
		{
			if (!Wrapper->UnmappedControl.IsValid())
			{
				Wrapper->UnmappedControl = MakeShared<FJsonObject>();;
			}

			Wrapper->UnmappedControl->Values.Append(UpdatedProperties->Values);

			FMixerCustomControlReference ControlRef;
			ControlRef.Name = ControlName;
			Wrapper->UpdateDelegate.Broadcast(ControlRef);
		}
	}
}

#if WITH_EDITORONLY_DATA
void UMixerInteractivityBlueprintEventSource::RefreshCustomControls()
{
	bool bCustomControlsChanged = false;
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	UMixerProjectAsset* ProjectAsset = Cast<UMixerProjectAsset>(Settings->ProjectDefinition.TryLoad());
	if (ProjectAsset != nullptr)
	{
		// Ensure map is populated.
		for (TMap<FName, FMixerCustomControlMapping>::TIterator It(ProjectAsset->CustomControlMappings); It; ++It)
		{
			CustomControlDelegates.FindOrAdd(It->Key);
		}

		// Remove any custom control instances that are now obsolete
		for (TMap<FName, FMixerCustomControlDelegateWrapper>::TIterator It(CustomControlDelegates); It; ++It)
		{
			const FMixerCustomControlMapping* ClassBinding = ProjectAsset->CustomControlMappings.Find(It->Key);
			if (ClassBinding != nullptr)
			{
				UClass* ControlClass = ClassBinding->Class.TryLoadClass<UMixerCustomControl>();
				if (ControlClass != nullptr)
				{
					if (It->Value.MappedControl == nullptr || ControlClass != It->Value.MappedControl->GetClass())
					{
						It->Value.MappedControl = NewObject<UMixerCustomControl>(this, ControlClass);
						It->Value.MappedControl->ControlName = It->Key;
						It->Value.MappedControl->SceneName = ClassBinding->SceneName;
						UBlueprintGeneratedClass::BindDynamicDelegates(ControlClass, It->Value.MappedControl);
						bCustomControlsChanged = true;
					}

					UBlueprint* GeneratedByBP = Cast<UBlueprint>(ControlClass->ClassGeneratedBy);
					if (GeneratedByBP && !GeneratedByBP->OnCompiled().IsBoundToObject(this))
					{
						GeneratedByBP->OnCompiled().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnCustomControlCompiled);
					}
				}

				if (It->Value.UpdateDelegate.IsBound() || It->Value.InputDelegate.IsBound())
				{
					// Bound both with an object instance and via global delegates.  Probably shouldn't allow this
				}
			}
			else if (It->Value.MappedControl != nullptr)
			{
				UBlueprint* GeneratedByBP = Cast<UBlueprint>(It->Value.MappedControl->GetClass()->ClassGeneratedBy);
				if (GeneratedByBP)
				{
					GeneratedByBP->OnCompiled().RemoveAll(this);
				}
				It->Value.MappedControl = nullptr;
				bCustomControlsChanged = true;

				if (!It->Value.InputDelegate.IsBound() && !It->Value.UpdateDelegate.IsBound())
				{
					It.RemoveCurrent();
				}
			}
		}
	}

#if WITH_EDITOR
	if (GIsEditor && bCustomControlsChanged)
	{
		GetOutermost()->SetDirtyFlag(true);
	}
#endif
}

void UMixerInteractivityBlueprintEventSource::OnCustomControlCompiled(UBlueprint* CompiledBP)
{
	for (TMap<FName, FMixerCustomControlDelegateWrapper>::TIterator It(CustomControlDelegates); It; ++It)
	{
		UMixerCustomControl* ControlObj = It->Value.MappedControl;
		if (ControlObj != nullptr)
		{
			UBlueprint* GeneratedByBP = Cast<UBlueprint>(ControlObj->GetClass()->ClassGeneratedBy);
			if (GeneratedByBP == CompiledBP)
			{
				UBlueprintGeneratedClass::BindDynamicDelegates(ControlObj->GetClass(), ControlObj);
			}
		}
	}
}
#endif

UWorld* UMixerInteractivityBlueprintEventSource::GetWorld() const
{
	return Cast<UWorld>(GetOuter());
}

void UMixerInteractivityBlueprintEventSource::PostLoad() 
{
	Super::PostLoad();

#if WITH_EDITOR
	bool bFoundOutOfDateEvents = false;
	for (TMap<FName, FMixerButtonEventDynamicDelegateWrapper>::TIterator It(ButtonDelegates); It; ++It)
	{
		if (!It->Value.PressedDelegate.IsBound() && !It->Value.ReleasedDelegate.IsBound())
		{
			bFoundOutOfDateEvents = true;
			It.RemoveCurrent();
		}
	}

	for (TMap<FName, FMixerStickEventDynamicDelegateWrapper>::TIterator It(StickDelegates); It; ++It)
	{
		if (!It->Value.Delegate.IsBound())
		{
			bFoundOutOfDateEvents = true;
			It.RemoveCurrent();
		}
	}
#endif

	for (TMap<FName, FMixerCustomMethodStubDelegateWrapper>::TIterator It(CustomMethodDelegates); It; ++It)
	{
#if WITH_EDITOR
		if (!It->Value.Delegate.IsBound())
		{
			bFoundOutOfDateEvents = true;
			It.RemoveCurrent();
		}
		else
#endif
		{
			It->Value.FunctionPrototype = It->Value.PrototypeReference.ResolveMember<UFunction>(static_cast<UClass*>(nullptr));
		}
	}

#if WITH_EDITOR
	if (GIsEditor && bFoundOutOfDateEvents)
	{
		GetOutermost()->SetDirtyFlag(true);

		FMessageLog("LoadErrors").Warning()
			->AddToken(FUObjectToken::Create(GetOutermost()))
			->AddToken(FTextToken::Create(NSLOCTEXT("MixerInteractivity", "ResaveNeeded_OutOfDateEvents", "References to out-of-date Mixer events found.  Please resave.")));
	}
#endif

	UWorld* World = GetWorld();
	if (World != nullptr && World->IsGameWorld())
	{
		RegisterForMixerEvents();
	}
}

UMixerCustomControl* UMixerInteractivityBlueprintEventSource::GetMappedCustomControl(FName ControlName)
{
	FMixerCustomControlDelegateWrapper* Wrapper = CustomControlDelegates.Find(ControlName);
	return Wrapper != nullptr ? Wrapper->MappedControl : nullptr;
}

TSharedPtr<FJsonObject> UMixerInteractivityBlueprintEventSource::GetUnmappedCustomControl(FName ControlName)
{
	FMixerCustomControlDelegateWrapper* Wrapper = CustomControlDelegates.Find(ControlName);
	return Wrapper != nullptr ? Wrapper->UnmappedControl : nullptr;
}

void UMixerDelegateBinding::AddButtonBinding(const FMixerButtonEventBinding& BindingInfo)
{
	ButtonEventBindings.Add(BindingInfo);
}

void UMixerDelegateBinding::AddStickBinding(const FMixerStickEventBinding& BindingInfo)
{
	StickEventBindings.Add(BindingInfo);
}

void UMixerDelegateBinding::AddCustomMethodBinding(const FMixerCustomMethodBinding& BindingInfo)
{
	CustomMethodBindings.Add(BindingInfo);
}

void UMixerDelegateBinding::AddCustomControlInputBinding(const FMixerCustomControlEventBinding& BindingInfo)
{
	CustomControlInputBindings.Add(BindingInfo);
}

void UMixerDelegateBinding::AddCustomControlUpdateBinding(const FMixerCustomControlEventBinding& BindingInfo)
{
	CustomControlUpdateBindings.Add(BindingInfo);
}

void UMixerDelegateBinding::BindDynamicDelegates(UObject* InInstance) const
{
	UMixerInteractivityBlueprintEventSource* EventSource = UMixerInteractivityBlueprintEventSource::GetBlueprintEventSource(InInstance->GetWorld());
	check(EventSource);

	for (const FMixerButtonEventBinding& ButtonBinding : ButtonEventBindings)
	{
		FMixerButtonEventDynamicDelegate* Event = EventSource->GetButtonEvent(ButtonBinding.ButtonId, ButtonBinding.Pressed);
		if (Event)
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(InInstance, ButtonBinding.TargetFunctionName);
			Event->AddUnique(Delegate);
		}
	}

	for (const FMixerStickEventBinding& StickBinding : StickEventBindings)
	{
		FMixerStickEventDynamicDelegate* Event = EventSource->GetStickEvent(StickBinding.StickId);
		if (Event)
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(InInstance, StickBinding.TargetFunctionName);
			Event->AddUnique(Delegate);
		}
	}

	for (const FMixerCustomMethodBinding& CustomEventBinding : CustomMethodBindings)
	{
		EventSource->AddCustomMethodBinding(CustomEventBinding.EventName, InInstance, CustomEventBinding.TargetFunctionName);
	}

	for (const FMixerCustomControlEventBinding& CustomControlBinding : CustomControlInputBindings)
	{
		FMixerCustomControlInputDynamicDelegate& Event = EventSource->GetCustomControlInputEvent(CustomControlBinding.ControlId);
		FScriptDelegate Delegate;
		Delegate.BindUFunction(InInstance, CustomControlBinding.TargetFunctionName);
		Event.AddUnique(Delegate);
	}


	for (const FMixerCustomControlEventBinding& CustomControlBinding : CustomControlUpdateBindings)
	{
		FMixerCustomControlUpdateDynamicDelegate& Event = EventSource->GetCustomControlUpdateEvent(CustomControlBinding.ControlId);
		FScriptDelegate Delegate;
		Delegate.BindUFunction(InInstance, CustomControlBinding.TargetFunctionName);
		Event.AddUnique(Delegate);
	}

	if (ParticipantJoinedBinding != NAME_None)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(InInstance, ParticipantJoinedBinding);
		EventSource->ParticipantJoinedDelegate.AddUnique(Delegate);
	}

	if (ParticipantLeftBinding != NAME_None)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(InInstance, ParticipantLeftBinding);
		EventSource->ParticipantLeftDelegate.AddUnique(Delegate);
	}

	if (ParticipantInputDisabledBinding != NAME_None)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(InInstance, ParticipantInputDisabledBinding);
		EventSource->ParticipantInputDisabledDelegate.AddUnique(Delegate);
	}

	if (BroadcastingStartedBinding != NAME_None)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(InInstance, BroadcastingStartedBinding);
		EventSource->BroadcastingStartedDelegate.AddUnique(Delegate);
	}

	if (BroadcastingStoppedBinding != NAME_None)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(InInstance, BroadcastingStoppedBinding);
		EventSource->BroadcastingStoppedDelegate.AddUnique(Delegate);
	}
}

void UMixerDelegateBinding::UnbindDynamicDelegates(UObject* InInstance) const
{
	UMixerInteractivityBlueprintEventSource* EventSource = UMixerInteractivityBlueprintEventSource::GetBlueprintEventSource(InInstance->GetWorld());
	check(EventSource);
	for (const FMixerButtonEventBinding& ButtonBinding : ButtonEventBindings)
	{
		FMixerButtonEventDynamicDelegate* Event = EventSource->GetButtonEvent(ButtonBinding.ButtonId, ButtonBinding.Pressed);
		if (Event)
		{
			Event->Remove(InInstance, ButtonBinding.TargetFunctionName);
		}
	}

	if (ParticipantJoinedBinding != NAME_None)
	{
		EventSource->ParticipantJoinedDelegate.Remove(InInstance, ParticipantJoinedBinding);
	}

	if (ParticipantLeftBinding != NAME_None)
	{
		EventSource->ParticipantLeftDelegate.Remove(InInstance, ParticipantLeftBinding);
	}

	if (ParticipantInputDisabledBinding != NAME_None)
	{
		EventSource->ParticipantInputDisabledDelegate.Remove(InInstance, ParticipantInputDisabledBinding);
	}

	if (BroadcastingStartedBinding != NAME_None)
	{
		EventSource->BroadcastingStartedDelegate.Remove(InInstance, BroadcastingStartedBinding);
	}

	if (BroadcastingStoppedBinding != NAME_None)
	{
		EventSource->BroadcastingStoppedDelegate.Remove(InInstance, BroadcastingStartedBinding);
	}
}
