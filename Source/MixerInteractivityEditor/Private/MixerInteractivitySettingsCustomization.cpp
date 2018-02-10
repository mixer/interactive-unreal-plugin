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
#include "MixerInteractiveGame.h"
#include "K2Node_MixerCustomGlobalEvent.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "IDetailGroup.h"
#include "Input/STextComboBox.h"
#include "PropertyHandle.h"
#include "Images/SThrobber.h"
#include "Factories/DataAssetFactory.h"
#include "ContentBrowserModule.h"
#include "K2Node_MixerButton.h"
#include "K2Node_MixerStickEvent.h"
#include "BlueprintActionDatabase.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"
#include "SErrorHint.h"
#include "SButton.h"
#include "SRichTextBlock.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

TSharedRef<IDetailCustomization> FMixerInteractivitySettingsCustomization::MakeInstance()
{
	return MakeShareable(new FMixerInteractivitySettingsCustomization);
}

FMixerInteractivitySettingsCustomization::FMixerInteractivitySettingsCustomization()
{
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
		.FillWidth(0.8f)
		.HAlign(EHorizontalAlignment::HAlign_Right)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(2.0f, 2.0f, 16.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(this, &FMixerInteractivitySettingsCustomization::GetCurrentUserText)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(EHorizontalAlignment::HAlign_Right)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(2.0f, 2.0f)
		[
			SNew(SButton)
			.Visibility(this, &FMixerInteractivitySettingsCustomization::GetLoginButtonVisibility)
			.IsEnabled(this, &FMixerInteractivitySettingsCustomization::GetLoginButtonEnabledState)
			.Text(LOCTEXT("LoginButtonText", "Login"))
			.ToolTipText(LOCTEXT("LoginButton_Tooltip", "Log into Mixer to select the game and version to associate with this project"))
			.OnClicked(this, &FMixerInteractivitySettingsCustomization::Login)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(EHorizontalAlignment::HAlign_Right)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(2.0f, 2.0f)
		[
			SNew(SButton)
			.Visibility(this, &FMixerInteractivitySettingsCustomization::GetChangeUserVisibility)
			.Text(LOCTEXT("ChangeUserButtonText", "Change user"))
			.ToolTipText(LOCTEXT("ChangeUserButton_Tooltip", "Change the current Mixer user (affects the set of interactive games that may be associated with this project"))
			.OnClicked(this, &FMixerInteractivitySettingsCustomization::ChangeUser)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(EHorizontalAlignment::HAlign_Right)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(2.0f, 2.0f)
		[
			SNew(SCircularThrobber)
			.Visibility(this, &FMixerInteractivitySettingsCustomization::GetLoginInProgressVisibility)
			.Radius(12.0f)
		];


	GameBindingCategoryBuilder.HeaderContent(HeaderContent);

	TSharedRef<IPropertyHandle> GameNameProperty = DetailBuilder.GetProperty("GameName");
	DetailBuilder.HideProperty(GameNameProperty);
	TSharedRef<IPropertyHandle> GameVersionProperty = DetailBuilder.GetProperty("GameVersionId");
	DetailBuilder.HideProperty(GameVersionProperty);

	GameBindingCategoryBuilder.AddCustomRow(GameNameProperty->GetPropertyDisplayName())
	.NameContent()
	[
		GameNameProperty->CreatePropertyNameWidget()
	]
	.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMixerInteractivitySettingsCustomization::GetEnabledStateRequiresLogin)))
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(GameNamesBox, SComboBox<TSharedPtr<FMixerInteractiveGame>>)
			.OptionsSource(&InteractiveGames)
			.OnSelectionChanged(this, &FMixerInteractivitySettingsCustomization::OnGameSelectionChanged, GameNameProperty)
			.OnGenerateWidget(this, &FMixerInteractivitySettingsCustomization::GenerateWidgetForGameComboRow)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &FMixerInteractivitySettingsCustomization::GetSelectedGameComboText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
	];

	GameBindingCategoryBuilder.AddCustomRow(GameVersionProperty->GetPropertyDisplayName())
	.NameContent()
	[
		GameVersionProperty->CreatePropertyNameWidget()
	]
	.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMixerInteractivitySettingsCustomization::GetEnabledStateRequiresLogin)))
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(GameVersionsBox, SComboBox<TSharedPtr<FMixerInteractiveGameVersion>>)
			.OptionsSource(&InteractiveVersions)
			.OnSelectionChanged(this, &FMixerInteractivitySettingsCustomization::OnVersionSelectionChanged, GameVersionProperty)
			.OnGenerateWidget(this, &FMixerInteractivitySettingsCustomization::GenerateWidgetForVersionComboRow)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &FMixerInteractivitySettingsCustomization::GetSelectedVersionComboText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
	];

	TSharedRef<IPropertyHandle> ShareCodeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, ShareCode));
	GameBindingCategoryBuilder.AddProperty(ShareCodeProperty);

	CachedButtonsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, CachedButtons));
	DetailBuilder.HideProperty(CachedButtonsProperty);
	CachedSticksProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, CachedSticks));
	DetailBuilder.HideProperty(CachedSticksProperty);
	CachedScenesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, CachedScenes));
	DetailBuilder.HideProperty(CachedScenesProperty);
	DesignTimeGroupsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, DesignTimeGroups));
	DetailBuilder.HideProperty(DesignTimeGroupsProperty);

	TSharedRef<FDetailArrayBuilder> GroupsBuilder = MakeShareable(new FDetailArrayBuilder(DesignTimeGroupsProperty.ToSharedRef()));
	GroupsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FMixerInteractivitySettingsCustomization::GenerateWidgetForDesignTimeGroupsElement));
	GameBindingCategoryBuilder.AddCustomBuilder(GroupsBuilder);
	FSimpleDelegate GroupsChangedDelegate = FSimpleDelegate::CreateLambda([GroupsBuilder]() 
	{ 
		GroupsBuilder->RefreshChildren();
		IMixerInteractivityEditorModule::Get().RefreshDesignTimeObjects(); 
	});
	DesignTimeGroupsProperty->AsArray()->SetOnNumElementsChanged(GroupsChangedDelegate);

	IDetailGroup& ControlSheetGroup = GameBindingCategoryBuilder.AddGroup("InteractiveControls", LOCTEXT("InteractiveControls", "Interactive Controls"));
	
	ControlSheetGroup.HeaderRow()
	.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMixerInteractivitySettingsCustomization::GetEnabledStateRequiresLogin)))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("InteractiveControls", "Interactive Controls"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(400.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		.Visibility(this, &FMixerInteractivitySettingsCustomization::GetUpdateControlSheetFromOnlineVisibility)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Fill)
		[
			SNew(SVerticalBox)
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
				SNew(SButton)
				.Text(this, &FMixerInteractivitySettingsCustomization::GetUpdateControlSheetFromOnlineButtonText)
				.OnClicked(this, &FMixerInteractivitySettingsCustomization::UpdateControlSheetFromOnline)
			]
		]
	];

	ControlSheetGroup.AddPropertyRow(CachedButtonsProperty.ToSharedRef()).IsEnabled(false);
	ControlSheetGroup.AddPropertyRow(CachedSticksProperty.ToSharedRef()).IsEnabled(false);
	ControlSheetGroup.AddPropertyRow(CachedScenesProperty.ToSharedRef()).IsEnabled(false);


	TSharedRef<IPropertyHandle> CustomEventsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMixerInteractivitySettings, CustomGlobalEvents));
	CustomEventsProperty->SetOnPropertyValuePreChange(FSimpleDelegate::CreateSP(this, &FMixerInteractivitySettingsCustomization::OnCustomGlobalEventsPreChange));
	CustomEventsProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FMixerInteractivitySettingsCustomization::OnCustomGlobalEventsPostChange));

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

