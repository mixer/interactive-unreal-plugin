#pragma once

#include "MixerProjectDefinitionCustomization.h"
#include "MixerInteractivityEditorModule.h"
#include "MixerInteractivityProjectAsset.h"
#include "MixerInteractivityJsonTypes.h"
#include "MixerCustomControl.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "PropertyCustomizationHelpers.h"
#include "STextBlock.h"
#include "HorizontalBox.h"
#include "K2Node_MixerSimpleCustomControlInput.h"
#include "K2Node_MixerSimpleCustomControlUpdate.h"
#include "BlueprintActionDatabase.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

TSharedRef<IDetailCustomization> FMixerProjectDefinitionCustomization::MakeInstance()
{
	return MakeShared<FMixerProjectDefinitionCustomization>();
}

void FMixerProjectDefinitionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	UMixerProjectAsset* ProjectAsset = Cast<UMixerProjectAsset>(ObjectsBeingCustomized[0].Get());
	if (ProjectAsset == nullptr)
	{
		return;
	}

	IDetailCategoryBuilder& ControlsCategoryBuilder = DetailBuilder.EditCategory("Controls");

	for (const FMixerInteractiveScene& Scene : ProjectAsset->ParsedProjectDefinition.Controls.Scenes)
	{
		IDetailGroup& SingleSceneGroup = ControlsCategoryBuilder.AddGroup(*Scene.Id, FText::FromString(Scene.Id), true);
		SingleSceneGroup.HeaderRow()
		.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(FText::FromString(*Scene.Id))
			]
		.ValueContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(FText::FromString(TEXT("Scene")))
			];


		for (const FMixerInteractiveControl& Control : Scene.Controls)
		{
			TSharedPtr<SHorizontalBox> ValueBox;

			FString PrettyControlKind = Control.Kind;
			if (PrettyControlKind.Len() > 0)
			{
				PrettyControlKind[0] = FChar::ToUpper(PrettyControlKind[0]);
			}

			FDetailWidgetRow& WidgetRow = SingleSceneGroup.AddWidgetRow()
				.NameContent()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(FText::FromString(*Control.Id))
				]
			.ValueContent()
				[
					SAssignNew(ValueBox, SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2.0f, 2.0f)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(FText::FromString(PrettyControlKind))
					]
				];

			if (!Control.IsButton() && !Control.IsJoystick())
			{
				FName ControlName = FName(*Control.Id);

				ValueBox->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					.Padding(2.0f, 2.0f)
					[
						SNew(SClassPropertyEntryBox)
						.MetaClass(UMixerCustomControl::StaticClass())
					.AllowAbstract(false)
					.SelectedClass(this, &FMixerProjectDefinitionCustomization::GetClassForCustomControl, ProjectAsset, ControlName)
					.OnSetClass(this, &FMixerProjectDefinitionCustomization::SetClassForCustomControl, ProjectAsset, ControlName)
					];
			}
		}
	}

}

const UClass* FMixerProjectDefinitionCustomization::GetClassForCustomControl(UMixerProjectAsset* ProjectDefinition, FName ControlName) const
{
	FSoftClassPath* CurrentClass = ProjectDefinition->CustomControlBindings.Find(ControlName);
	return CurrentClass != nullptr ? CurrentClass->TryLoadClass<UObject>() : nullptr;
}

void FMixerProjectDefinitionCustomization::SetClassForCustomControl(const UClass* NewClass, UMixerProjectAsset* ProjectDefinition, FName ControlName) const
{
	if (NewClass != nullptr)
	{
		ProjectDefinition->CustomControlBindings.FindOrAdd(ControlName) = FSoftClassPath(NewClass);
	}
	else
	{
		ProjectDefinition->CustomControlBindings.Remove(ControlName);
	}

	IMixerInteractivityEditorModule::Get().RefreshDesignTimeObjects();

	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerSimpleCustomControlInput::StaticClass());
	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerSimpleCustomControlUpdate::StaticClass());
}


#undef LOCTEXT_NAMESPACE
