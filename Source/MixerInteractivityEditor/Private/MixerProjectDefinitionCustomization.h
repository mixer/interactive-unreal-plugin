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

class FMixerProjectDefinitionCustomization : public IDetailCustomization
{
public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

private:
	const UClass* GetClassForCustomControl(TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName, FName SceneName) const;
	void SetClassForCustomControl(const UClass* NewClass, TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName, FName SceneName) const;
	void OnUseSelectedClicked(TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName, FName SceneName) const;
	void OnBrowseClicked(TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName, FName SceneName) const;
	void OnMakeNewClicked(TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName, FName SceneName) const;
	void OnClearClicked(TSharedRef<IPropertyHandle> ControlBindingProperty, FName ControlName, FName SceneName) const;
};