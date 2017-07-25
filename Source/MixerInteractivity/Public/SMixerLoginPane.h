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

#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SCompoundWidget.h"

class FUniqueNetId;
class SWebBrowserView;
class IWebBrowserWindow;
class IWebBrowserPopupFeatures;
struct FWebNavigationRequest;
class SOverlay;
enum class EMixerLoginState : uint8;

DECLARE_DELEGATE_RetVal_OneParam(FReply, FOnMixerAuthCodeReady, const FString& /*AuthCode*/);
DECLARE_DELEGATE_OneParam(FOnMixerLoginUIFlowFinished, bool /*bSucceeded*/);

/**
* A widget that displays browser-based UI for signing into Mixer via OAuth
*/
class MIXERINTERACTIVITY_API SMixerLoginPane : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMixerLoginPane)
		: _UserId()
		, _BackgroundColor(0, 0, 0, 255)
		, _AllowSilentLogin(false)
	{}
		SLATE_ATTRIBUTE(TSharedPtr<const FUniqueNetId>, UserId)

		/** Whether to attempt automatic signin before displaying UI. */
		SLATE_ARGUMENT(bool, AllowSilentLogin)

		/** Background color for web browser control when no document color is specified. */
		SLATE_ARGUMENT(FColor, BackgroundColor)

		/** Called when an OAuth authorization code has been obtained (note: full login is not yet complete). */
		SLATE_EVENT(FOnMixerAuthCodeReady, OnAuthCodeReady)
		
		/** Called when the phase of login that requires UI is finished (note: full login is not yet complete). */
		SLATE_EVENT(FOnMixerLoginUIFlowFinished, OnUIFlowFinished)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void StartLoginFlow();
	void StopLoginFlowAndHide();

	virtual FVector2D ComputeDesiredSize(float) const override;

private:
	void OnLoginStateChanged(EMixerLoginState NewState);

	TAttribute<TSharedPtr<const FUniqueNetId>> BoundUserId;

#if PLATFORM_SUPPORTS_MIXER_OAUTH

	bool OnBrowserBeforeNavigation(const FString& NewUrlString, const FWebNavigationRequest& Request);
	bool OnBrowserPopupWindow(const TWeakPtr<IWebBrowserWindow>& NewBrowserWindow, const TWeakPtr<IWebBrowserPopupFeatures>& PopupFeatures, bool IsSecondaryPopup);
	bool OnBrowserRequestCloseBaseWindow(const TWeakPtr<IWebBrowserWindow>& BrowserWindowPtr);
	bool OnBrowserRequestClosePopupWindow(const TWeakPtr<IWebBrowserWindow>& BrowserWindowPtr);

	bool OnPopupBeforeNavigation(const FString& NewUrlString, const FWebNavigationRequest& Request, bool IsSecondaryPopup);

	void StartLoginFlowAfterCookiesDeleted();

	TSharedPtr<SOverlay> OverlayWidget;
	TSharedPtr<SWebBrowserView> BrowserWidget;
	FOnMixerAuthCodeReady OnAuthCodeReady;
	FOnMixerLoginUIFlowFinished OnUIFlowFinished;
	FColor BackgroundColor;
	bool bShowInitialThrobber;
#endif

	bool bAllowSilentLogin;
	bool bAttemptedSilentLogin;
};