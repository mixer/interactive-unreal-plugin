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
#include "MixerInteractivitySettings.h"
#include "MixerInteractivityCustomGlobalEvents.h"
#include "Engine/World.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

TArray<TWeakObjectPtr<UMixerInteractivityBlueprintEventSource>> UMixerInteractivityBlueprintEventSource::BlueprintEventSources;

UMixerInteractivityBlueprintEventSource::UMixerInteractivityBlueprintEventSource(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	UWorld* World = GetWorld();
	if (World)
	{
		BlueprintEventSources.Add(TWeakObjectPtr<UMixerInteractivityBlueprintEventSource>(this));
		World->ExtraReferencedObjects.Add(this);

		// Use GetModuleChecked here to avoid unsafe non-game thread warning
		IMixerInteractivityModule& InteractivityModule = FModuleManager::GetModuleChecked<IMixerInteractivityModule>("MixerInteractivity");
		InteractivityModule.OnButtonEvent().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnButtonNativeEvent);
		InteractivityModule.OnParticipantStateChanged().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnParticipantStateChangedNativeEvent);
		InteractivityModule.OnStickEvent().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnStickNativeEvent);
		InteractivityModule.OnBroadcastingStateChanged().AddUObject(this, &UMixerInteractivityBlueprintEventSource::OnBroadcastingStateChangedNativeEvent);
	}
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

