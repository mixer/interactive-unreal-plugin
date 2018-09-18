#pragma once

#include "MixerProjectDefinitionCustomization.h"
#include "MixerInteractivityEditorModule.h"
#include "MixerInteractivityProjectAsset.h"
#include "MixerInteractivityJsonTypes.h"
#include "MixerCustomControl.h"
#include "MixerDynamicDelegateBinding.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Components/HorizontalBox.h"
#include "K2Node_MixerSimpleCustomControlInput.h"
#include "K2Node_MixerSimpleCustomControlUpdate.h"
#include "BlueprintActionDatabase.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Toolkits/AssetEditorManager.h"
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

	TSharedRef<IPropertyHandle> ControlMappingProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerProjectAsset, CustomControlMappings));
	DetailBuilder.HideProperty(ControlMappingProperty);

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

		FName SceneName = *Scene.Id;
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

			if (Control.IsCustom())
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
						.SelectedClass(this, &FMixerProjectDefinitionCustomization::GetClassForCustomControl, ControlMappingProperty, ControlName, SceneName)
						.OnSetClass(this, &FMixerProjectDefinitionCustomization::SetClassForCustomControl, ControlMappingProperty, ControlName, SceneName)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2.0f, 2.0f)
					[
						PropertyCustomizationHelpers::MakeUseSelectedButton(
							FSimpleDelegate::CreateSP(this, &FMixerProjectDefinitionCustomization::OnUseSelectedClicked, ControlMappingProperty, ControlName, SceneName))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2.0f, 2.0f)
					[
						PropertyCustomizationHelpers::MakeBrowseButton(
							FSimpleDelegate::CreateSP(this, &FMixerProjectDefinitionCustomization::OnBrowseClicked, ControlMappingProperty, ControlName, SceneName))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2.0f, 2.0f)
					[
						PropertyCustomizationHelpers::MakeNewBlueprintButton(
							FSimpleDelegate::CreateSP(this, &FMixerProjectDefinitionCustomization::OnMakeNewClicked, ControlMappingProperty, ControlName, SceneName))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2.0f, 2.0f)
					[
						PropertyCustomizationHelpers::MakeClearButton(
							FSimpleDelegate::CreateSP(this, &FMixerProjectDefinitionCustomization::OnClearClicked, ControlMappingProperty, ControlName, SceneName))
					]
				];
			}
		}
	}

}

const UClass* FMixerProjectDefinitionCustomization::GetClassForCustomControl(TSharedRef<IPropertyHandle> ControlMappingProperty, FName ControlName, FName SceneName) const
{
	TArray<UObject*> Outers;
	ControlMappingProperty->GetOuterObjects(Outers);
	UMixerProjectAsset* ProjectDefinition = CastChecked<UMixerProjectAsset>(Outers[0]);
	FMixerCustomControlMapping* Binding = ProjectDefinition->CustomControlMappings.Find(ControlName);
	return Binding != nullptr ? Binding->Class.TryLoadClass<UObject>() : nullptr;
}

void FMixerProjectDefinitionCustomization::SetClassForCustomControl(const UClass* NewClass, TSharedRef<IPropertyHandle> ControlMappingProperty, FName ControlName, FName SceneName) const
{
	ControlMappingProperty->NotifyPreChange();

	TArray<UObject*> Outers;
	ControlMappingProperty->GetOuterObjects(Outers);
	UMixerProjectAsset* ProjectDefinition = CastChecked<UMixerProjectAsset>(Outers[0]);
	if (NewClass != nullptr)
	{
		FMixerCustomControlMapping& Binding = ProjectDefinition->CustomControlMappings.FindOrAdd(ControlName);
		Binding.SceneName = SceneName;
		Binding.Class = FSoftClassPath(NewClass);
	}
	else
	{
		ProjectDefinition->CustomControlMappings.Remove(ControlName);
	}
	ControlMappingProperty->NotifyPostChange();

	IMixerInteractivityEditorModule::Get().RefreshDesignTimeObjects();

	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerSimpleCustomControlInput::StaticClass());
	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerSimpleCustomControlUpdate::StaticClass());

	UMixerInteractivityBlueprintEventSource::GetBlueprintEventSource(GEditor->GetEditorWorldContext().World())->RefreshCustomControls();
}

void FMixerProjectDefinitionCustomization::OnUseSelectedClicked(TSharedRef<IPropertyHandle> ControlMappingProperty, FName ControlName, FName SceneName) const
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	const UClass* SelectedClass = GEditor->GetFirstSelectedClass(UMixerCustomControl::StaticClass());
	if (SelectedClass)
	{
		SetClassForCustomControl(SelectedClass, ControlMappingProperty, ControlName, SceneName);
	}
}

void FMixerProjectDefinitionCustomization::OnBrowseClicked(TSharedRef<IPropertyHandle> ControlMappingProperty, FName ControlName, FName SceneName) const
{
	const UClass* SelectedClass = GetClassForCustomControl(ControlMappingProperty, ControlName, SceneName);
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

void FMixerProjectDefinitionCustomization::OnMakeNewClicked(TSharedRef<IPropertyHandle> ControlMappingProperty, FName ControlName, FName SceneName) const
{
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(LOCTEXT("CreateNewBlueprint", "Create New Blueprint"), UMixerCustomControl::StaticClass(), FString::Printf(TEXT("New%s"), *UMixerCustomControl::StaticClass()->GetName()));

	if (Blueprint != NULL && Blueprint->GeneratedClass)
	{
		SetClassForCustomControl(Blueprint->GeneratedClass, ControlMappingProperty, ControlName, SceneName);

		FAssetEditorManager::Get().OpenEditorForAsset(Blueprint);
	}
}

void FMixerProjectDefinitionCustomization::OnClearClicked(TSharedRef<IPropertyHandle> ControlMappingProperty, FName ControlName, FName SceneName) const
{
	SetClassForCustomControl(nullptr, ControlMappingProperty, ControlName, SceneName);
}

#undef LOCTEXT_NAMESPACE
