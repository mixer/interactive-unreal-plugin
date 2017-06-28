//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "K2Node_MixerButton.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintEditorUtils.h"
#include "GraphEditorSettings.h"
#include "KismetCompiler.h"
#include "K2Node_TemporaryVariable.h"
#include "K2Node_AssignmentStatement.h"
#include "MixerInteractivitySettings.h"
#include "K2Node_MixerButtonEvent.h"
#include "MixerInteractivityBlueprintLibrary.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

void UK2Node_MixerButton::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	check(Settings);
	if (!Settings->CachedButtons.Contains(ButtonId))
	{
		MessageLog.Warning(*FText::Format(LOCTEXT("MixerButtonNode_UnknownButtonWarning", "Mixer Button Event specifies invalid button id '{0}' for @@"), FText::FromName(ButtonId)).ToString(), this);
	}
}

void UK2Node_MixerButton::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UEdGraphPin* PressedPin = FindPin(TEXT("Pressed"));
	UEdGraphPin* ReleasedPin = FindPin(TEXT("Released"));

	bool PressedPinIsActive = PressedPin != nullptr && PressedPin->LinkedTo.Num() > 0;
	bool ReleasedPinIsActive = ReleasedPin != nullptr && ReleasedPin->LinkedTo.Num() > 0;
	UK2Node_TemporaryVariable* IntermediateButtonNode = nullptr;
	UK2Node_TemporaryVariable* IntermediateParticipantNode = nullptr;

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

	if (PressedPinIsActive && ReleasedPinIsActive)
	{
		IntermediateButtonNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
		IntermediateButtonNode->VariableType.PinCategory = Schema->PC_Struct;
		IntermediateButtonNode->VariableType.PinSubCategoryObject = FMixerButtonReference::StaticStruct();
		IntermediateButtonNode->AllocateDefaultPins();

		IntermediateParticipantNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
		IntermediateParticipantNode->VariableType.PinCategory = Schema->PC_Int;
		IntermediateParticipantNode->AllocateDefaultPins();
	}

	auto ExpandEventPin = [&](UEdGraphPin* OriginalPin, bool IsPressedEvent)
	{
		UK2Node_MixerButtonEvent* ButtonEvent = CompilerContext.SpawnIntermediateEventNode<UK2Node_MixerButtonEvent>(this, OriginalPin, SourceGraph);
		ButtonEvent->ButtonId = ButtonId;
		ButtonEvent->Pressed = IsPressedEvent;
		ButtonEvent->CustomFunctionName = FName(*FString::Printf(TEXT("MixerButtonEvt_%s_%s"), *ButtonId.ToString(), IsPressedEvent ? TEXT("Pressed") : TEXT("Released")));
		ButtonEvent->EventReference.SetExternalDelegateMember(FName(TEXT("MixerButtonEventDynamicDelegate__DelegateSignature")));
		ButtonEvent->bInternalEvent = true;
		ButtonEvent->AllocateDefaultPins();

		if (IntermediateButtonNode)
		{
			check(IntermediateParticipantNode);

			UK2Node_AssignmentStatement* ButtonAssignment = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
			ButtonAssignment->AllocateDefaultPins();

			Schema->TryCreateConnection(IntermediateButtonNode->GetVariablePin(), ButtonAssignment->GetVariablePin());
			Schema->TryCreateConnection(ButtonAssignment->GetValuePin(), ButtonEvent->FindPinChecked(TEXT("Button")));
			CompilerContext.MovePinLinksToIntermediate(*FindPin(TEXT("Button")), *IntermediateButtonNode->GetVariablePin());

			CompilerContext.MovePinLinksToIntermediate(*OriginalPin, *ButtonAssignment->GetThenPin());

			UK2Node_AssignmentStatement* ParticipantAssignment = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
			ParticipantAssignment->AllocateDefaultPins();

			Schema->TryCreateConnection(IntermediateParticipantNode->GetVariablePin(), ParticipantAssignment->GetVariablePin());
			Schema->TryCreateConnection(ParticipantAssignment->GetValuePin(), ButtonEvent->FindPinChecked(TEXT("ParticipantId")));
			CompilerContext.MovePinLinksToIntermediate(*FindPin(TEXT("ParticipantId")), *IntermediateParticipantNode->GetVariablePin());

			Schema->TryCreateConnection(ParticipantAssignment->GetThenPin(), ButtonAssignment->GetExecPin());
			Schema->TryCreateConnection(Schema->FindExecutionPin(*ButtonEvent, EGPD_Output), ParticipantAssignment->GetExecPin());
		}
		else
		{
			CompilerContext.MovePinLinksToIntermediate(*OriginalPin, *Schema->FindExecutionPin(*ButtonEvent, EGPD_Output));
			CompilerContext.MovePinLinksToIntermediate(*FindPin(TEXT("Button")), *ButtonEvent->FindPin(TEXT("Button")));
			CompilerContext.MovePinLinksToIntermediate(*FindPin(TEXT("ParticipantId")), *ButtonEvent->FindPin(TEXT("ParticipantId")));
		}
	};

	if (PressedPinIsActive)
	{
		ExpandEventPin(PressedPin, true);
	}

	if (ReleasedPinIsActive)
	{
		ExpandEventPin(ReleasedPin, false);
	}
}