void UMixerInteractivityBlueprintEventSource::AddCustomGlobalEventBinding(const FString& EventName, UObject* TargetObject, FName TargetFunctionName)
{
	FMixerCustomGlobalEventStubDelegateWrapper& DelegateWrapper = CustomGlobalEventDelegates.FindOrAdd(EventName);
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

void UMixerInteractivityBlueprintEventSource::ExtractCustomEventParamsFromMessage(FJsonObject* JsonObject, UFunction* FunctionPrototype, void* ParamStorage, SIZE_T ParamStorageSize)
{
	check(ParamStorageSize == FunctionPrototype->ParmsSize);
	for (TFieldIterator<UProperty> PropIt(FunctionPrototype); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		if (!PropIt->HasAnyPropertyFlags(CPF_OutParm) || PropIt->HasAnyPropertyFlags(CPF_ReferenceParm))
		{
			check(PropIt->GetOffset_ForUFunction() + PropIt->GetSize() <= ParamStorageSize);
			void* ThisParamStorage = static_cast<uint8*>(ParamStorage) + PropIt->GetOffset_ForUFunction();
			PropIt->InitializeValue(ThisParamStorage);
			TSharedPtr<FJsonValue> F = JsonObject->TryGetField(PropIt->GetName());
			if (F.IsValid())
			{
				PropIt->ImportText(*F->AsString(), ThisParamStorage, 0, nullptr);
			}
			else
			{
				UE_LOG(LogMixerInteractivity, Error, TEXT("Custom event %s does not contain expected parameter %s"), *JsonObject->GetStringField(TEXT("method")), *PropIt->GetName());
			}
		}
	}
}

void UMixerInteractivityBlueprintEventSource::DestroyCustomEventParams(UFunction* FunctionPrototype, void* ParamStorage, SIZE_T ParamStorageSize)
{
	for (TFieldIterator<UProperty> PropIt(FunctionPrototype); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		if (!PropIt->HasAnyPropertyFlags(CPF_OutParm) || PropIt->HasAnyPropertyFlags(CPF_ReferenceParm))
		{
			check(PropIt->GetOffset_ForUFunction() + PropIt->GetSize() <= ParamStorageSize);
			void* ThisParamStorage = static_cast<uint8*>(ParamStorage) + PropIt->GetOffset_ForUFunction();
			PropIt->DestroyValue(ThisParamStorage);
		}
	}
}


void UMixerInteractivityBlueprintEventSource::FindDelegatesForCustomEvent(const FString& Method, UFunction*& FunctionPrototype, const FMulticastScriptDelegate*& BlueprintEvent, const FMulticastScriptDelegate*& NativeEvent)
{
	FMixerCustomGlobalEventStubDelegateWrapper* DelegateWrapper = CustomGlobalEventDelegates.Find(Method);
	if (DelegateWrapper != nullptr)
	{
		FunctionPrototype = DelegateWrapper->FunctionPrototype;
		BlueprintEvent = &DelegateWrapper->Delegate;
	}

	UClass* CustomEventsDefinition = GetDefault<UMixerInteractivitySettings>()->CustomGlobalEvents;
	if (CustomEventsDefinition != nullptr)
	{
		UMulticastDelegateProperty* PossibleNativeProperty = Cast<UMulticastDelegateProperty>(CustomEventsDefinition->FindPropertyByName(FName(*Method)));
		if (PossibleNativeProperty != nullptr && PossibleNativeProperty->IsNative())
		{
			if (FunctionPrototype == nullptr)
			{
				FunctionPrototype = PossibleNativeProperty->SignatureFunction;
			}
			else
			{
				check(FunctionPrototype->IsSignatureCompatibleWith(PossibleNativeProperty->SignatureFunction));
			}
			NativeEvent = &PossibleNativeProperty->GetPropertyValue_InContainer(CustomEventsDefinition->GetDefaultObject<UObject>());
		}
	}
}


void UMixerInteractivityBlueprintEventSource::OnCustomMessageNativeEvent(const FString& MessageBodyString)
{
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(MessageBodyString);
	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		FString Method;
		if (JsonObject->TryGetStringField(TEXT("method"), Method))
		{
			UFunction* FunctionPrototype = nullptr;
			const FMulticastScriptDelegate* BlueprintEvent = nullptr;
			const FMulticastScriptDelegate* NativeEvent = nullptr;
			FindDelegatesForCustomEvent(Method, FunctionPrototype, BlueprintEvent, NativeEvent);

			if (FunctionPrototype != nullptr && (BlueprintEvent != nullptr || NativeEvent != nullptr))
			{
				void* ParamStorage = FMemory_Alloca(FunctionPrototype->ParmsSize);
				if (ParamStorage != nullptr)
				{
					ExtractCustomEventParamsFromMessage(JsonObject.Get(), FunctionPrototype, ParamStorage, FunctionPrototype->ParmsSize);

					if (BlueprintEvent != nullptr)
					{
						BlueprintEvent->ProcessMulticastDelegate<UObject>(ParamStorage);
					}

					if (NativeEvent != nullptr)
					{
						NativeEvent->ProcessMulticastDelegate<UObject>(ParamStorage);
					}

					DestroyCustomEventParams(FunctionPrototype, ParamStorage, FunctionPrototype->ParmsSize);
				}
			}
		}
	}
}


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

	for (TMap<FString, FMixerCustomGlobalEventStubDelegateWrapper>::TIterator It(CustomGlobalEventDelegates); It; ++It)
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
		FMessageLog("LoadErrors").Warning()
			->AddToken(FUObjectToken::Create(GetOutermost()))
			->AddToken(FTextToken::Create(NSLOCTEXT("MixerInteractivity", "ResaveNeeded_OutOfDateEvents", "References to out-of-date Mixer events found.  Please resave.")));
	}
#endif
}

void UMixerDelegateBinding::AddButtonBinding(const FMixerButtonEventBinding& BindingInfo)
{
	ButtonEventBindings.Add(BindingInfo);
}

void UMixerDelegateBinding::AddStickBinding(const FMixerStickEventBinding& BindingInfo)
{
	StickEventBindings.Add(BindingInfo);
}

void UMixerDelegateBinding::AddCustomGlobalEventBinding(const FMixerCustomGlobalEventBinding& BindingInfo)
{
	CustomGlobalEventBindings.Add(BindingInfo);
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

	for (const FMixerCustomGlobalEventBinding& CustomEventBinding : CustomGlobalEventBindings)
	{
		EventSource->AddCustomGlobalEventBinding(CustomEventBinding.EventName, InInstance, CustomEventBinding.TargetFunctionName);
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