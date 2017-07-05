//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "MixerInteractivityModulePrivate.h"
#include "MixerInteractivitySettings.h"
#include "MixerInteractivityUserSettings.h"
#include "MixerDynamicDelegateBinding.h"
#include "MixerInteractivityLog.h"

#include "HttpModule.h"
#include "PlatformHttp.h"
#include "JsonTypes.h"
#include "JsonPrintPolicy.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UObjectGlobals.h"
#include "CoreOnline.h"
#include "Engine/World.h"
#include "Async.h"
#include "TabManager.h"
#include "SlateApplication.h"
#include "App.h"
#include "Engine/Engine.h"

#if PLATFORM_SUPPORTS_MIXER_OAUTH
#include "WebBrowserModule.h"
#include "IWebBrowserSingleton.h"
#include "IWebBrowserCookieManager.h"
#include "SWebBrowser.h"
#endif

#if PLATFORM_WINDOWS
#include "PreWindowsApi.h"
#define TV_API 0
#define CPPREST_FORCE_PPLX 0
#define XBOX_UWP 0
#elif PLATFORM_XBOXONE
#include "XboxOneAllowPlatformTypes.h"
#define TV_API 1
#endif
#define _TURN_OFF_PLATFORM_STRING
#define _NO_MIXERIMP
#pragma warning(push)
#pragma warning(disable:4628)
#include <interactivity_types.h>
#include <interactivity.h>
#pragma warning(pop)
#if PLATFORM_WINDOWS
#include "PostWindowsApi.h"
#elif PLATFORM_XBOXONE
#include "XboxOneHidePlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY(LogMixerInteractivity);

IMPLEMENT_MODULE(FMixerInteractivityModule, MixerInteractivity);

void FMixerInteractivityModule::StartupModule()
{
	UserAuthState = EMixerLoginState::Not_Logged_In;
	InteractivityState = EMixerInteractivityState::Not_Interactive;
	HasCreatedGroups = false;
}

bool FMixerInteractivityModule::LoginSilently(TSharedPtr<const FUniqueNetId> UserId)
{
	if (GetLoginState() != EMixerLoginState::Not_Logged_In)
	{
		return false;
	}

#if PLATFORM_XBOXONE
	if (!UserId.IsValid())
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("User id is required to login to Mixer on Xbox."));
		return false;
	}

	FString Xuid = UserId->ToString();
	UserAuthState = EMixerLoginState::Logging_In;

	// Go async to avoid blocking the game thread on the cross-OS call
	PlatformUser = Async<Windows::Xbox::System::User^>(EAsyncExecution::ThreadPool,
	[Xuid]() -> Windows::Xbox::System::User^
	{
		for (uint32 i = 0; i < Windows::Xbox::System::User::Users->Size; ++i)
		{
			Windows::Xbox::System::User^ PlatformUser = Windows::Xbox::System::User::Users->GetAt(i);
			if (PlatformUser != nullptr && Xuid == PlatformUser->XboxUserId->Data())
			{
				return PlatformUser;
			}
		}
		return nullptr;
	});
	return true;
#else
	if (UserAuthState == EMixerLoginState::Logged_In)
	{
		check(NeedsClientLibraryActive());

		// User is already logged in, but client library not initialized.
		// This case will occur during PIE when logging in for interactivity with the same Mixer user that owns the Editor settings.
		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
		Microsoft::mixer::interactivity_manager::get_singleton_instance()->set_oauth_token(*UserSettings->AccessToken);
		if (Microsoft::mixer::interactivity_manager::get_singleton_instance()->initialize(*FString::FromInt(Settings->GameVersionId), false))
		{
			OnLoginStateChanged().Broadcast(EMixerLoginState::Logging_In);
			return true;
		}
		else
		{
			return false;
		}
	}

	const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
	if (UserSettings->RefreshToken.IsEmpty())
	{
		return false;
	}

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();

	FString ContentString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&ContentString);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(TEXT("grant_type"), TEXT("refresh_token"));
	JsonWriter->WriteValue(TEXT("refresh_token"), UserSettings->RefreshToken);
	JsonWriter->WriteValue(TEXT("redirect_uri"), GetRedirectUri());
	JsonWriter->WriteValue(TEXT("client_id"), Settings->ClientId);
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	// Now exchange the auth code for a token
	TSharedRef<IHttpRequest> TokenRequest = FHttpModule::Get().CreateRequest();
	TokenRequest->SetVerb(TEXT("POST"));
	TokenRequest->SetURL(TEXT("https://mixer.com/api/v1/oauth/token"));
	TokenRequest->SetHeader(TEXT("content-type"), TEXT("application/json"));
	TokenRequest->SetContentAsString(ContentString);
	TokenRequest->OnProcessRequestComplete().BindRaw(this, &FMixerInteractivityModule::OnTokenRequestComplete);
	if (!TokenRequest->ProcessRequest())
	{
		return false;
	}

	NetId = UserId;
	UserAuthState = EMixerLoginState::Logging_In;
	OnLoginStateChanged().Broadcast(EMixerLoginState::Logging_In);

	return true;
#endif
}

