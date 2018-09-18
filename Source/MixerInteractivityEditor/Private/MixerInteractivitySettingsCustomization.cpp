//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MixerInteractivitySettingsCustomization.h"
#include "MixerInteractivityBlueprintLibrary.h"
#include "MixerInteractivityTypes.h"
#include "MixerInteractivityModule.h"
#include "MixerInteractivitySettings.h"
#include "MixerInteractivityEditorModule.h"
#include "MixerInteractivityProjectAsset.h"
#include "K2Node_MixerCustomMethod.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "IDetailGroup.h"
#include "Widgets/Input/STextComboBox.h"
#include "PropertyHandle.h"
#include "Widgets/Images/SThrobber.h"
#include "Factories/DataAssetFactory.h"
#include "ContentBrowserModule.h"
#include "K2Node_MixerButton.h"
#include "K2Node_MixerStickEvent.h"
#include "K2Node_MixerSimpleCustomControlInput.h"
#include "K2Node_MixerSimpleCustomControlUpdate.h"
#include "BlueprintActionDatabase.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Notifications/SErrorHint.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

TSharedRef<IDetailCustomization> FMixerInteractivitySettingsCustomization::MakeInstance()
{
	return MakeShareable(new FMixerInteractivitySettingsCustomization);
}

FMixerInteractivitySettingsCustomization::FMixerInteractivitySettingsCustomization()
	: SelectedBindingMethod(EGameBindingMethod::FromMixerUser)
	, IsChangingUser(false)
{
	DownloadedProjectDefinition.Id = 0;
}

FMixerInteractivitySettingsCustomization::~FMixerInteractivitySettingsCustomization()
{
	IMixerInteractivityModule::Get().OnLoginStateChanged().Remove(LoginStateDelegateHandle);
}

void FMixerInteractivitySettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& AuthCategoryBuilder = DetailBuilder.EditCategory("Auth");

	AuthCategoryBuilder.AddCustomRow(LOCTEXT("GetClientId", "Get Client Id"), false)
	.WholeRowWidget
	[
		SNew(SBorder)
		.Padding(1)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(10, 10, 10, 10))
			.FillWidth(1.0f)
			[
				SNew(SRichTextBlock)
				.Text(LOCTEXT("GetClientIdText", "Visit the Mixer <a id=\"browser\" href=\"https://mixer.com/lab/oauth\">OAuth Clients Page</> to obtain a Client Id and configure valid hosts for your Redirect Uri."))
				.TextStyle(FEditorStyle::Get(), "MessageLog")
				.DecoratorStyleSet(&FEditorStyle::Get())
				.AutoWrapText(true)
				+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateLambda(
					[](const FSlateHyperlinkRun::FMetadata& Metadata)
					{
						const FString* UrlPtr = Metadata.Find(TEXT("href"));
						if (UrlPtr)
						{
							FPlatformProcess::LaunchURL(**UrlPtr, nullptr, nullptr);
						}
					}
				))
			]
		]
	];

	IDetailCategoryBuilder& GameBindingCategoryBuilder = DetailBuilder.EditCategory("Game Binding", FText::GetEmpty(), ECategoryPriority::Uncommon);

	TSharedRef<SWidget> HeaderContent =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SSpacer)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(2.0f, 2.0f)
		[
			SNew(SCheckBox)
			.Style(FCoreStyle::Get(), "RadioButton")
			.IsChecked(this, &FMixerInteractivitySettingsCustomization::CheckedWhenGameBindingMethod, EGameBindingMethod::Manual)
			.OnCheckStateChanged(this, &FMixerInteractivitySettingsCustomization::OnGameBindingMethodChanged, EGameBindingMethod::Manual)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GameBindingMethod_Manual", "Manual"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(2.0f, 2.0f)
		[
			SNew(SCheckBox)
			.Style(FCoreStyle::Get(), "RadioButton")
			.IsChecked(this, &FMixerInteractivitySettingsCustomization::CheckedWhenGameBindingMethod, EGameBindingMethod::FromMixerUser)
			.OnCheckStateChanged(this, &FMixerInteractivitySettingsCustomization::OnGameBindingMethodChanged, EGameBindingMethod::FromMixerUser)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GameBindingMethod_FromMixerUser", "From Mixer user:"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(2.0f, 2.0f)
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex(this, &FMixerInteractivitySettingsCustomization::GetWidgetIndexForLoginState)
			.IsEnabled(this, &FMixerInteractivitySettingsCustomization::EnabledWhenGameBindingMethod, EGameBindingMethod::FromMixerUser)
			+ SWidgetSwitcher::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(SButton)
					.IsEnabled(this, &FMixerInteractivitySettingsCustomization::GetLoginButtonEnabledState)
					.Text(LOCTEXT("LoginButtonText", "Login"))
					.ToolTipText(LOCTEXT("LoginButton_Tooltip", "Log into Mixer to select the game and version to associate with this project"))
					.OnClicked(this, &FMixerInteractivitySettingsCustomization::Login)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(SErrorHint)
					.ErrorText(LOCTEXT("MissingLoginInfo", "Client Id and Redirect Uri required."))
					.Visibility(this, &FMixerInteractivitySettingsCustomization::GetLoginErrorVisibility)
				]
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SCircularThrobber)
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(0.0f, 0.0f, 14.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(this, &FMixerInteractivitySettingsCustomization::GetCurrentUserText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("ChangeUserButtonText", "Change user"))
					.ToolTipText(LOCTEXT("ChangeUserButton_Tooltip", "Change the current Mixer user (affects the set of interactive games that may be associated with this project"))
					.OnClicked(this, &FMixerInteractivitySettingsCustomization::ChangeUser)
				]
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SCircularThrobber)
			]
		];

	GameBindingCategoryBuilder.HeaderContent(HeaderContent);

	TSharedRef<IPropertyHandle> GameNameProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, GameName));
	DetailBuilder.HideProperty(GameNameProperty);

	GameBindingCategoryBuilder.AddCustomRow(GameNameProperty->GetPropertyDisplayName())
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FMixerInteractivitySettingsCustomization::VisibleWhenGameBindingMethod, EGameBindingMethod::FromMixerUser)))
	.NameContent()
	[
		GameNameProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SBox)
		.IsEnabled(this, &FMixerInteractivitySettingsCustomization::GetEnabledStateRequiresLogin)
		[
			PropertyCustomizationHelpers::MakePropertyComboBox(
				GameNameProperty,
				FOnGetPropertyComboBoxStrings::CreateSP(this, &FMixerInteractivitySettingsCustomization::GetContentForGameNameCombo))
		]
	];

	TSharedRef<IPropertyHandle> GameVersionProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, GameVersionId));

	IDetailPropertyRow& VersionRow = GameBindingCategoryBuilder.AddProperty(GameVersionProperty);
	TSharedPtr<SWidget> VersionNameWidget;
	TSharedPtr<SWidget> VersionValueWidget;
	VersionRow.GetDefaultWidgets(VersionNameWidget, VersionValueWidget);

	VersionRow.CustomWidget()
	.NameContent()
	[
		VersionNameWidget.ToSharedRef()
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SWidgetSwitcher)
		.WidgetIndex(this, &FMixerInteractivitySettingsCustomization::GetWidgetIndexForGameBindingMethod)
		+ SWidgetSwitcher::Slot()
		[
			VersionValueWidget.ToSharedRef()
		]
		+ SWidgetSwitcher::Slot()
		[
			SNew(SBox)
			.IsEnabled(this, &FMixerInteractivitySettingsCustomization::GetEnabledStateRequiresLogin)
			[
				PropertyCustomizationHelpers::MakePropertyComboBox(
					GameVersionProperty,
					FOnGetPropertyComboBoxStrings::CreateSP(this, &FMixerInteractivitySettingsCustomization::GetContentForGameVersionCombo),
					FOnGetPropertyComboBoxValue::CreateSP(this, &FMixerInteractivitySettingsCustomization::GetGameVersionComboValue),
					FOnPropertyComboBoxValueSelected::CreateSP(this, &FMixerInteractivitySettingsCustomization::OnGameVersionComboValueSelected, GameVersionProperty))
			]
		]
	];

	TSharedRef<IPropertyHandle> ShareCodeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, ShareCode));
	GameBindingCategoryBuilder.AddProperty(ShareCodeProperty);

	IDetailCategoryBuilder& ControlsCategoryBuilder = DetailBuilder.EditCategory("Interactive Controls", FText::GetEmpty(), ECategoryPriority::Uncommon);

	DesignTimeGroupsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, DesignTimeGroups));
	DetailBuilder.HideProperty(DesignTimeGroupsProperty);

	TSharedRef<IPropertyHandle> ProjectDefinitionProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, ProjectDefinition));
	IDetailPropertyRow& ProjectDefinitionRow = ControlsCategoryBuilder.AddProperty(ProjectDefinitionProperty);
	TSharedPtr<SWidget> ProjectDefNameWidget;
	TSharedPtr<SWidget> ProjectDefValueWidget;
	ProjectDefinitionRow.GetDefaultWidgets(ProjectDefNameWidget, ProjectDefValueWidget);

	ProjectDefinitionRow.CustomWidget()
	.NameContent()
	[
		ProjectDefNameWidget.ToSharedRef()
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			ProjectDefValueWidget.ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SErrorHint)
			.ErrorText(LOCTEXT("Error_ProjectVersionMismatch", "The selected project definition has a Version Id that does not match the Version Id selected in the Mixer Interactivity settings.  This may cause runtime Interactive Controls to be different from those available in the Blueprint Editor."))
			.Visibility(this, &FMixerInteractivitySettingsCustomization::GetVersionMismatchErrorVisibility)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Fill)
		.Padding(16.0f, 16.0f)
		[
			SNew(SVerticalBox)
			.Visibility(this, &FMixerInteractivitySettingsCustomization::GetUpdateControlSheetFromOnlineVisibility)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(STextBlock)
				.Text(this, &FMixerInteractivitySettingsCustomization::GetUpdateControlSheetFromOnlineInfoText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 2.0f, 2.0f, 2.0f)
				[
					SNew(SButton)
					.Visibility(this, &FMixerInteractivitySettingsCustomization::GetUpdateExistingControlSheetVisibility)
					.Text(LOCTEXT("ControlSheetUpdateNow", "Update now"))
					.OnClicked(this, &FMixerInteractivitySettingsCustomization::UpdateControlSheetFromOnline)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ControlSheetSaveLocal", "Save local..."))
					.OnClicked(this, &FMixerInteractivitySettingsCustomization::CreateNewControlSheetFromOnline, ProjectDefinitionProperty)
				]
			]
		]
	];

	TSharedRef<FDetailArrayBuilder> GroupsBuilder = MakeShareable(new FDetailArrayBuilder(DesignTimeGroupsProperty.ToSharedRef()));
	GroupsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FMixerInteractivitySettingsCustomization::GenerateWidgetForDesignTimeGroupsElement));
	ControlsCategoryBuilder.AddCustomBuilder(GroupsBuilder);
	FSimpleDelegate GroupsChangedDelegate = FSimpleDelegate::CreateLambda([GroupsBuilder]() 
	{ 
		GroupsBuilder->RefreshChildren();
		IMixerInteractivityEditorModule::Get().RefreshDesignTimeObjects(); 
	});
	DesignTimeGroupsProperty->AsArray()->SetOnNumElementsChanged(GroupsChangedDelegate);

	TSharedRef<IPropertyHandle> CustomMethodsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, CustomMethods));
	CustomMethodsProperty->SetOnPropertyValuePreChange(FSimpleDelegate::CreateSP(this, &FMixerInteractivitySettingsCustomization::OnCustomMethodsPreChange));
	CustomMethodsProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FMixerInteractivitySettingsCustomization::OnCustomMethodsPostChange));

	if (!LoginStateDelegateHandle.IsValid())
	{
		LoginStateDelegateHandle = IMixerInteractivityModule::Get().OnLoginStateChanged().AddSP(this, &FMixerInteractivitySettingsCustomization::OnLoginStateChanged);
	}

	if (!IMixerInteractivityModule::Get().LoginSilently(nullptr))
	{
		IMixerInteractivityEditorModule::Get().RequestAvailableInteractiveGames(FOnMixerInteractiveGamesRequestFinished::CreateSP(this, &FMixerInteractivitySettingsCustomization::OnInteractiveGamesRequestFinished));
	}

	IsChangingUser = false;
}

