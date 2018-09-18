//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "SMixerLoginPane.h"

#include "MixerInteractivityModule.h"
#include "MixerInteractivityTypes.h"
#include "MixerInteractivitySettings.h"

#if PLATFORM_SUPPORTS_MIXER_OAUTH
#include "HttpModule.h"
#include "PlatformHttp.h"

#include "WebBrowserModule.h"
#include "IWebBrowserSingleton.h"
#include "IWebBrowserCookieManager.h"
#include "IWebBrowserWindow.h"
#include "SWebBrowserView.h"
#include "Widgets/SOverlay.h"
#include "HAL/PlatformProcess.h"
#include "MixerInteractivityLog.h"


static const FString EmptyUrl = TEXT("");
#endif

void SMixerLoginPane::Construct(const FArguments& InArgs)
{
	BoundUserId = InArgs._UserId;
	bAllowSilentLogin = InArgs._AllowSilentLogin;

#if PLATFORM_SUPPORTS_MIXER_OAUTH
	OnAuthCodeReady = InArgs._OnAuthCodeReady;
	OnUIFlowFinished = InArgs._OnUIFlowFinished;
	BackgroundColor = InArgs._BackgroundColor;

	ChildSlot
	[
		SAssignNew(OverlayWidget, SOverlay)
	];
#endif

	bAttemptedSilentLogin = false;
	IMixerInteractivityModule& InteractivityModule = IMixerInteractivityModule::Get();

	InteractivityModule.OnLoginStateChanged().AddSP(this, &SMixerLoginPane::OnLoginStateChanged);

	if (InteractivityModule.GetLoginState() == EMixerLoginState::Not_Logged_In)
	{
		StartLoginFlow();
	}
}

void SMixerLoginPane::StartLoginFlow()
{
	if (bAllowSilentLogin && !bAttemptedSilentLogin)
	{
		bAttemptedSilentLogin = true;
		if (IMixerInteractivityModule::Get().LoginSilently(BoundUserId.Get()))
		{
			return;
		}
	}

#if PLATFORM_SUPPORTS_MIXER_OAUTH
	if (!BrowserWidget.IsValid())
	{
		SAssignNew(BrowserWidget, SWebBrowserView)
			.ContentsToLoad(FString(TEXT("")))
			.InitialURL(EmptyUrl)
			.BackgroundColor(BackgroundColor)
			.SupportsTransparency(true)
			.OnBeforeNavigation(this, &SMixerLoginPane::OnBrowserBeforeNavigation)
			.OnCreateWindow(this, &SMixerLoginPane::OnBrowserPopupWindow, false)
			.OnCloseWindow(this, &SMixerLoginPane::OnBrowserRequestCloseBaseWindow);

		OverlayWidget->AddSlot()
		[
			BrowserWidget.ToSharedRef()
		];
	}

	BrowserWidget->LoadString(TEXT(""), TEXT(""));

	TWeakPtr<SMixerLoginPane> WeakThisForCallback = SharedThis(this);
	IWebBrowserModule::Get().GetSingleton()->GetCookieManager()->DeleteCookies(TEXT(""), TEXT(""),
		[WeakThisForCallback](int)
	{
		TSharedPtr<SMixerLoginPane> Pinned = WeakThisForCallback.Pin();
		if (Pinned.IsValid())
		{
			Pinned->StartLoginFlowAfterCookiesDeleted();
		}
	});
#else

#endif
}

void SMixerLoginPane::StopLoginFlowAndHide()
{
#if PLATFORM_SUPPORTS_MIXER_OAUTH
	if (BrowserWidget.IsValid())
	{
		OnUIFlowFinished.ExecuteIfBound(false);
		OverlayWidget->ClearChildren();
		BrowserWidget.Reset();
	}
#endif
	bAttemptedSilentLogin = false;
}

#if PLATFORM_SUPPORTS_MIXER_OAUTH

void SMixerLoginPane::StartLoginFlowAfterCookiesDeleted()
{
	if (BrowserWidget.IsValid())
	{
#if WITH_EDITOR
		static const FString OAuthScope = GIsEditor ? "interactive:manage:self interactive:robot:self chat:connect chat:chat chat:whisper" : "interactive:robot:self chat:connect chat:chat chat:whisper";
#else
		static const FString OAuthScope = "interactive:robot:self chat:connect chat:chat chat:whisper";
#endif

		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		FString OAuthUrl = FString::Printf(TEXT("https://mixer.com/oauth/authorize?redirect_uri=%s&client_id=%s&scope=%s&response_type=code&sandbox=%s"),
			*FPlatformHttp::UrlEncode(Settings->GetResolvedRedirectUri()), 
			*FPlatformHttp::UrlEncode(Settings->ClientId), 
			*FPlatformHttp::UrlEncode(OAuthScope), 
			*FPlatformHttp::UrlEncode(Settings->GetSandboxForOAuth()));
		BrowserWidget->LoadURL(OAuthUrl);
	}
}