bool FMixerInteractivityModule::LoginWithUI(TSharedPtr<const FUniqueNetId> UserId)
{
#if PLATFORM_SUPPORTS_MIXER_OAUTH
	if (GetLoginState() != EMixerLoginState::Not_Logged_In)
	{
		return false;
	}

	if (RetryLoginWithUI)
	{
		// This is already a retry attempt.  Don't allow another.
		RetryLoginWithUI = false;
	}
	else if (LoginSilently(UserId))
	{
		RetryLoginWithUI = true;
		return true;
	}

#if WITH_EDITOR
	static const FString OAuthScope = "interactive:manage:self interactive:robot:self";
#else
	static const FString OAuthScope = "interactive:robot:self user:details:self";
#endif

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	FString Command = FString::Printf(TEXT("https://mixer.com/oauth/authorize?redirect_uri=%s&client_id=%s&scope=%s&response_type=code"),
		*FPlatformHttp::UrlEncode(GetRedirectUri()), *FPlatformHttp::UrlEncode(Settings->ClientId), *FPlatformHttp::UrlEncode(OAuthScope));

	const FText TitleText = NSLOCTEXT("MixerInteractivity", "LoginWindowTitle", "Login to Mixer");
	LoginWindow = SNew(SWindow)
		.Title(TitleText)
		.SizingRule(ESizingRule::FixedSize)
		.ClientSize(FVector2D(500.f, 500.f))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	LoginWindow->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &FMixerInteractivityModule::OnBrowserWindowClosed));

	TSharedRef<SWebBrowser> BrowserWidget = SNew(SWebBrowser)
		.InitialURL(Command)
		.ShowControls(false)
		.ShowAddressBar(false)
		.OnUrlChanged_Raw(this, &FMixerInteractivityModule::OnBrowserUrlChanged)
		.OnCreateWindow_Raw(this, &FMixerInteractivityModule::OnBrowserPopupWindow);

	SAssignNew(LoginBrowserPanes, SOverlay) +
		SOverlay::Slot()
		[
			BrowserWidget
		];

	LoginWindow->SetContent(LoginBrowserPanes.ToSharedRef());
	TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (RootWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(LoginWindow.ToSharedRef(), RootWindow.ToSharedRef());
		//AddModalWindow
	}
	else
	{
		FSlateApplication::Get().AddWindow(LoginWindow.ToSharedRef());
	}

	NetId = UserId;
	UserAuthState = EMixerLoginState::Logging_In;
	OnLoginStateChanged().Broadcast(EMixerLoginState::Logging_In);

	return true;
#else
	UE_LOG(LogMixerInteractivity, Warning, TEXT("OAuth login flow not supported on this platform."));
	return false;

#endif
}

bool FMixerInteractivityModule::LoginWithAuthCode(const FString& AuthCode, TSharedPtr<const FUniqueNetId> UserId)
{
#if PLATFORM_SUPPORTS_MIXER_OAUTH
	if (GetLoginState() != EMixerLoginState::Not_Logged_In)
	{
		return false;
	}

	if (LoginWithAuthCodeInternal(AuthCode, UserId))
	{
		NetId = UserId;
		UserAuthState = EMixerLoginState::Logging_In;
		OnLoginStateChanged().Broadcast(EMixerLoginState::Logging_In);
		return true;
	}
	else
	{
		return false;
	}
#else
	UE_LOG(LogMixerInteractivity, Warning, TEXT("OAuth login flow not supported on this platform."));
	return false;
#endif
}

bool FMixerInteractivityModule::Logout()
{
	StopInteractivity();
	CurrentUser.Reset();
	NetId.Reset();
#if PLATFORM_SUPPORTS_MIXER_OAUTH
	UMixerInteractivityUserSettings* UserSettings = GetMutableDefault<UMixerInteractivityUserSettings>();
	UserSettings->AccessToken = TEXT("");
	UserSettings->RefreshToken = TEXT("");
	UserSettings->SaveConfig();
	UserAuthState = EMixerLoginState::Logging_Out;
	OnLoginStateChanged().Broadcast(EMixerLoginState::Logging_Out);
	IWebBrowserModule::Get().GetSingleton()->GetCookieManager()->DeleteCookies(TEXT(""), TEXT(""),
		[this](int)
	{
		UserAuthState = EMixerLoginState::Not_Logged_In;
		OnLoginStateChanged().Broadcast(EMixerLoginState::Not_Logged_In);
	});
#else
	UserAuthState = EMixerLoginState::Not_Logged_In;
	OnLoginStateChanged().Broadcast(EMixerLoginState::Not_Logged_In);
#endif

	return true;
}