void UK2Node_MixerButton::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto CustomizeMixerNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FName ButtonName)
	{
		UK2Node_MixerButton* MixerNode = CastChecked<UK2Node_MixerButton>(NewNode);
		MixerNode->ButtonId = ButtonName;
	};

	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		for (FName CachedButtonName : Settings->CachedButtons)
		{
			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			check(NodeSpawner != nullptr);

			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeMixerNodeLambda, CachedButtonName);
			ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
		}
	}
}

FText UK2Node_MixerButton::GetMenuCategory() const
{
	return LOCTEXT("MixerButtonNode_MenuCategory", "{MixerInteractivity}|Button Events");
}

FBlueprintNodeSignature UK2Node_MixerButton::GetSignature() const
{
	FBlueprintNodeSignature NodeSignature = Super::GetSignature();
	NodeSignature.AddKeyValue(ButtonId.ToString());

	return NodeSignature;
}

void UK2Node_MixerButton::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, TEXT("Pressed"));
	CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, TEXT("Released"));
	CreatePin(EGPD_Output, K2Schema->PC_Struct, TEXT(""), FMixerButtonReference::StaticStruct(), false, false, TEXT("Button"));
	CreatePin(EGPD_Output, K2Schema->PC_Int, TEXT(""), NULL, false, false, TEXT("ParticipantId"));

	Super::AllocateDefaultPins();
}

FLinearColor UK2Node_MixerButton::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->EventNodeTitleColor;
}

FText UK2Node_MixerButton::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedNodeTitle.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("MixerButtonNode_Title", "{0} (Mixer button)"), FText::FromName(ButtonId)), this);
	}
	return CachedNodeTitle;
}

FText UK2Node_MixerButton::GetTooltipText() const
{
	if (CachedTooltip.IsOutOfDate(this))
	{
		CachedTooltip.SetCachedText(FText::Format(LOCTEXT("MixerButtonNode_Tooltip", "Events for when the {0} button is pressed or released on Mixer."), FText::FromName(ButtonId)), this);
	}
	return CachedTooltip;
}

FSlateIcon UK2Node_MixerButton::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon("EditorStyle", "GraphEditor.PadEvent_16x");
}

bool UK2Node_MixerButton::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	bool bIsCompatible = Super::IsCompatibleWithGraph(TargetGraph);
	if (bIsCompatible)
	{
		EGraphType const GraphType = TargetGraph->GetSchema()->GetGraphType(TargetGraph);
		bIsCompatible = (GraphType == EGraphType::GT_Ubergraph);
	}

	return bIsCompatible;
}