FReply FMixerInteractivitySettingsCustomization::Login()
{
	IMixerInteractivityModule::Get().LoginWithUI(nullptr);
	return FReply::Handled();
}

FReply FMixerInteractivitySettingsCustomization::ChangeUser()
{
	IsChangingUser = true;
	IMixerInteractivityModule::Get().Logout();
	return FReply::Handled();
}

FText FMixerInteractivitySettingsCustomization::GetCurrentUserText() const
{
	TSharedPtr<const FMixerLocalUser> CurrentUser = IMixerInteractivityModule::Get().GetCurrentUser();
	return CurrentUser.IsValid() ? FText::FromString(CurrentUser->Name) : FText::GetEmpty();
}


void FMixerInteractivitySettingsCustomization::OnLoginStateChanged(EMixerLoginState NewState)
{	
	switch (NewState)
	{
	case EMixerLoginState::Logged_In:
		IMixerInteractivityEditorModule::Get().RequestAvailableInteractiveGames(FOnMixerInteractiveGamesRequestFinished::CreateSP(this, &FMixerInteractivitySettingsCustomization::OnInteractiveGamesRequestFinished));
		break;

	case EMixerLoginState::Not_Logged_In:
		if (IsChangingUser)
		{
			IsChangingUser = false;
			IMixerInteractivityModule::Get().LoginWithUI(nullptr);
		}
		break;

	default:
		break;
	}
}

