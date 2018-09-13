//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "K2Node_MixerParticipantEvent.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MixerInteractivityBlueprintLibrary.h"
#include "MixerDynamicDelegateBinding.h"


#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

void UK2Node_MixerParticipantEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto CustomizeMixerNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, EMixerInteractivityParticipantState ForState)
	{
		UK2Node_MixerParticipantEvent* MixerNode = CastChecked<UK2Node_MixerParticipantEvent>(NewNode);
		MixerNode->ForParticipantState = ForState;
		MixerNode->CustomFunctionName = FName(*FString::Printf(TEXT("MixerParticipantEvt_State_%d"), static_cast<uint8>(ForState)));
		MixerNode->EventReference.SetExternalDelegateMember(FName(TEXT("MixerParticipantEventDynamicDelegate__DelegateSignature")));
	};

	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		auto AddSpawnerForState = [&](EMixerInteractivityParticipantState ForState)
		{
			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			check(NodeSpawner != nullptr);
			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeMixerNodeLambda, ForState);
			ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
		};

		AddSpawnerForState(EMixerInteractivityParticipantState::Joined);
		AddSpawnerForState(EMixerInteractivityParticipantState::Left);
		AddSpawnerForState(EMixerInteractivityParticipantState::Input_Disabled);
	}
}

FText UK2Node_MixerParticipantEvent::GetMenuCategory() const
{
	return FText::FromString("{MixerInteractivity}");
}

FText UK2Node_MixerParticipantEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const 
{
	switch (ForParticipantState)
	{
	case EMixerInteractivityParticipantState::Joined:
		return LOCTEXT("MixerParticipantJoinedNode_Title", "Participant Joined (Mixer)");
	case EMixerInteractivityParticipantState::Left:
		return LOCTEXT("MixerParticipantLeftNode_Title", "Participant Left (Mixer)");
	case EMixerInteractivityParticipantState::Input_Disabled:
		return LOCTEXT("MixerParticipantInputDisabledNode_Title", "Participant Input Disabled (Mixer)");
	default:
		return FText::GetEmpty();
	}
}

FText UK2Node_MixerParticipantEvent::GetTooltipText() const
{
	switch (ForParticipantState)
	{
	case EMixerInteractivityParticipantState::Joined:
		return LOCTEXT("MixerParticipantJoinedNode_Tooltip", "Event for when a new remote participant joins an interactive session on the Mixer service.");
	case EMixerInteractivityParticipantState::Left:
		return LOCTEXT("MixerParticipantLeftNode_Tooltip", "Event for when a remote participant leaves an interactive session on the Mixer service.");
	case EMixerInteractivityParticipantState::Input_Disabled:
		return LOCTEXT("MixerParticipantInputDisabledNode_Tooltip", "Event for when a remote participant in an interactive session on the Mixer service has their interactive input capability disabled.");
	default:
		return FText::GetEmpty();
	}
}

UClass* UK2Node_MixerParticipantEvent::GetDynamicBindingClass() const
{
	return UMixerDelegateBinding::StaticClass();
}

void UK2Node_MixerParticipantEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UMixerDelegateBinding* MixerBindingObject = CastChecked<UMixerDelegateBinding>(BindingObject);
	switch (ForParticipantState)
	{
	case EMixerInteractivityParticipantState::Joined:
		MixerBindingObject->ParticipantJoinedBinding = CustomFunctionName;
		break;
	case EMixerInteractivityParticipantState::Left:
		MixerBindingObject->ParticipantLeftBinding = CustomFunctionName;
		break;
	case EMixerInteractivityParticipantState::Input_Disabled:
		MixerBindingObject->ParticipantInputDisabledBinding = CustomFunctionName;
		break;
	}
}

#undef LOCTEXT_NAMESPACE 