EMixerLoginState FMixerInteractivityModule::GetLoginState()
{
	switch (UserAuthState)
	{
	case EMixerLoginState::Logged_In:
		check(CurrentUser.IsValid());
		if (NeedsClientLibraryActive())
		{
			switch (Microsoft::mixer::interactivity_manager::get_singleton_instance()->interactivity_state())
			{
			case Microsoft::mixer::not_initialized:
				return EMixerLoginState::Not_Logged_In;

			case Microsoft::mixer::initializing:
				return EMixerLoginState::Logging_In;

			case Microsoft::mixer::interactivity_disabled:
			case Microsoft::mixer::interactivity_enabled:
			case Microsoft::mixer::interactivity_pending:
				return EMixerLoginState::Logged_In;

			default:
				// Internal error in Mixer client library state management
				check(false);
				return EMixerLoginState::Not_Logged_In;
			}
		}
		else
		{
			return EMixerLoginState::Logged_In;
		}
		break;

	case EMixerLoginState::Logging_In:
	case EMixerLoginState::Logging_Out:
	case EMixerLoginState::Not_Logged_In:
		return UserAuthState;

	default:
		// Internal error in Mixer plugin state management
		check(false);
		return EMixerLoginState::Not_Logged_In;
	}
}

#if PLATFORM_SUPPORTS_MIXER_OAUTH
void FMixerInteractivityModule::OnBrowserUrlChanged(const FText& NewUrl)
{
	FString NewUrlString = NewUrl.ToString();
	FString Protocol;
	FParse::SchemeNameFromURI(*NewUrlString, Protocol);
	NewUrlString = NewUrlString.RightChop(Protocol.Len() + 3);
	FString Host;
	FString PathAndQuery;
	NewUrlString.Split(TEXT("/"), &Host, &PathAndQuery);

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	if (!Host.EndsWith(TEXT("mixer.com")))
	{
		FString AuthCode;
		if (FParse::Value(*PathAndQuery, TEXT("code="), AuthCode) &&
			!AuthCode.IsEmpty())
		{
			// strip off any url parameters and just keep the token itself
			FString AuthCodeOnly;
			if (AuthCode.Split(TEXT("&"), &AuthCodeOnly, NULL))
			{
				AuthCode = AuthCodeOnly;

				if (!LoginWithAuthCodeInternal(AuthCode, NetId))
				{
					LoginAttemptFinished(false);
				}
			}
		}

		LoginWindow->SetOnWindowClosed(FOnWindowClosed());
		TSharedRef<SWindow> WindowToClose = LoginWindow.ToSharedRef();
		LoginWindow.Reset();
		LoginBrowserPanes.Reset();
		FSlateApplication::Get().RequestDestroyWindow(WindowToClose);
	}
}

bool FMixerInteractivityModule::OnBrowserPopupWindow(const TWeakPtr<IWebBrowserWindow>& NewBrowserWindow, const TWeakPtr<IWebBrowserPopupFeatures>& PopupFeatures)
{
	TSharedRef<SWebBrowser> BrowserWidget = SNew(SWebBrowser, NewBrowserWindow.Pin())
		.ShowControls(false)
		.ShowAddressBar(false)
		.OnCloseWindow_Lambda([this](const TWeakPtr<IWebBrowserWindow>& BrowserWindowPtr)
	{
		// Assume it's the last one that needs to come off.  May need to be more thorough
		LoginBrowserPanes->RemoveSlot();
		return true;
	});

	LoginBrowserPanes->AddSlot()
		[
			BrowserWidget
		];

	return true;
}

#endif

void FMixerInteractivityModule::OnBrowserWindowClosed(const TSharedRef<SWindow>&)
{
	if (LoginWindow.IsValid())
	{
		// Closed before we were done.
		LoginAttemptFinished(false);
	}
}

bool FMixerInteractivityModule::Tick(float DeltaTime)
{
#if PLATFORM_XBOXONE
	if (UserAuthState == EMixerLoginState::Logging_In)
	{
		if (PlatformUser.IsReady())
		{
			Windows::Xbox::System::User^ ResolvedUser = PlatformUser.Get();
			if (ResolvedUser != nullptr)
			{
				CurrentUser = MakeShareable(new FMixerLocalUserJsonSerializable());
				
				// Not good, could be cross-OS too if this info has changed.
				CurrentUser->Name = ResolvedUser->DisplayInfo->GameDisplayName->Data();
				UserAuthState = EMixerLoginState::Logged_In;
				Microsoft::mixer::interactivity_manager::get_singleton_instance()->set_local_user(PlatformUser.Get());
				const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
				if (!Microsoft::mixer::interactivity_manager::get_singleton_instance()->initialize(*FString::FromInt(Settings->GameVersionId), false))
				{
					LoginAttemptFinished(false);
				}
			}
			else
			{
				LoginAttemptFinished(false);
			}
		}
	}
#endif

	TickParticipantCacheMaintenance();
	TickClientLibrary();

	return true;
}

void FMixerInteractivityModule::TickParticipantCacheMaintenance()
{
	static const FTimespan IntervalForCacheFreshness = FTimespan::FromSeconds(30.0);
	FDateTime TimeNow = FDateTime::Now();
	for (TMap<uint32, TSharedPtr<FMixerRemoteUserCached>>::TIterator It(RemoteParticipantCache); It; ++It)
	{
		FDateTime MostRecentInteraction = FMath::Max(It.Value()->ConnectedAt, It.Value()->InputAt);
		if (!It.Value().IsUnique() || TimeNow - MostRecentInteraction < IntervalForCacheFreshness)
		{
			It.Value()->UpdateFromSourceParticipant();
		}
		else
		{
			It.RemoveCurrent();
		}
	}
}