void FMixerInteractivitySettingsCustomization::OnInteractiveGamesRequestFinished(bool Success, const TArray<FMixerInteractiveGame>& Games)
{
	InteractiveGames = Games;

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	if (Settings->GameVersionId != 0)
	{
		FMixerInteractiveGameVersion FakeVersion;
		FakeVersion.Id = Settings->GameVersionId;
		IMixerInteractivityEditorModule::Get().RequestInteractiveControlsForGameVersion(FakeVersion, FOnMixerInteractiveControlsRequestFinished::CreateSP(this, &FMixerInteractivitySettingsCustomization::OnInteractiveControlsForVersionRequestFinished));
	}
}

bool FMixerInteractivitySettingsCustomization::GetEnabledStateRequiresLogin() const
{
	return IMixerInteractivityModule::Get().GetLoginState() == EMixerLoginState::Logged_In;
}

void FMixerInteractivitySettingsCustomization::OnGameBindingMethodChanged(ECheckBoxState NewCheckState, EGameBindingMethod ForBindingMethod)
{
	if (NewCheckState == ECheckBoxState::Checked)
	{
		SelectedBindingMethod = ForBindingMethod;
	}
}

void FMixerInteractivitySettingsCustomization::OnInteractiveControlsForVersionRequestFinished(bool Success, const FMixerInteractiveGameVersion& VersionWithControls)
{
	if (Success)
	{
		DownloadedProjectDefinition = VersionWithControls;
	}
	else
	{
		DownloadedProjectDefinition.Id = 0;
		DownloadedProjectDefinition.Name.Empty();
		DownloadedProjectDefinition.Controls.Scenes.Empty();
	}
}

