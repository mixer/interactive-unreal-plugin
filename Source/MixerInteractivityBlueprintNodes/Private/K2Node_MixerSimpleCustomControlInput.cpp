//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "K2Node_MixerSimpleCustomControlInput.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintEditorUtils.h"
#include "MixerDynamicDelegateBinding.h"
#include "MixerInteractivitySettings.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

void UK2Node_MixerSimpleCustomControlInput::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto CustomizeMixerNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FName InControlName)
	{
		UK2Node_MixerSimpleCustomControlInput* MixerNode = CastChecked<UK2Node_MixerSimpleCustomControlInput>(NewNode);
		MixerNode->ControlId = InControlName;
		MixerNode->CustomFunctionName = FName(*FString::Printf(TEXT("MixerCustomControlInputEvt_%s"), *InControlName.ToString()));
		MixerNode->EventReference.SetExternalDelegateMember(FName(TEXT("MixerCustomControlInputDynamicDelegate__DelegateSignature")));
	};

	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		for (TMap<FName, TSubclassOf<UMixerCustomControl>>::TConstIterator It(Settings->CachedCustomControls); It; ++It)
		{
			if (It->Value.Get() == nullptr)
			{
				UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
				check(NodeSpawner != nullptr);
				NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeMixerNodeLambda, It->Key);
				ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
			}
		}
	}
}

FText UK2Node_MixerSimpleCustomControlInput::GetMenuCategory() const
{
	return FText::FromString("{MixerInteractivity}");
}

FText UK2Node_MixerSimpleCustomControlInput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedNodeTitle.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("MixerSimpleCustomControlInputNode_Title", "{0} input (Mixer custom control)"), FText::FromName(ControlId)), this);
	}
	return CachedNodeTitle;
}

FText UK2Node_MixerSimpleCustomControlInput::GetTooltipText() const
{
	if (CachedTooltip.IsOutOfDate(this))
	{
		CachedTooltip.SetCachedText(FText::Format(LOCTEXT("MixerButtonNode_Tooltip", "Events for when the {0} custom control receives input on Mixer."), FText::FromName(ControlId)), this);
	}
	return CachedTooltip;
}

UClass* UK2Node_MixerSimpleCustomControlInput::GetDynamicBindingClass() const
{
	return UMixerDelegateBinding::StaticClass();
}

void UK2Node_MixerSimpleCustomControlInput::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UMixerDelegateBinding* MixerBindingObject = CastChecked<UMixerDelegateBinding>(BindingObject);

	FMixerCustomControlEventBinding BindingInfo;
	BindingInfo.TargetFunctionName = CustomFunctionName;
	BindingInfo.ControlId = ControlId;
	MixerBindingObject->AddCustomControlInputBinding(BindingInfo);
}

#undef LOCTEXT_NAMESPACE 