void FMixerInteractivityModule::TickClientLibrary()
{
	using namespace Microsoft::mixer;

	if (!NeedsClientLibraryActive())
	{
		// Should really be Un-init if possible
		StopInteractivity();

		HasCreatedGroups = false;
	}

	std::vector<interactive_event> EventsThisFrame = interactivity_manager::get_singleton_instance()->do_work();
	for (auto& MixerEvent : EventsThisFrame)
	{
		switch (MixerEvent.event_type())
		{
		case interactive_event_type::error:
			switch (GetLoginState())
			{
			case EMixerLoginState::Logging_In:
				LoginAttemptFinished(false);
				break;

			case EMixerLoginState::Logged_In:
				Logout();
				break;

			default:
				break;
			}
			break;

		case interactive_event_type::interactivity_state_changed:
		{
			auto StateChangeArgs = std::static_pointer_cast<interactivity_state_change_event_args>(MixerEvent.event_args());
			switch (StateChangeArgs->new_state())
			{
			case interactivity_state::not_initialized:
				InteractivityState = EMixerInteractivityState::Not_Interactive;
				switch (GetLoginState())
				{
				case EMixerLoginState::Logging_In:
					LoginAttemptFinished(false);
					break;

				case EMixerLoginState::Logged_In:
					Logout();
					break;

				default:
					break;
				}
				break;

			case interactivity_state::initializing:
				InteractivityState = EMixerInteractivityState::Not_Interactive;
				// Ensure the default group has a non-null representation
				CreateGroup(NAME_DefaultMixerParticipantGroup);
				break;

			case interactivity_state::interactivity_pending:
				if (GetLoginState() == EMixerLoginState::Logging_In)
				{
					LoginAttemptFinished(true);
				}
				if (!HasCreatedGroups)
				{
					InitDesignTimeGroups();
				}
				break;

			case interactivity_state::interactivity_disabled:
				InteractivityState = EMixerInteractivityState::Not_Interactive;
				if (GetLoginState() == EMixerLoginState::Logging_In)
				{
					LoginAttemptFinished(true);
				}
				if (!HasCreatedGroups)
				{
					InitDesignTimeGroups();
				}
				break;

			case interactivity_state::interactivity_enabled:
				InteractivityState = EMixerInteractivityState::Interactive;
				if (GetLoginState() == EMixerLoginState::Logging_In)
				{
					LoginAttemptFinished(true);
				}
				if (!HasCreatedGroups)
				{
					InitDesignTimeGroups();
				}
				break;
			}
		}
		break;

		case interactive_event_type::participant_state_changed:
		{
			auto ParticipantEventArgs = std::static_pointer_cast<interactive_participant_state_change_event_args>(MixerEvent.event_args());
			TSharedPtr<const FMixerRemoteUser> RemoteParticipant = CreateOrUpdateCachedParticipant(ParticipantEventArgs->participant());
			switch (ParticipantEventArgs->state())
			{
			case interactive_participant_state::joined:
				ParticipantStateChanged.Broadcast(RemoteParticipant, EMixerInteractivityParticipantState::Joined);
				break;

			case interactive_participant_state::left:
				ParticipantStateChanged.Broadcast(RemoteParticipant, EMixerInteractivityParticipantState::Left);
				break;

			case interactive_participant_state::input_disabled:
				ParticipantStateChanged.Broadcast(RemoteParticipant, EMixerInteractivityParticipantState::Input_Disabled);
				break;

			default:
				break;
			}
		}
		break;

		case interactive_event_type::button:
		{
			auto OriginalButtonArgs = std::static_pointer_cast<interactive_button_event_args>(MixerEvent.event_args());
			TSharedPtr<const FMixerRemoteUser> RemoteParticipant = CreateOrUpdateCachedParticipant(OriginalButtonArgs->participant());
			ButtonEvent.Broadcast(FName(OriginalButtonArgs->control_id().c_str()), RemoteParticipant, OriginalButtonArgs->is_pressed());
		}
		break;

		case interactive_event_type::joystick:
		{
			auto OriginalStickArgs = std::static_pointer_cast<interactive_joystick_event_args>(MixerEvent.event_args());
			TSharedPtr<const FMixerRemoteUser> RemoteParticipant = CreateOrUpdateCachedParticipant(OriginalStickArgs->participant());
			StickEvent.Broadcast(FName(OriginalStickArgs->control_id().c_str()), RemoteParticipant, FVector2D(OriginalStickArgs->x(), OriginalStickArgs->y()));
			break;
		}

		default:
			break;
		}
	}
}

