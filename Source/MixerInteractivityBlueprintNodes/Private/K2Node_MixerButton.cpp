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
#include "Kismet2/BlueprintEditorUtils.h"
#include "GraphEditorSettings.h"
#include "KismetCompiler.h"
#include "K2Node_TemporaryVariable.h"
#include "K2Node_AssignmentStatement.h"
#include "MixerInteractivitySettings.h"
#include "MixerInteractivityJsonTypes.h"
#include "K2Node_MixerButtonEvent.h"
#include "MixerInteractivityBlueprintLibrary.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

void UK2Node_MixerButton::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	TArray<FString> Buttons;
	UMixerInteractivitySettings::GetAllControls(FMixerInteractiveControl::ButtonKind, Buttons);
	if (!Buttons.Contains(ButtonId.ToString()))
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
	UK2Node_TemporaryVariable* IntermediateTransactionNode = nullptr;
	UK2Node_TemporaryVariable* IntermediateCostNode = nullptr;

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

		IntermediateTransactionNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
		IntermediateTransactionNode->VariableType.PinCategory = Schema->PC_Struct;
		IntermediateTransactionNode->VariableType.PinSubCategoryObject = FMixerTransactionId::StaticStruct();
		IntermediateTransactionNode->AllocateDefaultPins();

		IntermediateCostNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
		IntermediateCostNode->VariableType.PinCategory = Schema->PC_Int;
		IntermediateCostNode->AllocateDefaultPins();
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


			auto MoveNodeToIntermediate = [&](const FString& OriginalPinName, UK2Node_TemporaryVariable* IntermediatePin)
			{
				UK2Node_AssignmentStatement* AssignmentNode = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
				AssignmentNode->AllocateDefaultPins();

				Schema->TryCreateConnection(IntermediatePin->GetVariablePin(), AssignmentNode->GetVariablePin());
				Schema->TryCreateConnection(AssignmentNode->GetValuePin(), ButtonEvent->FindPinChecked(OriginalPinName));
				CompilerContext.MovePinLinksToIntermediate(*FindPin(OriginalPinName), *IntermediatePin->GetVariablePin());
				return AssignmentNode;
			};

			UK2Node_AssignmentStatement* ButtonAssignment = MoveNodeToIntermediate(TEXT("Button"), IntermediateButtonNode);
			UK2Node_AssignmentStatement* ParticipantAssignment = MoveNodeToIntermediate(TEXT("ParticipantId"), IntermediateParticipantNode);
			UK2Node_AssignmentStatement* TransactionAssignment = MoveNodeToIntermediate(TEXT("TransactionId"), IntermediateTransactionNode);
			UK2Node_AssignmentStatement* CostAssignment = MoveNodeToIntermediate(TEXT("SparkCost"), IntermediateCostNode);

			CompilerContext.MovePinLinksToIntermediate(*OriginalPin, *ButtonAssignment->GetThenPin());
			Schema->TryCreateConnection(ParticipantAssignment->GetThenPin(), ButtonAssignment->GetExecPin());
			Schema->TryCreateConnection(TransactionAssignment->GetThenPin(), ParticipantAssignment->GetExecPin());
			Schema->TryCreateConnection(CostAssignment->GetThenPin(), TransactionAssignment->GetExecPin());

			Schema->TryCreateConnection(Schema->FindExecutionPin(*ButtonEvent, EGPD_Output), CostAssignment->GetExecPin());
		}
		else
		{
			CompilerContext.MovePinLinksToIntermediate(*OriginalPin, *Schema->FindExecutionPin(*ButtonEvent, EGPD_Output));
			CompilerContext.MovePinLinksToIntermediate(*FindPin(TEXT("Button")), *ButtonEvent->FindPin(TEXT("Button")));
			CompilerContext.MovePinLinksToIntermediate(*FindPin(TEXT("ParticipantId")), *ButtonEvent->FindPin(TEXT("ParticipantId")));
			CompilerContext.MovePinLinksToIntermediate(*FindPin(TEXT("TransactionId")), *ButtonEvent->FindPin(TEXT("TransactionId")));
			CompilerContext.MovePinLinksToIntermediate(*FindPin(TEXT("SparkCost")), *ButtonEvent->FindPin(TEXT("SparkCost")));
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
		TArray<FString> Buttons;
		UMixerInteractivitySettings::GetAllControls(FMixerInteractiveControl::ButtonKind, Buttons);
		for (const FString& ButtonName : Buttons)
		{
			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeMixerNodeLambda, FName(*ButtonName));
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
#if ENGINE_MINOR_VERSION >= 19
	typedef FName FPinSubCategoryParamType;
#else
	typedef FString FPinSubCategoryParamType;
#endif

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Output, K2Schema->PC_Exec, FPinSubCategoryParamType(), nullptr, TEXT("Pressed"));
	CreatePin(EGPD_Output, K2Schema->PC_Exec, FPinSubCategoryParamType(), nullptr, TEXT("Released"));
	CreatePin(EGPD_Output, K2Schema->PC_Struct, FPinSubCategoryParamType(), FMixerButtonReference::StaticStruct(), TEXT("Button"));
	CreatePin(EGPD_Output, K2Schema->PC_Int, FPinSubCategoryParamType(), nullptr, TEXT("ParticipantId"));

	// Advanced pins
	UEdGraphPin* AdvancedPin = CreatePin(EGPD_Output, K2Schema->PC_Struct, FPinSubCategoryParamType(), FMixerTransactionId::StaticStruct(), TEXT("TransactionId"));
	AdvancedPin->bAdvancedView = 1;
	AdvancedPin = CreatePin(EGPD_Output, K2Schema->PC_Int, FPinSubCategoryParamType(), nullptr, TEXT("SparkCost"));
	AdvancedPin->bAdvancedView = 1;

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
