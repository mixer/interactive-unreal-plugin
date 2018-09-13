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
#include "Widgets/Input/SComboBox.h"

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
	enum class EGameBindingMethod
	{
		Manual,
		FromMixerUser
	};

private:
	FMixerInteractivitySettingsCustomization();

	FReply Login();
	FReply ChangeUser();

	bool GetLoginButtonEnabledState() const;
	FText GetCurrentUserText() const;

	void OnLoginStateChanged(EMixerLoginState NewState);

	void OnInteractiveGamesRequestFinished(bool Success, const TArray<FMixerInteractiveGame>& Games);
	void OnInteractiveControlsForVersionRequestFinished(bool Success, const FMixerInteractiveGameVersion& VersionWithControls);

	bool GetEnabledStateRequiresLogin() const;

	void OnGameBindingMethodChanged(ECheckBoxState NewCheckState, EGameBindingMethod ForBindingMethod);

	void GenerateWidgetForDesignTimeGroupsElement(TSharedRef<IPropertyHandle> StructProperty, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder);
	void OnGroupInitialSceneSelectionChanged(TSharedPtr<FName> SceneName, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ChangedProperty);
	FText GetInitialSceneComboText(TSharedRef<IPropertyHandle> Property) const;
	EVisibility GetErrorVisibilityForGroup(int32 GroupElementIndex) const;

	TSharedRef<SWidget> GenerateWidgetForGroupInitialScene(TSharedPtr<FName> SceneName);

	EVisibility GetLoginErrorVisibility() const;
	EVisibility GetUpdateControlSheetFromOnlineVisibility() const;
	EVisibility GetVersionMismatchErrorVisibility() const;
	EVisibility GetUpdateExistingControlSheetVisibility() const;
	EVisibility VisibleWhenGameBindingMethod(EGameBindingMethod InMethod) const;
	ECheckBoxState CheckedWhenGameBindingMethod(EGameBindingMethod InMethod) const;
	bool EnabledWhenGameBindingMethod(EGameBindingMethod InMethod) const;
	int32 GetWidgetIndexForLoginState() const;
	int32 GetWidgetIndexForGameBindingMethod() const;
	FText GetUpdateControlSheetFromOnlineInfoText() const;
	FReply UpdateControlSheetFromOnline();
	FReply CreateNewControlSheetFromOnline(TSharedRef<IPropertyHandle> ProjectDefinitionProperty);

	void OnCustomMethodsPreChange();
	void OnCustomMethodsPostChange();

	void GetContentForGameNameCombo(TArray<TSharedPtr<FString>>& OutStrings, TArray<TSharedPtr<SToolTip>>& OutTooltips, TArray<bool>& OutRestrictedItems) const;
	void GetContentForGameVersionCombo(TArray<TSharedPtr<FString>>& OutStrings, TArray<TSharedPtr<SToolTip>>& OutTooltips, TArray<bool>& OutRestrictedItems) const;
	FString GetGameVersionComboValue() const;
	void OnGameVersionComboValueSelected(const FString& SelectedString, TSharedRef<IPropertyHandle> GameVersionProperty);

private:
	FDelegateHandle LoginStateDelegateHandle;

	TArray<FMixerInteractiveGame> InteractiveGames;

	TSharedPtr<SComboBox<TSharedPtr<FMixerInteractiveGame>>> GameNamesBox;
	TSharedPtr<SComboBox<TSharedPtr<FMixerInteractiveGameVersion>>> GameVersionsBox;

	TSharedPtr<IPropertyHandle> DesignTimeGroupsProperty;

	TArray<TSharedPtr<FName>> OnlineSceneNamesForCombo;

	FMixerInteractiveGameVersion DownloadedProjectDefinition;
	EGameBindingMethod SelectedBindingMethod;

	bool IsChangingUser;
};