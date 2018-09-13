//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "K2Node_MIxerGetCustomControlProperty.h"

#include "MixerInteractivityBlueprintLibrary.h"
#include "BlueprintNodeSpawner.h"
#include "KismetCompiler.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_CallFunction.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"


void UK2Node_MixerGetCustomControlProperty::AllocateDefaultPins()
{
#if ENGINE_MINOR_VERSION >= 19
	typedef FName FPinSubCategoryParamType;
#else
	typedef FString FPinSubCategoryParamType;
#endif

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* WorldContextPin = CreatePin(EGPD_Input, K2Schema->PC_Object, FPinSubCategoryParamType(), UObject::StaticClass(), TEXT("WorldContextObject"));
	WorldContextPin->bHidden = true;
	CreatePin(EGPD_Input, K2Schema->PC_Struct, FPinSubCategoryParamType(), FMixerCustomControlReference::StaticStruct(), TEXT("Control"));
	CreatePin(EGPD_Input, K2Schema->PC_Name, FPinSubCategoryParamType(), nullptr, TEXT("PropertyName"));
	CreatePin(EGPD_Output, K2Schema->PC_Wildcard, FPinSubCategoryParamType(), nullptr, K2Schema->PN_ReturnValue);

	Super::AllocateDefaultPins();
}

FText UK2Node_MixerGetCustomControlProperty::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("MixerGetCustomControlPropertyNode_Title", "Get Custom Control Property");
}

FText UK2Node_MixerGetCustomControlProperty::GetTooltipText() const
{
	return LOCTEXT("MixerGetCustomControlPropertyNode_Tooltip", "Access the value of the named property on a 'simple' Mixer custom control.");
}

void UK2Node_MixerGetCustomControlProperty::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* ResultPin = FindPinChecked(K2Schema->PN_ReturnValue);

	const FName FunctionName = GET_FUNCTION_NAME_CHECKED(UMixerInteractivityBlueprintLibrary, GetCustomControlProperty_Helper);
	UK2Node_CallFunction* GetPropertyHelperFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	GetPropertyHelperFunction->FunctionReference.SetExternalMember(FunctionName, UMixerInteractivityBlueprintLibrary::StaticClass());
	GetPropertyHelperFunction->AllocateDefaultPins();

	UEdGraphPin* FunctionReturnPin = GetPropertyHelperFunction->FindPinChecked(TEXT("OutProperty"), EGPD_Output);
	FunctionReturnPin->PinType = ResultPin->PinType;
	CompilerContext.MovePinLinksToIntermediate(*ResultPin, *FunctionReturnPin);

	UEdGraphPin* FunctionWorldContextPin = GetPropertyHelperFunction->FindPinChecked(TEXT("WorldContextObject"), EGPD_Input);
	UEdGraphPin* MyWorldContextPin = FindPinChecked(TEXT("WorldContextObject"), EGPD_Input);
	CompilerContext.MovePinLinksToIntermediate(*MyWorldContextPin, *FunctionWorldContextPin);

	UEdGraphPin* FunctionControlPin = GetPropertyHelperFunction->FindPinChecked(TEXT("Control"), EGPD_Input);
	UEdGraphPin* MyControlPin = FindPinChecked(TEXT("Control"), EGPD_Input);
	CompilerContext.MovePinLinksToIntermediate(*MyControlPin, *FunctionControlPin);

	UEdGraphPin* FunctionPropertyNamePin = GetPropertyHelperFunction->FindPinChecked(TEXT("PropertyName"), EGPD_Input);
	UEdGraphPin* MyPropertyNamePin = FindPinChecked(TEXT("PropertyName"), EGPD_Input);
	CompilerContext.MovePinLinksToIntermediate(*MyPropertyNamePin, *FunctionPropertyNamePin);

	BreakAllNodeLinks();
}

FSlateIcon UK2Node_MixerGetCustomControlProperty::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = GetNodeTitleColor();
	static FSlateIcon Icon("EditorStyle", "Kismet.AllClasses.FunctionIcon");
	return Icon;
}

void UK2Node_MixerGetCustomControlProperty::PostReconstructNode()
{
	Super::PostReconstructNode();

	RefreshOutputPinType();
}

void UK2Node_MixerGetCustomControlProperty::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (Pin == FindPinChecked(K2Schema->PN_ReturnValue))
	{
		RefreshOutputPinType();
	}
}

void UK2Node_MixerGetCustomControlProperty::RefreshOutputPinType()
{
	UScriptStruct* OutputStructType = nullptr;

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* ResultPin = FindPinChecked(K2Schema->PN_ReturnValue);
	check(ResultPin);
	if (ResultPin->LinkedTo.Num() > 0)
	{
		if (ResultPin->LinkedTo[0]->PinType.PinCategory != K2Schema->PC_Wildcard)
		{
			ResultPin->PinType = ResultPin->LinkedTo[0]->PinType;
		}
		else
		{
			ResultPin->PinType.PinCategory = K2Schema->PC_String;
			ResultPin->PinType.PinSubCategory = decltype(ResultPin->PinType.PinSubCategory)();
			ResultPin->PinType.PinSubCategoryObject = nullptr;
		}
		ResultPin->PinType.bIsReference = false;
	}
	else
	{
		ResultPin->PinType.PinCategory = K2Schema->PC_Wildcard;
		ResultPin->PinType.PinSubCategory = decltype(ResultPin->PinType.PinSubCategory)();
		ResultPin->PinType.PinSubCategoryObject = nullptr;
	}
}

void UK2Node_MixerGetCustomControlProperty::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_MixerGetCustomControlProperty::GetMenuCategory() const
{
	return LOCTEXT("MixerGetCustomControlPropertyNode_MenuCategory", "{MixerInteractivity}|Custom Controls");
}

bool UK2Node_MixerGetCustomControlProperty::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	bool bDisallowed = Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
	if (!bDisallowed)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if (MyPin == FindPinChecked(K2Schema->PN_ReturnValue))
		{
			const auto& OtherPinCategory = OtherPin->PinType.PinCategory;
			bDisallowed = OtherPinCategory != K2Schema->PC_Boolean &&
				OtherPinCategory != K2Schema->PC_Byte &&
				OtherPinCategory != K2Schema->PC_Float &&
				OtherPinCategory != K2Schema->PC_Int &&
				OtherPinCategory != K2Schema->PC_Name &&
				OtherPinCategory != K2Schema->PC_String &&
				OtherPinCategory != K2Schema->PC_Struct &&
				OtherPinCategory != K2Schema->PC_Text &&
				OtherPinCategory != K2Schema->PC_Wildcard;

			if (bDisallowed)
			{
				OutReason = LOCTEXT("MixerGetCustomControlPropertyNode_InvalidOutputType", "Properties can only be accessed as integral, floating point, string or USTRUCT types.").ToString();
			}
		}
	}

	return bDisallowed;
}

#undef LOCTEXT_NAMESPACE