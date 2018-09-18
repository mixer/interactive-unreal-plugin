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

#include "SGraphPin.h"
#include "Widgets/Input/SComboBox.h"

// Based on SGraphPinNameList, but with additions to allow refreshing the list
class SGraphPinMixerObjectNameList : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinMixerObjectNameList) {}
	SLATE_END_ARGS()

	DECLARE_DELEGATE_RetVal(const TArray<TSharedPtr<FName>>&, FGetMixerObjectNames);
	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, FGetMixerObjectNames InItemsSource);

protected:

	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;

	void ComboBoxSelectionChanged(TSharedPtr<FName> StringItem, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateWidgetForComboItem(TSharedPtr<FName> Name);
	FText GetTextForSelectedItem() const;
	EVisibility GetErrorVisibility() const;
	void RefreshOptions();

	TSharedPtr<SComboBox<TSharedPtr<FName>>> ComboBox;
	TWeakObjectPtr<UObject> ObjectType;

	FGetMixerObjectNames ItemsSource;
};