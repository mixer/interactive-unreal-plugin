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
#include "SRichTextBlock.h"
#include "SBorder.h"
#include "HorizontalBox.h"
#include "K2Node_MixerSimpleCustomControlInput.h"
#include "K2Node_MixerSimpleCustomControlUpdate.h"
#include "BlueprintActionDatabase.h"
#include "KismetEditorUtilities.h"
#include "AssetEditorManager.h"
#include "Editor.h"

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

	TSharedRef<IPropertyHandle> ControlBindingProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerProjectAsset, CustomControlBindings));
	DetailBuilder.HideProperty(ControlBindingProperty);

	IDetailCategoryBuilder& ProjectInfoCategoryBuilder = DetailBuilder.EditCategory("Project Info");

	ProjectInfoCategoryBuilder.AddCustomRow(LOCTEXT("ProjectDefinition_GameName", "Game Name"))
	.WholeRowWidget
	[
		SNew(STextBlock)
		.Text(FText::FromString(ProjectAsset->ParsedProjectDefinition.Game.Name))
		.Font(IDetailLayoutBuilder::GetDetailFontBold())
	];

	ProjectInfoCategoryBuilder.AddCustomRow(LOCTEXT("ProjectDefinition_VersionName", "Version Name"))
	.WholeRowWidget
	[
		SNew(STextBlock)
		.Text(FText::FromString(ProjectAsset->ParsedProjectDefinition.Name))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	ProjectInfoCategoryBuilder.AddCustomRow(LOCTEXT("ProjectDefinition_EditLink", "Edit Link"))
	.WholeRowWidget
	[
		SNew(SRichTextBlock)
		.Text(FText::Format(
			LOCTEXT("ProjectDefinitionHeader_EditLinkFormat", "<a id=\"browser\" href=\"https://mixer.com/lab/interactive/tool/{0}/{1}/editor/build\">Edit this project at mixer.com</>"),
			FText::AsNumber(ProjectAsset->ParsedProjectDefinition.Game.Id, &FNumberFormattingOptions::DefaultNoGrouping()),
			FText::AsNumber(ProjectAsset->ParsedProjectDefinition.Id, &FNumberFormattingOptions::DefaultNoGrouping())))
		.TextStyle(FEditorStyle::Get(), "MessageLog")
		.DecoratorStyleSet(&FEditorStyle::Get())
		+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateLambda(
		[](const FSlateHyperlinkRun::FMetadata& Metadata)
		{
			const FString* UrlPtr = Metadata.Find(TEXT("href"));
			if (UrlPtr)
			{
				FPlatformProcess::LaunchURL(**UrlPtr, nullptr, nullptr);
			}
		}))
	];

	IDetailCategoryBuilder& ScenesCategoryBuilder = DetailBuilder.EditCategory("Scenes");
	for (const FMixerInteractiveScene& Scene : ProjectAsset->ParsedProjectDefinition.Controls.Scenes)
	{
		IDetailGroup& SingleSceneGroup = ScenesCategoryBuilder.AddGroup(*Scene.Id, FText::FromString(Scene.Id));
		SingleSceneGroup.HeaderRow()
		.WholeRowContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
			.Text(FText::FromString(*Scene.Id))
		];

		for (const FMixerInteractiveControl& Control : Scene.Controls)
		{
			FString PrettyControlKind = Control.Kind;
			if (PrettyControlKind.Len() > 0)
			{
				PrettyControlKind[0] = FChar::ToUpper(PrettyControlKind[0]);
			}

			FName ControlName = *Control.Id;
			FText ControlDisplayName = FText::FromString(Control.Id);
			IDetailGroup& ControlGroup = SingleSceneGroup.AddGroup(ControlName, ControlDisplayName);
			ControlGroup.HeaderRow()
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(ControlDisplayName)
			]
			.ValueContent()
			.MaxDesiredWidth(500.0f)
			.MinDesiredWidth(100.0f)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(Control.IsCustom()
					? FText::Format(LOCTEXT("CustomControlKindFormatString", "Custom ({0})"), FText::FromString(PrettyControlKind))
					: FText::FromString(PrettyControlKind))
			];

			if (!Control.IsButton() && !Control.IsJoystick())
			{
				ControlGroup.AddWidgetRow()
				.NameContent()
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Right)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("BindCustomControlLabel", "Bind to class:"))
				]
				.ValueContent()
				.MaxDesiredWidth(500.0f)
				.MinDesiredWidth(100.0f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2.0f, 2.0f)
					[
						SNew(SClassPropertyEntryBox)
						.MetaClass(UMixerCustomControl::StaticClass())
						.AllowAbstract(false)
						.SelectedClass(this, &FMixerProjectDefinitionCustomization::GetClassForCustomControl, ControlBindingProperty, ControlName)
						.OnSetClass(this, &FMixerProjectDefinitionCustomization::SetClassForCustomControl, ControlBindingProperty, ControlName)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2.0f, 2.0f)
					[
						PropertyCustomizationHelpers::MakeUseSelectedButton(
							FSimpleDelegate::CreateSP(this, &FMixerProjectDefinitionCustomization::OnUseSelectedClicked, ControlBindingProperty, ControlName))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2.0f, 2.0f)
					[
						PropertyCustomizationHelpers::MakeBrowseButton(
							FSimpleDelegate::CreateSP(this, &FMixerProjectDefinitionCustomization::OnBrowseClicked, ControlBindingProperty, ControlName))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2.0f, 2.0f)
					[
						PropertyCustomizationHelpers::MakeNewBlueprintButton(
							FSimpleDelegate::CreateSP(this, &FMixerProjectDefinitionCustomization::OnMakeNewClicked, ControlBindingProperty, ControlName))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2.0f, 2.0f)
					[
						PropertyCustomizationHelpers::MakeClearButton(
							FSimpleDelegate::CreateSP(this, &FMixerProjectDefinitionCustomization::OnClearClicked, ControlBindingProperty, ControlName))
					]
				];
			}
		}
	}

}

