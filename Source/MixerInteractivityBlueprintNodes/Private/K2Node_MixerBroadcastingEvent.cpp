//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "K2Node_MixerBroadcastingEvent.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MixerInteractivityBlueprintLibrary.h"
#include "MixerDynamicDelegateBinding.h"


#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

void UK2Node_MixerBroadcastingEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto CustomizeMixerNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, bool ForBroadcastingState)
	{
		UK2Node_MixerBroadcastingEvent* MixerNode = CastChecked<UK2Node_MixerBroadcastingEvent>(NewNode);
		MixerNode->ForBroadcastingState = ForBroadcastingState;
		MixerNode->CustomFunctionName = FName(*FString::Printf(TEXT("MixerBroadcastingEvt_%s"), ForBroadcastingState ? TEXT("Started") : TEXT("Stopped")));
		MixerNode->EventReference.SetExternalDelegateMember(FName(TEXT("MixerBroadcastingEventDynamicDelegate__DelegateSignature")));
	};

	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeMixerNodeLambda, true);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);

		NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeMixerNodeLambda, false);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_MixerBroadcastingEvent::GetMenuCategory() const
{
	return FText::FromString("{MixerInteractivity}");
}

FText UK2Node_MixerBroadcastingEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return ForBroadcastingState ?
		LOCTEXT("MixerBroadcastingStartedNode_Title", "Broadcasting Started (Mixer)") :
		LOCTEXT("MixerBroadcastingStoppedNode_Title", "Broadcasting Stopped (Mixer)");
}

FText UK2Node_MixerBroadcastingEvent::GetTooltipText() const
{
	return ForBroadcastingState ?
		LOCTEXT("MixerBroadcastingStartedNode_Tooltip", "Event for when the local user begins broadcasting video content on the Mixer service.") :
		LOCTEXT("MixerBroadcastingStoppedNode_Tooltip", "Event for when the local user stops broadcasting video content on the Mixer service.");
}

UClass* UK2Node_MixerBroadcastingEvent::GetDynamicBindingClass() const
{
	return UMixerDelegateBinding::StaticClass();
}

void UK2Node_MixerBroadcastingEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UMixerDelegateBinding* MixerBindingObject = CastChecked<UMixerDelegateBinding>(BindingObject);
	if (ForBroadcastingState)
	{
		MixerBindingObject->BroadcastingStartedBinding = CustomFunctionName;
	}
	else
	{
		MixerBindingObject->BroadcastingStoppedBinding = CustomFunctionName;
	}
}

#undef LOCTEXT_NAMESPACE 