EVisibility FMixerInteractivitySettingsCustomization::GetLoginErrorVisibility() const
{
	return GetLoginButtonEnabledState() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FMixerInteractivitySettingsCustomization::GetUpdateControlSheetFromOnlineVisibility() const
{
	if (IMixerInteractivityModule::Get().GetLoginState() != EMixerLoginState::Logged_In)
	{
		return EVisibility::Collapsed;
	}

	if (DownloadedProjectDefinition.Id == 0)
	{
		return EVisibility::Collapsed;
	}

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	UMixerProjectAsset* ProjectAsset = Cast<UMixerProjectAsset>(Settings->ProjectDefinition.TryLoad());
	if (ProjectAsset == nullptr)
	{
		return EVisibility::Visible;
	}

	return ProjectAsset->ParsedProjectDefinition == DownloadedProjectDefinition ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FMixerInteractivitySettingsCustomization::GetVersionMismatchErrorVisibility() const
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	UMixerProjectAsset* CurrentDefinition = Cast<UMixerProjectAsset>(Settings->ProjectDefinition.TryLoad());
	if (CurrentDefinition != nullptr && CurrentDefinition->ParsedProjectDefinition.Id != Settings->GameVersionId)
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

EVisibility FMixerInteractivitySettingsCustomization::GetUpdateExistingControlSheetVisibility() const
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	// Full-on TryLoad here, since if this passes we assume the existing entry is fully usable.
	return Settings->ProjectDefinition.TryLoad() != nullptr ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FMixerInteractivitySettingsCustomization::VisibleWhenGameBindingMethod(EGameBindingMethod InMethod) const
{
	return SelectedBindingMethod == InMethod ? EVisibility::Visible : EVisibility::Collapsed;
}

ECheckBoxState FMixerInteractivitySettingsCustomization::CheckedWhenGameBindingMethod(EGameBindingMethod InMethod) const
{
	return SelectedBindingMethod == InMethod ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool FMixerInteractivitySettingsCustomization::EnabledWhenGameBindingMethod(EGameBindingMethod InMethod) const
{
	return SelectedBindingMethod == InMethod;
}

int32 FMixerInteractivitySettingsCustomization::GetWidgetIndexForLoginState() const
{
	return static_cast<int32>(IMixerInteractivityModule::Get().GetLoginState());
}

int32 FMixerInteractivitySettingsCustomization::GetWidgetIndexForGameBindingMethod() const
{
	return static_cast<int32>(SelectedBindingMethod);
}

FText FMixerInteractivitySettingsCustomization::GetUpdateControlSheetFromOnlineInfoText() const
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	if (Settings->ProjectDefinition.TryLoad() != nullptr)
	{
		return LOCTEXT("ControlSheetNeedsUpdate", "The controls for the selected interactive game and version are different to the local saved version.");
	}
	else
	{
		return LOCTEXT("ControlSheetMissing", "You can save information about the current interactivity controls to an Unreal asset.  This will allow you to work with Mixer controls in Blueprint, even when not signed in to Mixer.");
	}
}

FReply FMixerInteractivitySettingsCustomization::UpdateControlSheetFromOnline()
{
	UMixerInteractivitySettings* Settings = GetMutableDefault<UMixerInteractivitySettings>();
	UMixerProjectAsset* CurrentDefinition = Cast<UMixerProjectAsset>(Settings->ProjectDefinition.TryLoad());
	check(CurrentDefinition != nullptr);

	CurrentDefinition->ParsedProjectDefinition = DownloadedProjectDefinition;
	CurrentDefinition->ProjectDefinitionJson = DownloadedProjectDefinition.ToJson(false);
	CurrentDefinition->MarkPackageDirty();

	IMixerInteractivityEditorModule::Get().RefreshDesignTimeObjects();

	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerButton::StaticClass());
	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerStickEvent::StaticClass());
	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerSimpleCustomControlInput::StaticClass());
	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerSimpleCustomControlUpdate::StaticClass());

	return FReply::Handled();
}

FReply FMixerInteractivitySettingsCustomization::CreateNewControlSheetFromOnline(TSharedRef<IPropertyHandle> ProjectDefinitionProperty)
{
	UMixerProjectAsset* ProjectDefinition;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	FString DefaultPath = TEXT("/Game/");
	FString DefaultName = TEXT("NewMixerProject");
	FString UniquePackageName;
	FString UniqueAssetName;
	AssetTools.Get().CreateUniqueAssetName(DefaultPath / DefaultName, TEXT(""), UniquePackageName, UniqueAssetName);
	DefaultName = FPaths::GetCleanFilename(UniqueAssetName);

	FSaveAssetDialogConfig SaveConfig;
	SaveConfig.DefaultPath = DefaultPath;
	SaveConfig.DefaultAssetName = DefaultName;
	SaveConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
	FString SaveToPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveConfig);
	FText BadPathReason;
	if (!FPackageName::IsValidObjectPath(SaveToPath, &BadPathReason))
	{
		FNotificationInfo Info(FText::Format(LOCTEXT("NewProjectDefinition_Failed", "Failed to create Mixer project definition asset: {0}"), BadPathReason));
		Info.ExpireDuration = 8.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		return FReply::Handled();
	}

	const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveToPath);
	const FString SavePackagePath = FPaths::GetPath(SavePackageName);
	const FString SaveAssetName = FPaths::GetBaseFilename(SavePackageName);

	ProjectDefinition = Cast<UMixerProjectAsset>(AssetTools.Get().CreateAsset(SaveAssetName, SavePackagePath, UMixerProjectAsset::StaticClass(), nullptr));

	UMixerInteractivitySettings* Settings = GetMutableDefault<UMixerInteractivitySettings>();
	ProjectDefinitionProperty->NotifyPreChange();
	Settings->ProjectDefinition = FSoftObjectPath(ProjectDefinition);
	ProjectDefinitionProperty->NotifyPostChange();

	return UpdateControlSheetFromOnline();
}