const UClass* FMixerProjectDefinitionCustomization::GetClassForCustomControl(TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName) const
{
	TArray<UObject*> Outers;
	ControlBindingProperty->GetOuterObjects(Outers);
	UMixerProjectAsset* ProjectDefinition = CastChecked<UMixerProjectAsset>(Outers[0]);
	FSoftClassPath* CurrentClass = ProjectDefinition->CustomControlBindings.Find(ControlName);
	return CurrentClass != nullptr ? CurrentClass->TryLoadClass<UObject>() : nullptr;
}

void FMixerProjectDefinitionCustomization::SetClassForCustomControl(const UClass* NewClass, TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName) const
{
	ControlBindingProperty->NotifyPreChange();

	TArray<UObject*> Outers;
	ControlBindingProperty->GetOuterObjects(Outers);
	UMixerProjectAsset* ProjectDefinition = CastChecked<UMixerProjectAsset>(Outers[0]);
	if (NewClass != nullptr)
	{
		ProjectDefinition->CustomControlBindings.FindOrAdd(ControlName) = FSoftClassPath(NewClass);
	}
	else
	{
		ProjectDefinition->CustomControlBindings.Remove(ControlName);
	}
	ControlBindingProperty->NotifyPostChange();

	IMixerInteractivityEditorModule::Get().RefreshDesignTimeObjects();

	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerSimpleCustomControlInput::StaticClass());
	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerSimpleCustomControlUpdate::StaticClass());
}

void FMixerProjectDefinitionCustomization::OnUseSelectedClicked(TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName)
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	const UClass* SelectedClass = GEditor->GetFirstSelectedClass(UMixerCustomControl::StaticClass());
	if (SelectedClass)
	{
		SetClassForCustomControl(SelectedClass, ControlBindingProperty, ControlName);
	}
}

void FMixerProjectDefinitionCustomization::OnBrowseClicked(TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName)
{
	const UClass* SelectedClass = GetClassForCustomControl(ControlBindingProperty, ControlName);
	if (SelectedClass != nullptr && SelectedClass->ClassGeneratedBy != nullptr)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(SelectedClass->ClassGeneratedBy);
		if (ensure(Blueprint != NULL))
		{
			TArray<UObject*> SyncObjects;
			SyncObjects.Add(Blueprint);
			GEditor->SyncBrowserToObjects(SyncObjects);
		}
	}
}

void FMixerProjectDefinitionCustomization::OnMakeNewClicked(TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName)
{
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(LOCTEXT("CreateNewBlueprint", "Create New Blueprint"), UMixerCustomControl::StaticClass(), FString::Printf(TEXT("New%s"), *UMixerCustomControl::StaticClass()->GetName()));

	if (Blueprint != NULL && Blueprint->GeneratedClass)
	{
		SetClassForCustomControl(Blueprint->GeneratedClass, ControlBindingProperty, ControlName);

		FAssetEditorManager::Get().OpenEditorForAsset(Blueprint);
	}
}

void FMixerProjectDefinitionCustomization::OnClearClicked(TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName)
{
	SetClassForCustomControl(nullptr, ControlBindingProperty, ControlName);
}

#undef LOCTEXT_NAMESPACE