EVisibility FMixerInteractivitySettingsCustomization::GetLoginButtonVisibility() const
{
	return IMixerInteractivityModule::Get().GetLoginState() == EMixerLoginState::Not_Logged_In ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FMixerInteractivitySettingsCustomization::GetChangeUserVisibility() const
{
	return IMixerInteractivityModule::Get().GetLoginState() == EMixerLoginState::Logged_In ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FMixerInteractivitySettingsCustomization::GetLoginInProgressVisibility() const
{
	switch (IMixerInteractivityModule::Get().GetLoginState())
	{
	case EMixerLoginState::Logging_In:
	case EMixerLoginState::Logging_Out:
		return EVisibility::Visible;

	default:
		return EVisibility::Collapsed;
	}
}


FText FMixerInteractivitySettingsCustomization::GetCurrentUserText() const
{
	TSharedPtr<const FMixerLocalUser> CurrentUser = IMixerInteractivityModule::Get().GetCurrentUser();
	switch (IMixerInteractivityModule::Get().GetLoginState())
	{
	case EMixerLoginState::Logged_In:
		check(CurrentUser.IsValid());
		return FText::FormatOrdered(LOCTEXT("CurrentUserFormatString", "Logged in as {0}"), FText::FromString(CurrentUser->Name));
	case EMixerLoginState::Logging_In:
	case EMixerLoginState::Logging_Out:
		return LOCTEXT("LogInOutInProgress", "Working on it...");
	case EMixerLoginState::Not_Logged_In:
		if (GetLoginButtonEnabledState())
		{
			return LOCTEXT("NoCurrentUser", "Not logged in");
		}
		else
		{
			return LOCTEXT("MissingLoginInfo", "Client Id and Redirect Uri required.");
		}

	default:
		return FText::GetEmpty();
	}
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
	if (Success)
	{
		InteractiveGames.Empty(Games.Num());
		for (const FMixerInteractiveGame& Game : Games)
		{
			InteractiveGames.Add(MakeShareable(new FMixerInteractiveGame(Game)));
		}
		GameNamesBox->RefreshOptions();

		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		for (TSharedPtr<FMixerInteractiveGame> Game : InteractiveGames)
		{
			if (Game->Name == Settings->GameName)
			{
				GameNamesBox->SetSelectedItem(Game);
				break;
			}
		}
	}
}

bool FMixerInteractivitySettingsCustomization::GetEnabledStateRequiresLogin()
{
	return IMixerInteractivityModule::Get().GetLoginState() == EMixerLoginState::Logged_In;
}

void FMixerInteractivitySettingsCustomization::OnGameSelectionChanged(TSharedPtr<FMixerInteractiveGame> Game, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ChangedProperty)
{
	if (Game.IsValid())
	{
		ChangedProperty->SetValue(Game->Name);
		InteractiveVersions.Empty(Game->Versions.Num());
		for (const FMixerInteractiveGameVersion& Version : Game->Versions)
		{
			InteractiveVersions.Add(MakeShareable(new FMixerInteractiveGameVersion(Version)));
		}
		GameVersionsBox->RefreshOptions();
		GameVersionsBox->ClearSelection();

		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		for (TSharedPtr<FMixerInteractiveGameVersion> Version : InteractiveVersions)
		{
			if (Version->Id == Settings->GameVersionId)
			{
				GameVersionsBox->SetSelectedItem(Version);
				break;
			}
		}
	}
}

void FMixerInteractivitySettingsCustomization::OnVersionSelectionChanged(TSharedPtr<FMixerInteractiveGameVersion> Version, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ChangedProperty)
{
	if (Version.IsValid())
	{
		ChangedProperty->SetValue(Version->Id);
		IMixerInteractivityEditorModule::Get().RequestInteractiveControlsForGameVersion(*Version, FOnMixerInteractiveControlsRequestFinished::CreateSP(this, &FMixerInteractivitySettingsCustomization::OnInteractiveControlsForVersionRequestFinished));
	}
}

void FMixerInteractivitySettingsCustomization::OnInteractiveControlsForVersionRequestFinished(bool Success, const FMixerInteractiveGameVersion& VersionWithControls)
{
	if (Success)
	{
		OnlineSceneNames.Empty();
		OnlineButtonNames.Empty();
		OnlineJoystickNames.Empty();
		for (const FMixerInteractiveScene& Scene : VersionWithControls.Controls.Scenes)
		{
			OnlineSceneNames.Add(FName(*Scene.Id));

			for (const FMixerInteractiveControl& Control : Scene.Controls)
			{
				if (Control.IsButton())
				{
					OnlineButtonNames.Add(FName(*Control.Id));
				}
				else if (Control.IsJoystick())
				{
					OnlineJoystickNames.Add(FName(*Control.Id));
				}
			}
		}
	}
}

EVisibility FMixerInteractivitySettingsCustomization::GetUpdateControlSheetFromOnlineVisibility() const
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	if (Settings->CachedButtons != OnlineButtonNames ||
		Settings->CachedSticks != OnlineJoystickNames ||
		Settings->CachedScenes != OnlineSceneNames)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FText FMixerInteractivitySettingsCustomization::GetUpdateControlSheetFromOnlineInfoText() const
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	check(Settings);
	if (Settings->CachedButtons.Num() > 0 || Settings->CachedScenes.Num() > 0 || Settings->CachedSticks.Num() > 0)
	{
		return LOCTEXT("ControlSheetNeedsUpdate", "The controls for the selected interactive game and version are different to the local saved version.");
	}
	else
	{
		return LOCTEXT("ControlSheetMissing", "You can save information about the current interactivity controls to your game config file.  This will allow you to work with Mixer controls in Blueprint, even when not signed in to Mixer.");
	}
}

FText FMixerInteractivitySettingsCustomization::GetUpdateControlSheetFromOnlineButtonText() const
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	check(Settings);
	if (Settings->CachedButtons.Num() > 0 || Settings->CachedScenes.Num() > 0 || Settings->CachedSticks.Num() > 0)
	{
		return LOCTEXT("ControlSheetUpdateNow", "Update now");
	}
	else
	{
		return LOCTEXT("ControlSheetSaveLocal", "Save local...");
	}
}