void FMixerInteractivitySettingsCustomization::GenerateWidgetForDesignTimeGroupsElement(TSharedRef<IPropertyHandle> StructProperty, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	if (ArrayIndex == 0)
	{
		ChildrenBuilder.AddCustomRow(FText::GetEmpty())
			.NameContent()
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GroupName", "Name"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Margin(2.0f)
			]
			.ValueContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GroupInitialScene", "Initial Scene"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Margin(2.0f)
			];
	}

	TSharedRef<IPropertyHandle> GroupNameProperty = StructProperty->GetChildHandle(FName("Name")).ToSharedRef();
	TSharedRef<IPropertyHandle> InitialSceneProperty = StructProperty->GetChildHandle(FName("InitialScene")).ToSharedRef();
	TSharedPtr<SComboBox<TSharedPtr<FName>>> InitialSceneCombo;
	ChildrenBuilder.AddProperty(StructProperty).CustomWidget()
		.NameContent()
		.MinDesiredWidth(200.0f)
		.MaxDesiredWidth(400.0f)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SErrorHint)
				.ErrorText(LOCTEXT("GroupNameDuplicateError", "A group with this name has already been defined.  Group names must be unique."))
				.Visibility(this, &FMixerInteractivitySettingsCustomization::GetErrorVisibilityForGroup, ArrayIndex)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				GroupNameProperty->CreatePropertyValueWidget()
			]
		]
		.ValueContent()
		[
			SAssignNew(InitialSceneCombo, SComboBox<TSharedPtr<FName>>)
			.OptionsSource(&IMixerInteractivityEditorModule::Get().GetDesignTimeObjects(TEXT("scene")))
			.OnGenerateWidget(this, &FMixerInteractivitySettingsCustomization::GenerateWidgetForGroupInitialScene)
			.OnSelectionChanged(this, &FMixerInteractivitySettingsCustomization::OnGroupInitialSceneSelectionChanged, InitialSceneProperty)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &FMixerInteractivitySettingsCustomization::GetInitialSceneComboText, InitialSceneProperty)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];

	FSimpleDelegate GroupNameChangedDelegate = FSimpleDelegate::CreateLambda([]()
	{
		IMixerInteractivityEditorModule::Get().RefreshDesignTimeObjects();
	});
	GroupNameProperty->SetOnPropertyValueChanged(GroupNameChangedDelegate);
	IMixerInteractivityEditorModule::Get().OnDesignTimeObjectsChanged().AddSP(InitialSceneCombo.ToSharedRef(), &SComboBox<TSharedPtr<FName>>::RefreshOptions);
}

TSharedRef<SWidget> FMixerInteractivitySettingsCustomization::GenerateWidgetForGroupInitialScene(TSharedPtr<FName> SceneName)
{
	return
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(FText::FromString(*SceneName->ToString()));
}

void FMixerInteractivitySettingsCustomization::OnGroupInitialSceneSelectionChanged(TSharedPtr<FName> SceneName, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ChangedProperty)
{
	if (SceneName.IsValid())
	{
		ChangedProperty->SetValue(*SceneName);
	}
	else
	{
		ChangedProperty->SetValue(NAME_None);
	}
}

FText FMixerInteractivitySettingsCustomization::GetInitialSceneComboText(TSharedRef<IPropertyHandle> Property) const
{
	FText DisplayText;
	Property->GetValueAsDisplayText(DisplayText);
	return DisplayText;
}

EVisibility FMixerInteractivitySettingsCustomization::GetErrorVisibilityForGroup(int32 GroupElementIndex) const
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();

	if (GroupElementIndex < Settings->DesignTimeGroups.Num())
	{
		FName ThisElementName = Settings->DesignTimeGroups[GroupElementIndex].Name;
		for (int i = 0; i < GroupElementIndex; ++i)
		{
			if (Settings->DesignTimeGroups[i].Name == ThisElementName)
			{
				return EVisibility::Visible;
			}
		}
	}
	return EVisibility::Collapsed;
}

