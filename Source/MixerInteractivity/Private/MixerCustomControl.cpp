//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "MixerCustomControl.h"
#include "MixerDynamicDelegateBinding.h"
#include "Containers/Ticker.h"
#include "JsonObjectConverter.h"
#include "Engine/World.h"
#include "Engine/BlueprintGeneratedClass.h"

UMixerCustomControl::~UMixerCustomControl()
{
	uint8* CompactedPropertyLocation = LastSentPropertyData.GetData();
	for (UProperty* ClientProp : ClientWritableProperties)
	{
		ClientProp->DestroyValue(CompactedPropertyLocation);
		CompactedPropertyLocation += ClientProp->GetSize();
	}
}

void UMixerCustomControl::PostLoad()
{
	Super::PostLoad();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		InitClientWrittenPropertyMaintenance();
	}
}

void UMixerCustomControl::InitClientWrittenPropertyMaintenance()
{
	ClientWritableProperties.Empty();
	GetClientWritableProperties(ClientWritableProperties);

	int32 PropertyBlobRequiredSize = 0;
	for (UProperty* ClientProp : ClientWritableProperties)
	{
		PropertyBlobRequiredSize += ClientProp->GetSize();
	}

	LastSentPropertyData.AddUninitialized(PropertyBlobRequiredSize);
	uint8* CompactedPropertyLocation = LastSentPropertyData.GetData();
	for (UProperty* ClientProp : ClientWritableProperties)
	{
		void* SourcePropertyValue = ClientProp->ContainerPtrToValuePtr<void>(this);
		ClientProp->InitializeValue(CompactedPropertyLocation);
		ClientProp->CopyCompleteValue(CompactedPropertyLocation, SourcePropertyValue);
		CompactedPropertyLocation += ClientProp->GetSize();
	}

	if (ClientWritableProperties.Num() > 0)
	{
		FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMixerCustomControl::Tick), FMath::Max(ClientPropertyUpdateInterval, 0.0f));
	}
}

void UMixerCustomControl::GetClientWritableProperties(TArray<UProperty*>& OutProperties)
{
	for (UProperty* Prop = GetClass()->PropertyLink; Prop; Prop = Prop->PropertyLinkNext)
	{
		if ((Prop->HasAnyPropertyFlags(CPF_BlueprintVisible) && !Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly)))
		{
			ClientWritableProperties.Add(Prop);
		}
	}
}

bool UMixerCustomControl::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	check(World != nullptr);
	if (!World->IsGameWorld())
	{
		return false;
	}

	TSharedPtr<FJsonObject> ControlJson;
	uint8* CompactedPropertyLocation = LastSentPropertyData.GetData();
	for (UProperty* ClientProp : ClientWritableProperties)
	{
		void* SourcePropertyValue = ClientProp->ContainerPtrToValuePtr<void>(this);
		if (!ClientProp->Identical(SourcePropertyValue, CompactedPropertyLocation))
		{
			if (!ControlJson.IsValid())
			{
				ControlJson = MakeShared<FJsonObject>();
			}
			ControlJson->SetField(FJsonObjectConverter::StandardizeCase(ClientProp->GetName()), FJsonObjectConverter::UPropertyToJsonValue(ClientProp, SourcePropertyValue, 0, 0));
			ClientProp->CopyCompleteValue(CompactedPropertyLocation, SourcePropertyValue);
		}
		CompactedPropertyLocation += ClientProp->GetSize();
	}

	if (ControlJson.IsValid())
	{
		IMixerInteractivityModule::Get().UpdateRemoteControl(SceneName, ControlName, ControlJson.ToSharedRef());
	}

	return true;
}

void UMixerCustomControl::NativeOnServerPropertiesUpdated()
{
	OnServerPropertiesUpdated();
}