//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "MixerInteractivityTypes.h"
#include "MixerInteractivityJsonTypes.h"
#include "SComboBox.h"

struct FMixerInteractiveGame;
struct FMixerInteractiveGameVersion;

class FMixerInteractivitySettingsCustomization : public IDetailCustomization
{
public:
	virtual ~FMixerInteractivitySettingsCustomization();

	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

private:
	FMixerInteractivitySettingsCustomization();

	FReply Login();
	FReply ChangeUser();

	EVisibility GetLoginButtonVisibility() const;
	bool GetLoginButtonEnabledState() const;
	EVisibility GetChangeUserVisibility() const;
	EVisibility GetLoginInProgressVisibility() const;
	FText GetCurrentUserText() const;

	void OnLoginStateChanged(EMixerLoginState NewState);

	void OnInteractiveGamesRequestFinished(bool Success, const TArray<FMixerInteractiveGame>& Games);
	void OnInteractiveControlsForVersionRequestFinished(bool Success, const FMixerInteractiveGameVersion& VersionWithControls);

	bool GetEnabledStateRequiresLogin();

	void OnGameSelectionChanged(TSharedPtr<FMixerInteractiveGame> Game, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ChangedProperty);
	void OnVersionSelectionChanged(TSharedPtr<FMixerInteractiveGameVersion> Version, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ChangedProperty);

	void GenerateWidgetForDesignTimeGroupsElement(TSharedRef<IPropertyHandle> StructProperty, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder);
	void OnGroupInitialSceneSelectionChanged(TSharedPtr<FName> SceneName, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ChangedProperty);
	FText GetInitialSceneComboText(TSharedRef<IPropertyHandle> Property) const;
	EVisibility GetErrorVisibilityForGroup(int32 GroupElementIndex) const;

	TSharedRef<SWidget> GenerateWidgetForGameComboRow(TSharedPtr<FMixerInteractiveGame> Game);
	TSharedRef<SWidget> GenerateWidgetForVersionComboRow(TSharedPtr<FMixerInteractiveGameVersion> Version);
	TSharedRef<SWidget> GenerateWidgetForGroupInitialScene(TSharedPtr<FName> SceneName);
	FText GetSelectedGameComboText() const;
	FText GetSelectedVersionComboText() const;

	void UpdateOnlineInteractiveControlSheet(const FMixerInteractiveGameVersion& Version);

	EVisibility GetUpdateControlSheetFromOnlineVisibility() const;
	FText GetUpdateControlSheetFromOnlineInfoText() const;
	FText GetUpdateControlSheetFromOnlineButtonText() const;
	FReply UpdateControlSheetFromOnline();

	void OnCustomMethodsPreChange();
	void OnCustomMethodsPostChange();

private:
	FDelegateHandle LoginStateDelegateHandle;

	TArray<TSharedPtr<FMixerInteractiveGame>> InteractiveGames;
	TArray<TSharedPtr<FMixerInteractiveGameVersion>> InteractiveVersions;

	TSharedPtr<SComboBox<TSharedPtr<FMixerInteractiveGame>>> GameNamesBox;
	TSharedPtr<SComboBox<TSharedPtr<FMixerInteractiveGameVersion>>> GameVersionsBox;

	TSharedPtr<IPropertyHandle> CachedButtonsProperty;
	TSharedPtr<IPropertyHandle> CachedSticksProperty;
	TSharedPtr<IPropertyHandle> CachedScenesProperty;
	TSharedPtr<IPropertyHandle> DesignTimeGroupsProperty;

	TArray<FName> OnlineButtonNames;
	TArray<FName> OnlineJoystickNames;
	TArray<FName> OnlineSceneNames;
	TArray<TSharedPtr<FName>> OnlineSceneNamesForCombo;

	FMixerInteractiveGameVersion DownloadedProjectDefinition;

	bool IsChangingUser;
};