bool FMixerInteractivitySettingsCustomization::GetLoginButtonEnabledState() const
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	return !Settings->ClientId.IsEmpty() && !Settings->RedirectUri.IsEmpty();
}

void FMixerInteractivitySettingsCustomization::OnCustomMethodsPreChange()
{
	const FSoftClassPath& CustomMethodsPath = GetDefault<UMixerInteractivitySettings>()->CustomMethods;
	if (CustomMethodsPath.IsValid())
	{
		UClass* CustomMethods = CustomMethodsPath.ResolveClass();
		if (CustomMethods != nullptr)
		{
			UBlueprint* CustomMethodsBlueprint = Cast<UBlueprint>(CustomMethods->ClassGeneratedBy);
			if (CustomMethodsBlueprint != nullptr)
			{
				CustomMethodsBlueprint->OnCompiled().RemoveAll(this);
			}
		}
	}
}

void FMixerInteractivitySettingsCustomization::OnCustomMethodsPostChange()
{
	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerCustomMethod::StaticClass());

	const FSoftClassPath& CustomMethodsPath = GetDefault<UMixerInteractivitySettings>()->CustomMethods;
	if (CustomMethodsPath.IsValid())
	{
		UClass* CustomMethods = CustomMethodsPath.TryLoadClass<UObject>();
		if (CustomMethods != nullptr)
		{
			UBlueprint* CustomMethodsBlueprint = Cast<UBlueprint>(CustomMethods->ClassGeneratedBy);
			if (CustomMethodsBlueprint != nullptr)
			{
				CustomMethodsBlueprint->OnCompiled().AddLambda(
					[](UBlueprint*)
				{
					FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerCustomMethod::StaticClass());
				});
			}
		}
	}
}

void FMixerInteractivitySettingsCustomization::GetContentForGameNameCombo(TArray<TSharedPtr<FString>>& OutStrings, TArray<TSharedPtr<SToolTip>>& OutTooltips, TArray<bool>& OutRestrictedItems) const
{
	for (const FMixerInteractiveGame& Game : InteractiveGames)
	{
		OutStrings.Add(MakeShared<FString>(Game.Name));
		OutTooltips.Add(SNew(SToolTip).Text(FText::FromString(Game.Description)));
		OutRestrictedItems.Add(false);
	}
}

void FMixerInteractivitySettingsCustomization::GetContentForGameVersionCombo(TArray<TSharedPtr<FString>>& OutStrings, TArray<TSharedPtr<SToolTip>>& OutTooltips, TArray<bool>& OutRestrictedItems) const
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	for (const FMixerInteractiveGame& Game : InteractiveGames)
	{
		if (Game.Name == Settings->GameName)
		{
			for (const FMixerInteractiveGameVersion& Version : Game.Versions)
			{
				OutStrings.Add(MakeShared<FString>(Version.Name));
				OutTooltips.Add(SNew(SToolTip).Text(FText::Format(LOCTEXT("VersionTooltipFormatString", "Id: {0}"), Version.Id)));
				OutRestrictedItems.Add(false);
			}
		}
	}
}

FString FMixerInteractivitySettingsCustomization::GetGameVersionComboValue() const
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	for (const FMixerInteractiveGame& Game : InteractiveGames)
	{
		if (Game.Name == Settings->GameName)
		{
			for (const FMixerInteractiveGameVersion& Version : Game.Versions)
			{
				if (Version.Id == Settings->GameVersionId)
				{
					return Version.Name;
				}
			}
		}
	}

	return FString::Printf(TEXT("Unknown (id: %d)"), Settings->GameVersionId);
}

void FMixerInteractivitySettingsCustomization::OnGameVersionComboValueSelected(const FString& SelectedString, TSharedRef<IPropertyHandle> GameVersionProperty)
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	for (const FMixerInteractiveGame& Game : InteractiveGames)
	{
		if (Game.Name == Settings->GameName)
		{
			for (const FMixerInteractiveGameVersion& Version : Game.Versions)
			{
				if (Version.Name == SelectedString)
				{
					GameVersionProperty->SetValue(Version.Id);
					IMixerInteractivityEditorModule::Get().RequestInteractiveControlsForGameVersion(Version, FOnMixerInteractiveControlsRequestFinished::CreateSP(this, &FMixerInteractivitySettingsCustomization::OnInteractiveControlsForVersionRequestFinished));
				}
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