bool FMixerInteractivityModule::LoginWithAuthCodeInternal(const FString& AuthCode, TSharedPtr<const FUniqueNetId> UserId)
{
	check(PLATFORM_SUPPORTS_MIXER_OAUTH);

#if PLATFORM_SUPPORTS_MIXER_OAUTH

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	FString ContentString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&ContentString);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(TEXT("grant_type"), TEXT("authorization_code"));
	JsonWriter->WriteValue(TEXT("redirect_uri"), GetRedirectUri());
	JsonWriter->WriteValue(TEXT("client_id"), Settings->ClientId);
	JsonWriter->WriteValue(TEXT("code"), AuthCode);
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	// Now exchange the auth code for a token
	TSharedRef<IHttpRequest> TokenRequest = FHttpModule::Get().CreateRequest();
	TokenRequest->SetVerb(TEXT("POST"));
	TokenRequest->SetURL(TEXT("https://mixer.com/api/v1/oauth/token"));
	TokenRequest->SetHeader(TEXT("content-type"), TEXT("application/json"));
	TokenRequest->SetContentAsString(ContentString);
	TokenRequest->OnProcessRequestComplete().BindRaw(this, &FMixerInteractivityModule::OnTokenRequestComplete);
	if (!TokenRequest->ProcessRequest())
	{
		return false;
	}

	return true;
#else
	return false;
#endif
}

void FMixerInteractivityModule::OnTokenRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	check(PLATFORM_SUPPORTS_MIXER_OAUTH);

#if PLATFORM_SUPPORTS_MIXER_OAUTH
	bool GotAccessToken = false;
	if (bSucceeded && HttpResponse.IsValid())
	{
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			FString ResponseStr = HttpResponse->GetContentAsString();
			TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ResponseStr);
			TSharedPtr<FJsonObject> JsonObject;
			if (FJsonSerializer::Deserialize(JsonReader, JsonObject) &&
				JsonObject.IsValid())
			{
				UMixerInteractivityUserSettings* UserSettings = GetMutableDefault<UMixerInteractivityUserSettings>();

				GotAccessToken = true;
				GotAccessToken &= JsonObject->TryGetStringField(TEXT("access_token"), UserSettings->AccessToken);
				GotAccessToken &= JsonObject->TryGetStringField(TEXT("refresh_token"), UserSettings->RefreshToken);
				if (GotAccessToken)
				{
					UserSettings->SaveConfig();
				}
			}
		}
	}

	if (GotAccessToken)
	{
		// Now get user info
		const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
		FString AuthZHeaderValue = FString::Printf(TEXT("Bearer %s"), *UserSettings->AccessToken);
		TSharedRef<IHttpRequest> UserRequest = FHttpModule::Get().CreateRequest();
		UserRequest->SetVerb(TEXT("GET"));
		UserRequest->SetURL(TEXT("https://mixer.com/api/v1/users/current"));
		UserRequest->SetHeader(TEXT("Authorization"), AuthZHeaderValue);
		UserRequest->OnProcessRequestComplete().BindRaw(this, &FMixerInteractivityModule::OnUserRequestComplete);
		if (!UserRequest->ProcessRequest())
		{
			LoginAttemptFinished(false);
		}
	}
	else
	{
		LoginAttemptFinished(false);
	}
#endif
}

void FMixerInteractivityModule::OnUserRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	check(PLATFORM_SUPPORTS_MIXER_OAUTH);

#if PLATFORM_SUPPORTS_MIXER_OAUTH
	if (bSucceeded && HttpResponse.IsValid())
	{
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			CurrentUser = MakeShareable(new FMixerLocalUserJsonSerializable());
			if (!CurrentUser->FromJson(HttpResponse->GetContentAsString()))
			{
				CurrentUser.Reset();
			}
		}
	}

	if (CurrentUser.IsValid())
	{
		UserAuthState = EMixerLoginState::Logged_In;

		if (NeedsClientLibraryActive())
		{
			const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
			const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
			Microsoft::mixer::interactivity_manager::get_singleton_instance()->set_oauth_token(*UserSettings->AccessToken);
			if (!Microsoft::mixer::interactivity_manager::get_singleton_instance()->initialize(*FString::FromInt(Settings->GameVersionId), false))
			{
				LoginAttemptFinished(false);
			}
		}
		else
		{
			LoginAttemptFinished(true);
		}
	}
	else
	{
		LoginAttemptFinished(false);
	}
#endif
}

void FMixerInteractivityModule::StartInteractivity()
{
	switch (Microsoft::mixer::interactivity_manager::get_singleton_instance()->interactivity_state())
	{
	case Microsoft::mixer::interactivity_disabled:
		check(InteractivityState == EMixerInteractivityState::Not_Interactive || InteractivityState == EMixerInteractivityState::Interactivity_Stopping);
		Microsoft::mixer::interactivity_manager::get_singleton_instance()->start_interactive();
		InteractivityState = EMixerInteractivityState::Interactivity_Starting;
		break;

	case Microsoft::mixer::interactivity_enabled:
	case Microsoft::mixer::interactivity_pending:
		check(InteractivityState == EMixerInteractivityState::Interactivity_Starting || InteractivityState == EMixerInteractivityState::Interactive);
		// No-op, but not a problem
		break;

	case Microsoft::mixer::not_initialized:
	case Microsoft::mixer::initializing:
		check(InteractivityState == EMixerInteractivityState::Not_Interactive || InteractivityState == EMixerInteractivityState::Interactivity_Stopping);
		// Caller should wait!
		// @TODO: tell them so.
		break;

	default:
		// Internal error in state management
		check(false);
		break;
	}
}