FReply FMixerInteractivitySettingsCustomization::UpdateControlSheetFromOnline()
{
	UMixerInteractivitySettings* Settings = GetMutableDefault<UMixerInteractivitySettings>();

	CachedButtonsProperty->NotifyPreChange();
	CachedScenesProperty->NotifyPreChange();
	CachedSticksProperty->NotifyPreChange();
	Settings->CachedButtons = OnlineButtonNames;
	Settings->CachedScenes = OnlineSceneNames;
	Settings->CachedSticks = OnlineJoystickNames;
	CachedButtonsProperty->NotifyPostChange();
	CachedScenesProperty->NotifyPostChange();
	CachedSticksProperty->NotifyPostChange();

	IMixerInteractivityEditorModule::Get().RefreshDesignTimeObjects();

	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerButton::StaticClass());
	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerStickEvent::StaticClass());

	return FReply::Handled();
}

TSharedRef<SWidget> FMixerInteractivitySettingsCustomization::GenerateWidgetForGameComboRow(TSharedPtr<FMixerInteractiveGame> Game)
{
	return
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(FText::FromString(Game->Name));
}

TSharedRef<SWidget> FMixerInteractivitySettingsCustomization::GenerateWidgetForVersionComboRow(TSharedPtr<FMixerInteractiveGameVersion> Version)
{
	return
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(FText::FromString(Version->Name));
}

