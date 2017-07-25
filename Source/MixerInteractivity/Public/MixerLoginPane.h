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

#include "SMixerLoginPane.h"
#include "Components/Widget.h"
#include "MixerLoginPane.generated.h"

class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMixerAuthCodeReadyEvent, const FString&, AuthCode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMixerLoginUIFlowFinishedEvent, bool, bSucceeded);

/**
* A widget that displays browser-based UI for signing into Mixer via OAuth
*/
UCLASS()
class MIXERINTERACTIVITY_API UMixerLoginPane : public UWidget
{
	GENERATED_BODY()

public:
	/** Background color for web browser control when no document color is specified. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mixer", meta=(HideAlphaChannel))
	FColor BackgroundColor;

	/** Whether to attempt automatic signin before displaying UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mixer")
	bool AllowSilentLogin;

public:
	/** Called when an OAuth authorization code has been obtained (note: full login is not yet complete). */
	UPROPERTY(BlueprintAssignable, Category = "Mixer|Event")
	FOnMixerAuthCodeReadyEvent OnAuthCodeReady;

	/** Called when the phase of login that requires UI is finished (note: full login is not yet complete). */
	UPROPERTY(BlueprintAssignable, Category = "Mixer|Event")
	FOnMixerLoginUIFlowFinishedEvent OnUIFlowFinished;

public:
	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

private:
	FReply SlateHandleAuthCodeReady(const FString& AuthCode);
	void SlateHandleUIFlowFinished(bool bSucceeded);

	TSharedPtr<const FUniqueNetId> SlateGetUserId() const;

private:
	TSharedPtr<SWidget> MyLoginPane;
};