void FMixerInteractivityModule::StopInteractivity()
{
	switch (Microsoft::mixer::interactivity_manager::get_singleton_instance()->interactivity_state())
	{
	case Microsoft::mixer::interactivity_enabled:
	case Microsoft::mixer::interactivity_pending:
		check(InteractivityState == EMixerInteractivityState::Interactivity_Starting || InteractivityState == EMixerInteractivityState::Interactive);
		Microsoft::mixer::interactivity_manager::get_singleton_instance()->stop_interactive();
		InteractivityState = EMixerInteractivityState::Interactivity_Stopping;
		break;

	case Microsoft::mixer::interactivity_disabled:
		check(InteractivityState == EMixerInteractivityState::Not_Interactive || InteractivityState == EMixerInteractivityState::Interactivity_Stopping);
		// No-op, but not a problem
		break;

	case Microsoft::mixer::not_initialized:
	case Microsoft::mixer::initializing:
		check(InteractivityState == EMixerInteractivityState::Not_Interactive || InteractivityState == EMixerInteractivityState::Interactivity_Stopping);
		// Caller should wait!
		// @TODO: tell them so.
		break;

	default:
		// Internal error in state management
		check(false);
		break;
	}
}

EMixerInteractivityState FMixerInteractivityModule::GetInteractivityState()
{
	return InteractivityState;
}

void FMixerInteractivityModule::SetCurrentScene(FName Scene, FName GroupName)
{
	using namespace Microsoft::mixer;

	if (GetInteractivityState() == EMixerInteractivityState::Interactive)
	{
		std::shared_ptr<interactive_group> Group = GroupName == NAME_None ? interactivity_manager::get_singleton_instance()->group() : interactivity_manager::get_singleton_instance()->group(*GroupName.ToString());
		std::shared_ptr<interactive_scene> TargetScene = interactivity_manager::get_singleton_instance()->scene(*Scene.ToString());
		if (Group != nullptr && TargetScene != nullptr)
		{
			Group->set_scene(TargetScene);
		}
	}
}

FName FMixerInteractivityModule::GetCurrentScene(FName GroupName)
{
	using namespace Microsoft::mixer;
	FName SceneName = NAME_None;
	if (GetInteractivityState() == EMixerInteractivityState::Interactive)
	{
		std::shared_ptr<interactive_group> Group = GroupName == NAME_None ? interactivity_manager::get_singleton_instance()->group() : interactivity_manager::get_singleton_instance()->group(*GroupName.ToString());
		if (Group && Group->scene())
		{
			SceneName = Group->scene()->scene_id().c_str();
		}
	}
	return SceneName;
}

bool FMixerInteractivityModule::NeedsClientLibraryActive()
{
#if WITH_EDITOR
	if (FApp::IsGame())
	{
		return true;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World()->IsPlayInEditor())
		{
			return true;
		}
	}
	return false;
#else
	return true;
#endif
}

void FMixerInteractivityModule::TriggerButtonCooldown(FName Button, FTimespan CooldownTime)
{
	using namespace Microsoft::mixer;

	if (GetInteractivityState() == EMixerInteractivityState::Interactive)
	{
		std::chrono::milliseconds CooldownTimeInMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double, std::milli>(CooldownTime.GetTotalMilliseconds()));
		interactivity_manager::get_singleton_instance()->get_singleton_instance()->trigger_cooldown(*Button.ToString(), CooldownTimeInMs);
	}
}

bool FMixerInteractivityModule::GetButtonDescription(FName Button, FMixerButtonDescription& OutDesc)
{
	using namespace Microsoft::mixer;

	std::shared_ptr<interactive_button_control> ButtonControl = FindButton(Button);
	if (ButtonControl)
	{
		OutDesc.ButtonText = FText::FromString(ButtonControl->button_text().c_str());
		OutDesc.HelpText = FText::GetEmpty(); //FText::FromString(ButtonControl->help_text().c_str());
		OutDesc.SparkCost = ButtonControl->cost();
		return true;
	}
	return false;
}

bool FMixerInteractivityModule::GetButtonState(FName Button, FMixerButtonState& OutState)
{
	using namespace Microsoft::mixer;

	std::shared_ptr<interactive_button_control> ButtonControl = FindButton(Button);
	if (ButtonControl)
	{
		OutState.RemainingCooldown = FTimespan::FromMilliseconds(ButtonControl->remaining_cooldown().count());
		OutState.Progress = 0.0f; // ButtonControl->progress();
		OutState.PressCount = ButtonControl->count_of_button_presses();
		OutState.DownCount = ButtonControl->count_of_button_downs();
		OutState.UpCount = ButtonControl->count_of_button_ups();
		OutState.Enabled = !ButtonControl->disabled();
		return true;
	}
	return false;
}

bool FMixerInteractivityModule::GetStickDescription(FName Stick, FMixerStickDescription& OutDesc)
{
	using namespace Microsoft::mixer;

	std::shared_ptr<interactive_joystick_control> StickControl = FindStick(Stick);
	if (StickControl)
	{
		OutDesc.HelpText = FText::GetEmpty(); //FText::FromString(StickControl->help_text().c_str());
		return true;
	}
	return false;
}

