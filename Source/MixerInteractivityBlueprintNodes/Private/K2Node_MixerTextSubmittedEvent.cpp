//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "K2Node_MixerTextSubmittedEvent.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "GraphEditorSettings.h"
#include "KismetCompiler.h"
#include "MixerDynamicDelegateBinding.h"
#include "MixerInteractivitySettings.h"
#include "MixerInteractivityJsonTypes.h"
#include "MixerInteractivityProjectAsset.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

void UK2Node_MixerTextSubmittedEvent::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	TArray<FString> Textboxes;
	UMixerInteractivitySettings::GetAllControls(FMixerInteractiveControl::TextboxKind, Textboxes);
	if (!Textboxes.Contains(TextboxId.ToString()))
	{
		MessageLog.Warning(*FText::Format(LOCTEXT("MixerTextSubmittedNode_UnknownTextboxWarning", "Mixer Text Submitted Event specifies invalid textbox id '{0}' for @@"), FText::FromName(TextboxId)).ToString(), this);
	}
}

void UK2Node_MixerTextSubmittedEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto CustomizeMixerNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FString TextboxName)
	{
		UK2Node_MixerTextSubmittedEvent* MixerNode = CastChecked<UK2Node_MixerTextSubmittedEvent>(NewNode);
		MixerNode->TextboxId = *TextboxName;
		MixerNode->CustomFunctionName = FName(*FString::Printf(TEXT("MixerTextSubmittedEvt_%s"), *TextboxName));
		MixerNode->EventReference.SetExternalDelegateMember(FName(TEXT("MixerTextSubmittedEventDynamicDelegate__DelegateSignature")));
	};

	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		TArray<FString> Textboxes;
		UMixerInteractivitySettings::GetAllControls(FMixerInteractiveControl::TextboxKind, Textboxes);
		for (const FString& TextboxName : Textboxes)
		{
			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeMixerNodeLambda, TextboxName);
			ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
		}
	}
}

FText UK2Node_MixerTextSubmittedEvent::GetMenuCategory() const
{
	return LOCTEXT("MixerTextSubmittedNode_MenuCategory", "{MixerInteractivity}|Textbox Events");
}

FText UK2Node_MixerTextSubmittedEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedNodeTitle.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("MixerTextSubmittedNode_Title", "{0} text submitted (Mixer textbox)"), FText::FromName(TextboxId)), this);
	}
	return CachedNodeTitle;
}

FText UK2Node_MixerTextSubmittedEvent::GetTooltipText() const
{
	if (CachedTooltip.IsOutOfDate(this))
	{
		CachedTooltip.SetCachedText(FText::Format(LOCTEXT("MixerTextSubmittedNode_Tooltip", "Events for when text is submitted via the {0} textbox on Mixer."), FText::FromName(TextboxId)), this);
	}
	return CachedTooltip;
}

FSlateIcon UK2Node_MixerTextSubmittedEvent::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon("EditorStyle", "GraphEditor.PadEvent_16x");
}

UClass* UK2Node_MixerTextSubmittedEvent::GetDynamicBindingClass() const
{
	return UMixerDelegateBinding::StaticClass();
}

void UK2Node_MixerTextSubmittedEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	FMixerGenericEventBinding BindingInfo;
	BindingInfo.TargetFunctionName = CustomFunctionName;
	BindingInfo.NameParam = TextboxId;
	BindingInfo.BindingType = EMixerGenericEventBindingType::TextSubmitted;

	UMixerDelegateBinding* MixerBindingObject = CastChecked<UMixerDelegateBinding>(BindingObject);
	MixerBindingObject->AddGenericBinding(BindingInfo);
}

#undef LOCTEXT_NAMESPACE
