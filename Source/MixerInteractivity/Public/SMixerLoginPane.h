#pragma once

#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SCompoundWidget.h"

class FUniqueNetId;
class SWebBrowser;
class IWebBrowserWindow;
class IWebBrowserPopupFeatures;
struct FWebNavigationRequest;
enum class EMixerLoginState : uint8;

DECLARE_DELEGATE_OneParam(FOnMixerAuthCodeReady, const FString& /*AuthCode*/);
DECLARE_DELEGATE_OneParam(FOnMixerLoginUIFlowFinished, bool /*bSucceeded*/);

class MIXERINTERACTIVITY_API SMixerLoginPane : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMixerLoginPane)
		: _UserId()
		, _BackgroundColor(0, 0, 0, 255)
		, _ShowInitialThrobber(false)
		, _AllowSilentLogin(false)
	{}
		SLATE_ATTRIBUTE(TSharedPtr<const FUniqueNetId>, UserId)

		SLATE_ARGUMENT(bool, AllowSilentLogin)

		SLATE_ARGUMENT(bool, ShowInitialThrobber)

		SLATE_ARGUMENT(FColor, BackgroundColor)

		SLATE_EVENT(FOnMixerAuthCodeReady, OnAuthCodeReady)
		
		SLATE_EVENT(FOnMixerLoginUIFlowFinished, OnUIFlowFinished)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void StartLoginFlow();
	void StopLoginFlowAndHide();

	virtual FVector2D ComputeDesiredSize(float) const override;

private:

	void OnLoginStateChanged(EMixerLoginState NewState);

	bool OnBrowserBeforeNavigation(const FString& NewUrlString, const FWebNavigationRequest& Request);
	bool OnBrowserPopupWindow(const TWeakPtr<IWebBrowserWindow>& NewBrowserWindow, const TWeakPtr<IWebBrowserPopupFeatures>& PopupFeatures);
	bool OnBrowserRequestCloseBaseWindow(const TWeakPtr<IWebBrowserWindow>& BrowserWindowPtr);
	bool OnBrowserRequestClosePopupWindow(const TWeakPtr<IWebBrowserWindow>& BrowserWindowPtr);

	void StartLoginFlowAfterCookiesDeleted();

	TSharedPtr<SOverlay> OverlayWidget;
	TSharedPtr<SWebBrowser> BrowserWidget;
	TAttribute<TSharedPtr<const FUniqueNetId>> BoundUserId;
	FOnMixerAuthCodeReady OnAuthCodeReady;
	FOnMixerLoginUIFlowFinished OnUIFlowFinished;
	FColor BackgroundColor;
	bool bShowInitialThrobber;
	bool bAllowSilentLogin;
	bool bAttemptedSilentLogin;
};