bool SMixerLoginPane::OnBrowserBeforeNavigation(const FString& NewUrlString, const FWebNavigationRequest& Request)
{
	if (NewUrlString.IsEmpty() || !Request.bIsMainFrame || !BrowserWidget.IsValid())
	{
		return false;
	}

	bool IsAuthFlowFinished = false;
	bool IsAuthFlowSuccessful = false;
	FString AuthCode;

	// Check first for known done-with-OAuth query parameters.  This should cover us
	// in the case that the redirect uri is on mixer.com
	if (FParse::Value(*NewUrlString, TEXT("code="), AuthCode))
	{
		IsAuthFlowFinished = true;
		if (!AuthCode.IsEmpty())
		{
			// strip off any url parameters and just keep the token itself
			FString AuthCodeOnly;
			if (AuthCode.Split(TEXT("&"), &AuthCodeOnly, NULL))
			{
				AuthCode = AuthCodeOnly;
				IsAuthFlowSuccessful = true;
			}
		}
	}
	else if (FParse::Value(*NewUrlString, TEXT("error="), AuthCode))
	{
		IsAuthFlowFinished = true;
		IsAuthFlowSuccessful = false;
	}
	else
	{
		// Failsafe - if we've left mixer.com we're definitely finished.
		FString Protocol;
		FParse::SchemeNameFromURI(*NewUrlString, Protocol);
		FString UrlMinusProtocol = NewUrlString.RightChop(Protocol.Len() + 3);
		FString Host;
		FString PathAndQuery;

		UrlMinusProtocol.Split(TEXT("/"), &Host, &PathAndQuery);
		IsAuthFlowFinished = !Host.EndsWith(TEXT("mixer.com"));
		IsAuthFlowSuccessful = false;
	}

	if (IsAuthFlowFinished)
	{
		if (IsAuthFlowSuccessful)
		{
			FReply Reply = FReply::Unhandled();
			if (OnAuthCodeReady.IsBound())
			{
				Reply = OnAuthCodeReady.Execute(AuthCode);
			}
			
			if (!Reply.IsEventHandled())
			{
				IMixerInteractivityModule::Get().LoginWithAuthCode(AuthCode, BoundUserId.Get());
			}
			OnUIFlowFinished.ExecuteIfBound(true);
		}
		else
		{
			OnUIFlowFinished.ExecuteIfBound(false);
		}

		OverlayWidget->ClearChildren();
		BrowserWidget.Reset();
	}

	return false;
}

bool SMixerLoginPane::OnBrowserPopupWindow(const TWeakPtr<IWebBrowserWindow>& NewBrowserWindow, const TWeakPtr<IWebBrowserPopupFeatures>& PopupFeatures, bool IsSecondaryPopup)
{
	if (BrowserWidget.IsValid())
	{
		TSharedRef<SWebBrowserView> PopupBrowserWidget = SNew(SWebBrowserView, NewBrowserWindow.Pin())
			.OnCreateWindow(this, &SMixerLoginPane::OnBrowserPopupWindow, true)
			.OnBeforeNavigation(this, &SMixerLoginPane::OnPopupBeforeNavigation, IsSecondaryPopup)
			.BackgroundColor(BackgroundColor)
			.OnCloseWindow(this, &SMixerLoginPane::OnBrowserRequestClosePopupWindow);

		OverlayWidget->AddSlot()
		[
			PopupBrowserWidget
		];
		return true;
	}
	else
	{
		return false;
	}
}

bool SMixerLoginPane::OnBrowserRequestCloseBaseWindow(const TWeakPtr<IWebBrowserWindow>& BrowserWindowPtr)
{
	if (BrowserWidget.IsValid())
	{
		TSharedRef<SWidget> PinnedThis = AsShared();
		OnUIFlowFinished.ExecuteIfBound(false);
		StartLoginFlow();
	}
	return true;
}

bool SMixerLoginPane::OnBrowserRequestClosePopupWindow(const TWeakPtr<IWebBrowserWindow>& BrowserWindowPtr)
{
	if (BrowserWidget.IsValid() && OverlayWidget->GetNumWidgets() > 1)
	{
		// Assume it's the last one that needs to come off.  May need to be more thorough
		OverlayWidget->RemoveSlot();
		return true;
	}
	return false;
}

bool SMixerLoginPane::OnPopupBeforeNavigation(const FString& NewUrlString, const FWebNavigationRequest& Request, bool IsSecondaryPopup)
{
	if (Request.bIsMainFrame && (IsSecondaryPopup || !NewUrlString.Contains(TEXT("oauth"))))
	{
		// We've wandered out of the core oauth flow.  Break out into the platform browser if possible
		// and close things down - we don't want to end up wandering around the web inside this control
		if (PLATFORM_WINDOWS || FPlatformProcess::CanLaunchURL(*NewUrlString))
		{
			FPlatformProcess::LaunchURL(*NewUrlString, nullptr, nullptr);
		}
		OverlayWidget->RemoveSlot();
		return true;
	}
	return false;
}
#endif

FVector2D SMixerLoginPane::ComputeDesiredSize(float) const
{
	return FVector2D(350.0f, 500.0f);
}

void SMixerLoginPane::OnLoginStateChanged(EMixerLoginState NewState)
{
	switch (NewState)
	{
	case EMixerLoginState::Not_Logged_In:
		StartLoginFlow();
		break;

	case EMixerLoginState::Logged_In:
#if PLATFORM_SUPPORTS_MIXER_OAUTH
		OverlayWidget->ClearChildren();
		BrowserWidget.Reset();
#endif
		bAttemptedSilentLogin = false;
		break;

	default:
		break;
	}
}
