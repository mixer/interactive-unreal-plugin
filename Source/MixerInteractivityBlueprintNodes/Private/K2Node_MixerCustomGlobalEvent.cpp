//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "K2Node_MixerCustomGlobalEvent.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintEditorUtils.h"
#include "MixerInteractivityBlueprintLibrary.h"
#include "MixerDynamicDelegateBinding.h"
#include "MixerInteractivitySettings.h"
#include "KismetCompiler.h"
#include "JsonObject.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

void UK2Node_MixerCustomGlobalEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto CustomizeMixerNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FName EventName, UFunction* DelegateSignatureFunc)
	{
		UK2Node_MixerCustomGlobalEvent* MixerNode = CastChecked<UK2Node_MixerCustomGlobalEvent>(NewNode);
		MixerNode->EventName = EventName;
		MixerNode->CustomFunctionName = FName(*FString::Printf(TEXT("MixerCustomGlobalEvt_%s"), *EventName.ToString()));
		MixerNode->EventReference.SetExternalMember(DelegateSignatureFunc->GetFName(), DelegateSignatureFunc->GetOuterUClass());
	};

	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		const FSoftClassPath& CustomEventsPath = GetDefault<UMixerInteractivitySettings>()->CustomGlobalEvents;
		if (CustomEventsPath.IsValid())
		{
			UClass* CustomEventsDefinition = GetDefault<UMixerInteractivitySettings>()->CustomGlobalEvents.TryLoadClass<UObject>();
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

UClass* UK2Node_MixerCustomGlobalEvent::GetDynamicBindingClass() const
{
	return UMixerDelegateBinding::StaticClass();
}

FText UK2Node_MixerCustomGlobalEvent::GetMenuCategory() const
{
	return LOCTEXT("MixerCustomGlobalEventNode_MenuCategory", "{MixerInteractivity}|Custom Global Events");
}

void UK2Node_MixerCustomGlobalEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	FMixerCustomGlobalEventBinding BindingInfo;
	BindingInfo.TargetFunctionName = CustomFunctionName;
	BindingInfo.EventName = EventName;

	UMixerDelegateBinding* MixerBindingObject = CastChecked<UMixerDelegateBinding>(BindingObject);
	MixerBindingObject->AddCustomGlobalEventBinding(BindingInfo);
}

FText UK2Node_MixerCustomGlobalEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromName(EventName);
}

FText UK2Node_MixerCustomGlobalEvent::GetTooltipText() const
{
	return FText::FromName(EventName);
}

#undef LOCTEXT_NAMESPACE