FText FMixerInteractivitySettingsCustomization::GetSelectedGameComboText() const
{
	if (GameNamesBox.IsValid())
	{
		TSharedPtr<FMixerInteractiveGame> SelectedGame = GameNamesBox->GetSelectedItem();
		if (SelectedGame.IsValid())
		{
			return FText::FromString(SelectedGame->Name);
		}
	}

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	return FText::FromString(Settings->GameName);
}

FText FMixerInteractivitySettingsCustomization::GetSelectedVersionComboText() const
{
	if (GameVersionsBox.IsValid())
	{
		TSharedPtr<FMixerInteractiveGameVersion> SelectedVersion = GameVersionsBox->GetSelectedItem();
		if (SelectedVersion.IsValid())
		{
			return FText::FromString(SelectedVersion->Name);
		}
	}

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	return FText::Format(LOCTEXT("UnknownGameVersion", "Unknown (id: {0})"), FText::AsNumber(Settings->GameVersionId));
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
			.OptionsSource(&IMixerInteractivityEditorModule::Get().GetDesignTimeScenes())
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

void FMixerInteractivitySettingsCustomization::OnCustomGlobalEventsPreChange()
{
	UClass* CustomGlobalEvents = GetMutableDefault<UMixerInteractivitySettings>()->CustomGlobalEvents;
	if (CustomGlobalEvents != nullptr)
	{
		UBlueprint* CustomEventsBlueprint = Cast<UBlueprint>(CustomGlobalEvents->ClassGeneratedBy);
		if (CustomEventsBlueprint != nullptr)
		{
			CustomEventsBlueprint->OnCompiled().RemoveAll(this);
		}
	}
}

void FMixerInteractivitySettingsCustomization::OnCustomGlobalEventsPostChange()
{
	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerCustomGlobalEvent::StaticClass());

	UClass* CustomGlobalEvents = GetMutableDefault<UMixerInteractivitySettings>()->CustomGlobalEvents;
	if (CustomGlobalEvents != nullptr)
	{
		UBlueprint* CustomEventsBlueprint = Cast<UBlueprint>(CustomGlobalEvents->ClassGeneratedBy);
		if (CustomEventsBlueprint != nullptr)
		{
			CustomEventsBlueprint->OnCompiled().AddLambda(
				[](UBlueprint*)
			{
				FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_MixerCustomGlobalEvent::StaticClass());
			});
		}
	}
}

#undef LOCTEXT_NAMESPACE
