//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "K2Node_MixerCustomMethod.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MixerInteractivityBlueprintLibrary.h"
#include "MixerDynamicDelegateBinding.h"
#include "MixerInteractivitySettings.h"
#include "KismetCompiler.h"
#include "Dom/JsonObject.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

void UK2Node_MixerCustomMethod::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto CustomizeMixerNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FName EventName, UFunction* DelegateSignatureFunc)
	{
		UK2Node_MixerCustomMethod* MixerNode = CastChecked<UK2Node_MixerCustomMethod>(NewNode);
		MixerNode->EventName = EventName;
		MixerNode->CustomFunctionName = FName(*FString::Printf(TEXT("MixerCustomGlobalEvt_%s"), *EventName.ToString()));
		MixerNode->EventReference.SetExternalMember(DelegateSignatureFunc->GetFName(), DelegateSignatureFunc->GetOuterUClass());
	};

	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		const FSoftClassPath& CustomEventsPath = GetDefault<UMixerInteractivitySettings>()->CustomMethods;
		if (CustomEventsPath.IsValid())
		{
			UClass* CustomEventsDefinition = GetDefault<UMixerInteractivitySettings>()->CustomMethods.TryLoadClass<UObject>();
			if (CustomEventsDefinition != nullptr)
			{
				for (UProperty* Prop = CustomEventsDefinition->PropertyLink; Prop; Prop = Prop->PropertyLinkNext)
				{
					UMulticastDelegateProperty* MulticastProp = Cast<UMulticastDelegateProperty>(Prop);
					if (MulticastProp != nullptr && MulticastProp->SignatureFunction != nullptr)
					{
						UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
						check(NodeSpawner != nullptr);
						NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeMixerNodeLambda, Prop->GetFName(), MulticastProp->SignatureFunction);
						ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
					}
				}
			}
		}
	}
}

UClass* UK2Node_MixerCustomMethod::GetDynamicBindingClass() const
{
	return UMixerDelegateBinding::StaticClass();
}

FText UK2Node_MixerCustomMethod::GetMenuCategory() const
{
	return LOCTEXT("MixerCustomMethodNode_MenuCategory", "{MixerInteractivity}|Custom Global Events");
}

void UK2Node_MixerCustomMethod::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	FMixerGenericEventBinding BindingInfo;
	BindingInfo.TargetFunctionName = CustomFunctionName;
	BindingInfo.NameParam = EventName;
	BindingInfo.BindingType = EMixerGenericEventBindingType::CustomMethod;

	UMixerDelegateBinding* MixerBindingObject = CastChecked<UMixerDelegateBinding>(BindingObject);
	MixerBindingObject->AddGenericBinding(BindingInfo);
}

FText UK2Node_MixerCustomMethod::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromName(EventName);
}

FText UK2Node_MixerCustomMethod::GetTooltipText() const
{
	return FText::FromName(EventName);
}

#undef LOCTEXT_NAMESPACE