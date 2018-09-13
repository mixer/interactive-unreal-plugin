//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MixerLoginPane.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

void UMixerLoginPane::SynchronizeProperties()
{
	Super::SynchronizeProperties();
}

void UMixerLoginPane::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyLoginPane.Reset();
}

#if WITH_EDITOR
const FText UMixerLoginPane::GetPaletteCategory()
{
	return NSLOCTEXT("MixerInteractivity", "Mixer", "Mixer");
}
#endif

TSharedRef<SWidget> UMixerLoginPane::RebuildWidget()
{
	if (!IsDesignTime())
	{
		MyLoginPane = SNew(SMixerLoginPane)
			.UserId_UObject(this, &UMixerLoginPane::SlateGetUserId)
			.OnAuthCodeReady_UObject(this, &UMixerLoginPane::SlateHandleAuthCodeReady)
			.OnUIFlowFinished_UObject(this, &UMixerLoginPane::SlateHandleUIFlowFinished)
			.AllowSilentLogin(AllowSilentLogin)
			.BackgroundColor(BackgroundColor);
	}
	else
	{
		MyLoginPane = SNew(SBox)
			.WidthOverride(350.0f)
			.HeightOverride(500.0f)
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("MixerInteractivity", "Mixer_Login_Pane", "Mixer Login Pane"))
			];
	}

	return MyLoginPane.ToSharedRef();
}

FReply UMixerLoginPane::SlateHandleAuthCodeReady(const FString& AuthCode)
{
	OnAuthCodeReady.Broadcast(AuthCode);

	// Report unhandled so that Slate widget will continue with login process.
	return FReply::Unhandled();
}

void UMixerLoginPane::SlateHandleUIFlowFinished(bool bSucceeded)
{
	OnUIFlowFinished.Broadcast(bSucceeded);
}

TSharedPtr<const FUniqueNetId> UMixerLoginPane::SlateGetUserId() const
{
	TSharedPtr<const FUniqueNetId> NetId = nullptr;
	APlayerController* OwningController = GetOwningPlayer();
	if (OwningController != nullptr)
	{
		APlayerState* PlayerState = OwningController->PlayerState;
		if (PlayerState)
		{
			NetId = PlayerState->UniqueId.GetUniqueNetId();
		}
	}
	return NetId;
}