bool FMixerInteractivityModule::GetStickState(FName Stick, FMixerStickState& OutState)
{
	using namespace Microsoft::mixer;

	std::shared_ptr<interactive_joystick_control> StickControl = FindStick(Stick);
	if (StickControl)
	{
		OutState.Axes = FVector2D(static_cast<float>(StickControl->x()), static_cast<float>(StickControl->y()));
		OutState.Enabled = true; //!StickControl->disabled();
		return true;
	}
	return false;
}

std::shared_ptr<Microsoft::mixer::interactive_button_control> FMixerInteractivityModule::FindButton(FName Name)
{
	using namespace Microsoft::mixer;

	if (GetInteractivityState() == EMixerInteractivityState::Interactive)
	{
		FString NameAsString = Name.ToString();
		for (const std::shared_ptr<interactive_scene> SceneObject : interactivity_manager::get_singleton_instance()->scenes())
		{
			if (SceneObject)
			{
				std::shared_ptr<Microsoft::mixer::interactive_button_control> ButtonControl = SceneObject->button(*NameAsString);
				if (ButtonControl)
				{
					return ButtonControl;
				}
			}
		}
	}
	
	return nullptr;
}

std::shared_ptr<Microsoft::mixer::interactive_joystick_control> FMixerInteractivityModule::FindStick(FName Name)
{
	using namespace Microsoft::mixer;

	if (GetInteractivityState() == EMixerInteractivityState::Interactive)
	{
		FString NameAsString = Name.ToString();
		for (const std::shared_ptr<interactive_scene> SceneObject : interactivity_manager::get_singleton_instance()->scenes())
		{
			if (SceneObject)
			{
				std::shared_ptr<Microsoft::mixer::interactive_joystick_control> StickControl = SceneObject->joystick(*NameAsString);
				if (StickControl)
				{
					return StickControl;
				}
			}
		}
	}

	return nullptr;
}

TSharedPtr<const FMixerRemoteUser> FMixerInteractivityModule::GetParticipant(uint32 ParticipantId)
{
	using namespace Microsoft::mixer;

	if (GetInteractivityState() == EMixerInteractivityState::Interactive)
	{
		TSharedPtr<FMixerRemoteUserCached>* CachedUser = RemoteParticipantCache.Find(ParticipantId);
		if (CachedUser)
		{
			return *CachedUser;
		}

		for (std::shared_ptr<interactive_participant> Participant : interactivity_manager::get_singleton_instance()->participants())
		{
			check(Participant);
			if (Participant->mixer_id() == ParticipantId)
			{
				return CreateOrUpdateCachedParticipant(Participant);
			}
		}
	}

	return nullptr;
}

TSharedPtr<FMixerRemoteUserCached> FMixerInteractivityModule::CreateOrUpdateCachedParticipant(std::shared_ptr<Microsoft::mixer::interactive_participant> Participant)
{
	check(Participant);
	TSharedPtr<FMixerRemoteUserCached>& NewUser = RemoteParticipantCache.Add(Participant->mixer_id());
	if (!NewUser.IsValid())
	{
		NewUser = MakeShareable(new FMixerRemoteUserCached(Participant));
	}
	NewUser->UpdateFromSourceParticipant();
	return NewUser;
}

void FMixerInteractivityModule::LoginAttemptFinished(bool Success)
{
	if (Success)
	{
		RetryLoginWithUI = false;
		UserAuthState = EMixerLoginState::Logged_In;

		// Should be fully logged in here (including client library init if relevant)
		check(GetLoginState() == EMixerLoginState::Logged_In);

		InitDesignTimeGroups();

		OnLoginStateChanged().Broadcast(EMixerLoginState::Logged_In);
	}
	else
	{
		CurrentUser.Reset();

#if PLATFORM_SUPPORTS_MIXER_OAUTH
		UMixerInteractivityUserSettings* UserSettings = GetMutableDefault<UMixerInteractivityUserSettings>();
		UserSettings->AccessToken = TEXT("");
		UserSettings->RefreshToken = TEXT("");
		UserSettings->SaveConfig();
		
		IWebBrowserModule::Get().GetSingleton()->GetCookieManager()->DeleteCookies(TEXT(""), TEXT(""),
			[this](int)
		{
			if (RetryLoginWithUI)
			{
				LoginWithUI(NetId);
			}
			else
			{
				NetId.Reset();
				UserAuthState = EMixerLoginState::Not_Logged_In;
				OnLoginStateChanged().Broadcast(EMixerLoginState::Not_Logged_In);
			}
		});
#else
		NetId.Reset();
		UserAuthState = EMixerLoginState::Not_Logged_In;
		OnLoginStateChanged().Broadcast(EMixerLoginState::Not_Logged_In);
#endif

	}
}

