//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "K2Node_MixerSimpleCustomControlUpdate.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MixerDynamicDelegateBinding.h"
#include "MixerInteractivitySettings.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

void UK2Node_MixerSimpleCustomControlUpdate::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto CustomizeMixerNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FString InControlName)
	{
		UK2Node_MixerSimpleCustomControlUpdate* MixerNode = CastChecked<UK2Node_MixerSimpleCustomControlUpdate>(NewNode);
		MixerNode->ControlId = *InControlName;
		MixerNode->CustomFunctionName = FName(*FString::Printf(TEXT("MixerCustomControlUpdateEvt_%s"), *InControlName));
		MixerNode->EventReference.SetExternalDelegateMember(FName(TEXT("MixerCustomControlUpdateDynamicDelegate__DelegateSignature")));
	};

	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		TArray<FString> CustomControls;
		UMixerInteractivitySettings::GetAllUnmappedCustomControls(CustomControls);
		for (const FString& CustomControlName : CustomControls)
		{
			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeMixerNodeLambda, CustomControlName);
			ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
		}
	}
}

FText UK2Node_MixerSimpleCustomControlUpdate::GetMenuCategory() const
{
	return FText::FromString("{MixerInteractivity}|Custom Controls");
}

FText UK2Node_MixerSimpleCustomControlUpdate::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedNodeTitle.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("MixerSimpleCustomControlUpdateNode_Title", "{0} updated (Mixer custom control)"), FText::FromName(ControlId)), this);
	}
	return CachedNodeTitle;
}

FText UK2Node_MixerSimpleCustomControlUpdate::GetTooltipText() const
{
	if (CachedTooltip.IsOutOfDate(this))
	{
		CachedTooltip.SetCachedText(FText::Format(LOCTEXT("MixerSimpleCustomControlUpdateNode_Tooltip", "Events for when the {0} custom control receives property updates from Mixer."), FText::FromName(ControlId)), this);
	}
	return CachedTooltip;
}

UClass* UK2Node_MixerSimpleCustomControlUpdate::GetDynamicBindingClass() const
{
	return UMixerDelegateBinding::StaticClass();
}

void UK2Node_MixerSimpleCustomControlUpdate::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UMixerDelegateBinding* MixerBindingObject = CastChecked<UMixerDelegateBinding>(BindingObject);

	FMixerCustomControlEventBinding BindingInfo;
	BindingInfo.TargetFunctionName = CustomFunctionName;
	BindingInfo.ControlId = ControlId;
	MixerBindingObject->AddCustomControlUpdateBinding(BindingInfo);
}

#undef LOCTEXT_NAMESPACE 