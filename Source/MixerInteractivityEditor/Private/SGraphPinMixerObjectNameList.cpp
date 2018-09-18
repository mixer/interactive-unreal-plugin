//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "SGraphPinMixerObjectNameList.h"
#include "Editor/UnrealEd/Public/ScopedTransaction.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SErrorHint.h"
#include "MixerInteractivityEditorModule.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

void SGraphPinMixerObjectNameList::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, FGetMixerObjectNames InItemsSource)
{
	ItemsSource = InItemsSource;
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinMixerObjectNameList::GetDefaultValueWidget()
{
	TSharedPtr<FName> CurrentlySelectedName;

	const TArray<TSharedPtr<FName>>* NameList = &ItemsSource.Execute();
	if (GraphPinObj)
	{
		// Preserve previous selection, or set to first in list
		FName PreviousSelection = FName(*GraphPinObj->GetDefaultAsString());
		if (NameList)
		{
			for (TSharedPtr<FName> ListNamePtr : *NameList)
			{
				if (PreviousSelection == *ListNamePtr.Get())
				{
					CurrentlySelectedName = ListNamePtr;
					break;
				}
			}
		}

		if (!CurrentlySelectedName.IsValid())
		{
			CurrentlySelectedName = MakeShareable(new FName(PreviousSelection));
		}
	}

	// Create widget
	TSharedRef<SWidget> ValueWidget =
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(ComboBox, SComboBox<TSharedPtr<FName>>)
			.ContentPadding(FMargin(6.0f, 2.0f))
			.OptionsSource(NameList)
			.InitiallySelectedItem(CurrentlySelectedName)
			.OnSelectionChanged(this, &SGraphPinMixerObjectNameList::ComboBoxSelectionChanged)
			.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
			.OnGenerateWidget(this, &SGraphPinMixerObjectNameList::GenerateWidgetForComboItem)
			[
				SNew(STextBlock)
				.Text(this, &SGraphPinMixerObjectNameList::GetTextForSelectedItem)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SErrorHint)
			.ErrorText(LOCTEXT("ObjectNameNotFoundError", "Mixer object not found.  Interactive game definition may have been updated.  Check Mixer Interactivity settings."))
			.Visibility(this, &SGraphPinMixerObjectNameList::GetErrorVisibility)
		];

	IMixerInteractivityEditorModule::Get().OnDesignTimeObjectsChanged().AddSP(this, &SGraphPinMixerObjectNameList::RefreshOptions);

	return ValueWidget;
}

void SGraphPinMixerObjectNameList::ComboBoxSelectionChanged(TSharedPtr<FName> NameItem, ESelectInfo::Type SelectInfo)
{
	FName Name = NameItem.IsValid() ? *NameItem : NAME_None;
	if (auto Schema = (GraphPinObj ? GraphPinObj->GetSchema() : NULL))
	{
		FString NameAsString = Name.ToString();
		if (GraphPinObj->GetDefaultAsString() != NameAsString)
		{
			const FScopedTransaction Transaction(NSLOCTEXT("GraphEditor", "ChangeNameListPinValue", "Change Name List Pin Value"));
			GraphPinObj->Modify();

			Schema->TrySetDefaultValue(*GraphPinObj, NameAsString);
		}
	}
}

TSharedRef<SWidget> SGraphPinMixerObjectNameList::GenerateWidgetForComboItem(TSharedPtr<FName> Name)
{
	return
		SNew(STextBlock)
		.Text(FText::FromName(*Name));
}

FText SGraphPinMixerObjectNameList::GetTextForSelectedItem() const
{
	return FText::FromString(GraphPinObj->GetDefaultAsString());
}

EVisibility SGraphPinMixerObjectNameList::GetErrorVisibility() const
{
	if (IsConnected())
	{
		return EVisibility::Collapsed;
	}

	const TArray<TSharedPtr<FName>>* NameList = &ItemsSource.Execute();
	if (GraphPinObj && NameList)
	{
		FName PreviousSelection = FName(*GraphPinObj->GetDefaultAsString());
		for (TSharedPtr<FName> ListNamePtr : *NameList)
		{
			if (PreviousSelection == *ListNamePtr.Get())
			{
				return EVisibility::Collapsed;
			}
		}
	}

	return EVisibility::Visible;
}

void SGraphPinMixerObjectNameList::RefreshOptions()
{
	if (ComboBox.IsValid())
	{
		ComboBox->RefreshOptions();
	}
}

#undef LOCTEXT_NAMESPACE