bool FMixerInteractivityModule::CreateGroup(FName GroupName, FName InitialScene)
{
	using namespace Microsoft::mixer;

	FString GroupNameAsString = GroupName.ToString();
	std::shared_ptr<interactive_group> FoundGroup = nullptr;
	// Workaround issue in client library - do this the slow way for now
	for (std::shared_ptr<interactive_group> ExistingGroup : interactivity_manager::get_singleton_instance()->groups())
	{
		if (GroupNameAsString == ExistingGroup->group_id().c_str())
		{
			FoundGroup = ExistingGroup;
			break;
		}
	}
	bool CanCreate = FoundGroup == nullptr;
	if (CanCreate)
	{
		if (InitialScene != NAME_None)
		{
			std::shared_ptr<interactive_scene> TargetScene = interactivity_manager::get_singleton_instance()->scene(*InitialScene.ToString());
			if (TargetScene)
			{
				std::make_shared<interactive_group>(*GroupNameAsString, TargetScene);
			}
			else
			{
				CanCreate = false;
			}
		}
		else
		{
			// Constructor adds it to the internal manager.
			std::make_shared<interactive_group>(*GroupNameAsString);
		}
	}

	return CanCreate;
}

bool FMixerInteractivityModule::GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants)
{
	using namespace Microsoft::mixer;

	std::shared_ptr<interactive_group> ExistingGroup = interactivity_manager::get_singleton_instance()->group(*GroupName.ToString());
	bool FoundGroup = false;
	if (ExistingGroup)
	{
		FoundGroup = true;
		const std::vector<std::shared_ptr<interactive_participant>> ParticipantsInternal = ExistingGroup->participants();
		OutParticipants.Empty(ParticipantsInternal.size());
		for (std::shared_ptr<interactive_participant> Participant : ParticipantsInternal)
		{
			OutParticipants.Add(CreateOrUpdateCachedParticipant(Participant));
		}
	}
	else
	{
		OutParticipants.Empty();
	}

	return FoundGroup;
}

bool FMixerInteractivityModule::MoveParticipantToGroup(FName GroupName, uint32 ParticipantId)
{
	using namespace Microsoft::mixer;

	FString GroupNameAsString = GroupName.ToString();
	std::shared_ptr<interactive_group> ExistingGroup = interactivity_manager::get_singleton_instance()->group(*GroupNameAsString);
	bool FoundUser = false;
	if (ExistingGroup)
	{
		std::shared_ptr<interactive_participant> Participant;
		TSharedPtr<FMixerRemoteUserCached>* CachedUser = RemoteParticipantCache.Find(ParticipantId);
		if (CachedUser)
		{
			Participant = (*CachedUser)->GetSourceParticipant();
		}
		else
		{
			for (std::shared_ptr<interactive_participant> PossibleParticipant : interactivity_manager::get_singleton_instance()->participants())
			{
				check(PossibleParticipant);
				if (PossibleParticipant->mixer_id() == ParticipantId)
				{
					Participant = PossibleParticipant;
					break;
				}
			}
		}

		if (Participant)
		{
			FoundUser = true;
			Participant->set_group(ExistingGroup);
			CreateOrUpdateCachedParticipant(Participant);
		}
	}
	return FoundUser;
}

void FMixerInteractivityModule::InitDesignTimeGroups()
{
	if (NeedsClientLibraryActive())
	{
		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		for (const FMixerPredefinedGroup& PredefinedGroup : Settings->DesignTimeGroups)
		{
			if (!CreateGroup(PredefinedGroup.Name, PredefinedGroup.InitialScene))
			{
				// Already exists, try to just set the scene
				SetCurrentScene(PredefinedGroup.InitialScene, PredefinedGroup.Name);
			}
		}
	}
}

FString FMixerInteractivityModule::GetRedirectUri()
{
	// Deal with any wildcards and ensure protocol is on the front.  This is helpful to
	// allow the 'Hosts' entry from the Mixer OAuth Clients page to just be directly reused.
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	FString RedirectUri = Settings->RedirectUri;
	RedirectUri = RedirectUri.Replace(TEXT(".*."), TEXT("."));
	if (!RedirectUri.StartsWith(TEXT("http")))
	{
		RedirectUri = FString(TEXT("http://")) + RedirectUri;
	}
	RedirectUri = RedirectUri.Replace(TEXT("/*."), TEXT("/www."));

	return RedirectUri;
}

FMixerRemoteUserCached::FMixerRemoteUserCached(std::shared_ptr<Microsoft::mixer::interactive_participant> InParticipant)
	: SourceParticipant(InParticipant)
{
	Id = SourceParticipant->mixer_id();
}

void FMixerRemoteUserCached::UpdateFromSourceParticipant()
{
	// Is there really not a std definition for this?
	typedef std::chrono::duration<uint64, std::ratio_multiply<std::nano, std::ratio<100>>> DateTimeTicks;

	Name = SourceParticipant->username().c_str();
	Level = SourceParticipant->level();
	ConnectedAt = FDateTime::FromUnixTimestamp(std::chrono::duration_cast<DateTimeTicks>(SourceParticipant->connected_at()).count());
	InputAt = FDateTime::FromUnixTimestamp(std::chrono::duration_cast<DateTimeTicks>(SourceParticipant->last_input_at()).count());
	InputEnabled = !SourceParticipant->input_disabled();
	std::shared_ptr<Microsoft::mixer::interactive_group> GroupInternal = SourceParticipant->group();
	Group = GroupInternal ? FName(GroupInternal->group_id().c_str()) : NAME_DefaultMixerParticipantGroup;
}