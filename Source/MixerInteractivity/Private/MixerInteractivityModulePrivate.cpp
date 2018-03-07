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
#include "OnlineChatMixerPrivate.h"

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
#include "OnlineSubsystemTypes.h"

#if PLATFORM_SUPPORTS_MIXER_OAUTH
#include "SMixerLoginPane.h"
#endif

DEFINE_LOG_CATEGORY(LogMixerInteractivity);

void FMixerInteractivityModule::StartupModule()
{
	UserAuthState = EMixerLoginState::Not_Logged_In;
	InteractiveConnectionAuthState = EMixerLoginState::Not_Logged_In;
	InteractivityState = EMixerInteractivityState::Not_Interactive;
	HasCreatedGroups = false;

	ChatInterface = MakeShared<FOnlineChatMixer>();
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

	// Go async to avoid blocking the game thread on the cross-OS call
	XboxUserOperation = Async<Windows::Xbox::System::User^>(EAsyncExecution::ThreadPool,
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
#else
	if (UserAuthState == EMixerLoginState::Logged_In)
	{
		check(NeedsClientLibraryActive());

		// User is already logged in, but client library not initialized.
		// This case will occur during PIE when logging in for interactivity with the same Mixer user that owns the Editor settings.
		if (StartInteractiveConnection())
		{
			// Set this immediately so that polling for state matches the event
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
	JsonWriter->WriteValue(TEXT("redirect_uri"), Settings->GetResolvedRedirectUri());
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
#endif

	NetId = UserId;
	UserAuthState = EMixerLoginState::Logging_In;
	OnLoginStateChanged().Broadcast(EMixerLoginState::Logging_In);

	return true;
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

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
#if WITH_EDITOR
	const FText TitleText = FText::Format(NSLOCTEXT("MixerInteractivity", "LoginWindowTitle_Sandbox", "Login to Mixer - XBL Sandbox {0}"), FText::FromString(Settings->GetSandboxForOAuth()));
#else
	const FText TitleText = NSLOCTEXT("MixerInteractivity", "LoginWindowTitle", "Login to Mixer");
#endif
	LoginWindow = SNew(SWindow)
		.Title(TitleText)
		.SizingRule(ESizingRule::FixedSize)
		.ClientSize(FVector2D(500.f, 500.f))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	LoginWindow->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &FMixerInteractivityModule::OnLoginWindowClosed));
	
	LoginWindow->SetContent(
		SNew(SMixerLoginPane)
		.AllowSilentLogin(false)
		.BackgroundColor(FColor(255, 255, 255, 255))
		.OnAuthCodeReady_Raw(this, &FMixerInteractivityModule::OnAuthCodeReady)
		.OnUIFlowFinished_Raw(this, &FMixerInteractivityModule::OnLoginUIFlowFinished)
	);

	TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (RootWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(LoginWindow.ToSharedRef(), RootWindow.ToSharedRef());
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

FReply FMixerInteractivityModule::OnAuthCodeReady(const FString& AuthCode)
{
	if (!LoginWithAuthCodeInternal(AuthCode, NetId))
	{
		TSharedRef<SWindow> WindowToClose = LoginWindow.ToSharedRef();
		FSlateApplication::Get().RequestDestroyWindow(WindowToClose);
	}

	return FReply::Handled();
}

void FMixerInteractivityModule::OnLoginUIFlowFinished(bool WasSuccessful)
{
	if (LoginWindow.IsValid())
	{
		TSharedRef<SWindow> WindowToClose = LoginWindow.ToSharedRef();
		if (WasSuccessful)
		{
			LoginWindow->SetOnWindowClosed(FOnWindowClosed());
			LoginWindow.Reset();
		}

		FSlateApplication::Get().RequestDestroyWindow(WindowToClose);
	}
}

void FMixerInteractivityModule::OnLoginWindowClosed(const TSharedRef<SWindow>&)
{
	if (LoginWindow.IsValid())
	{
		// Closed before we were done.
		LoginAttemptFinished(false);
		LoginWindow.Reset();
	}
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
#endif
	UserAuthState = EMixerLoginState::Not_Logged_In;
	OnLoginStateChanged().Broadcast(EMixerLoginState::Not_Logged_In);

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
			return InteractiveConnectionAuthState;

			//switch (ClientLibraryState)
			//{
			//case Microsoft::mixer::not_initialized:
			//	return EMixerLoginState::Not_Logged_In;

			//case Microsoft::mixer::initializing:
			//	return EMixerLoginState::Logging_In;

			//case Microsoft::mixer::interactivity_disabled:
			//case Microsoft::mixer::interactivity_enabled:
			//case Microsoft::mixer::interactivity_pending:
			//	return EMixerLoginState::Logged_In;

			//default:
			//	// Internal error in Mixer client library state management
			//	check(false);
			//	return EMixerLoginState::Not_Logged_In;
			//}
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

bool FMixerInteractivityModule::Tick(float DeltaTime)
{
#if PLATFORM_XBOXONE
	TickXboxLogin();
#endif

	TickLocalUserMaintenance();

	if (!NeedsClientLibraryActive())
	{
		// Should really be Un-init if possible
		StopInteractivity();

		HasCreatedGroups = false;
	}

	return true;
}

void FMixerInteractivityModule::TickLocalUserMaintenance()
{
	if (NeedsClientLibraryActive() && CurrentUser.IsValid())
	{
		if (FApp::GetCurrentTime() > CurrentUser->RefreshAtAppTime)
		{
			TSharedRef<IHttpRequest> UserRequest = FHttpModule::Get().CreateRequest();
			UserRequest->SetVerb(TEXT("GET"));
			UserRequest->SetURL(FString::Printf(TEXT("https://mixer.com/api/v1/users/%d"), CurrentUser->Id));
			UserRequest->OnProcessRequestComplete().BindRaw(this, &FMixerInteractivityModule::OnUserMaintenanceRequestComplete);
			UserRequest->ProcessRequest();

			// Prevent further polling while we have a request active
			CurrentUser->RefreshAtAppTime = MAX_dbl;
		}
	}
}

void FMixerInteractivityModule::OnUserMaintenanceRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (CurrentUser.IsValid())
	{
		if (bSucceeded && HttpResponse.IsValid())
		{
			if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
			{
				FMixerLocalUserJsonSerializable UpdatedUser;
				if (UpdatedUser.FromJson(HttpResponse->GetContentAsString()))
				{
					// Make sure the user hasn't changed!
					if (CurrentUser->Id == UpdatedUser.Id)
					{
						CurrentUser->Sparks = UpdatedUser.Sparks;
						CurrentUser->Experience = UpdatedUser.Experience;
						CurrentUser->Level = UpdatedUser.Level;

						bool UserBroadcastStateChanged = CurrentUser->Channel.IsBroadcasting != UpdatedUser.Channel.IsBroadcasting;
						CurrentUser->Channel = UpdatedUser.Channel;

						if (UserBroadcastStateChanged)
						{
							OnBroadcastingStateChanged().Broadcast(CurrentUser->Channel.IsBroadcasting);
						}
					}
				}
			}
		}

		// Note: so far, adjusting this interval in response to app lifecycle events (in case broadcasting
		// was started by the user while app was in background) have not been terribly effective - when starting the
		// state takes long enough to change on the service that it's not very different from just waiting for the next
		// regularly schedule poll.
		static const double UserPollingInterval = 30.0;
		CurrentUser->RefreshAtAppTime = FMath::Min(CurrentUser->RefreshAtAppTime, FApp::GetCurrentTime() + UserPollingInterval);
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
	JsonWriter->WriteValue(TEXT("redirect_uri"), Settings->GetResolvedRedirectUri());
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
		TSharedRef<IHttpRequest> UserRequest = FHttpModule::Get().CreateRequest();
		UserRequest->SetVerb(TEXT("GET"));
		UserRequest->SetURL(TEXT("https://mixer.com/api/v1/users/current"));
		UserRequest->SetHeader(TEXT("Authorization"), UserSettings->GetAuthZHeaderValue());
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
			switch (InteractiveConnectionAuthState)
			{
			case EMixerLoginState::Not_Logged_In:
				if (!StartInteractiveConnection())
				{
					LoginAttemptFinished(false);
				}
				else
				{
					check(InteractiveConnectionAuthState != EMixerLoginState::Not_Logged_In);
				}
				break;

			case EMixerLoginState::Logging_In:
				// Should naturally progress and we'll handle it in Tick
				break;

			case EMixerLoginState::Logged_In:
				// We're already logged in
				LoginAttemptFinished(true);
				break;

			default:
				// Internal error in state management
				check(false);
				break;
			}

			//switch (ClientLibraryState)
			//{
			//case Microsoft::mixer::not_initialized:
			//	if (!Microsoft::mixer::interactivity_manager::get_singleton_instance()->initialize(*FString::FromInt(Settings->GameVersionId), false, *Settings->ShareCode))
			//	{
			//		LoginAttemptFinished(false);
			//	}
			//	else
			//	{
			//		// Set this immediately to avoid a temporary pop to Not_Logged_In
			//		ClientLibraryState = Microsoft::mixer::initializing;
			//	}
			//	break;

			//case Microsoft::mixer::initializing:
			//	// Should naturally progress and we'll handle it in Tick
			//	break;

			//default:
			//	// We're already logged in
			//	LoginAttemptFinished(true);
			//}
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
}

EMixerInteractivityState FMixerInteractivityModule::GetInteractivityState()
{
	return InteractivityState;
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

		// If we arrive at Logged_In and are already broadcasting we
		// should send a changed event - from the client's POV we weren't
		// broadcasting before since we weren't logged in!
		if (CurrentUser.IsValid() && CurrentUser->Channel.IsBroadcasting)
		{
			OnBroadcastingStateChanged().Broadcast(true);
		}
	}
	else
	{
		CurrentUser.Reset();
		UserAuthState = EMixerLoginState::Not_Logged_In;

#if PLATFORM_SUPPORTS_MIXER_OAUTH
		UMixerInteractivityUserSettings* UserSettings = GetMutableDefault<UMixerInteractivityUserSettings>();
		UserSettings->AccessToken = TEXT("");
		UserSettings->RefreshToken = TEXT("");
		UserSettings->SaveConfig();
#endif
		if (RetryLoginWithUI)
		{
			LoginWithUI(NetId);
		}
		else
		{
			NetId.Reset();
			OnLoginStateChanged().Broadcast(EMixerLoginState::Not_Logged_In);
		}
	}
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

TSharedPtr<IOnlineChat> FMixerInteractivityModule::GetChatInterface()
{
	return ChatInterface;
}

TSharedPtr<IOnlineChatMixer> FMixerInteractivityModule::GetExtendedChatInterface()
{
	return ChatInterface;
}

#if PLATFORM_XBOXONE
void FMixerInteractivityModule::TickXboxLogin()
{
	if (UserAuthState == EMixerLoginState::Logging_In)
	{
		bool LoginError = false;
		if (GetXTokenOperation != nullptr)
		{
			if (GetXTokenOperation->Status != Windows::Foundation::AsyncStatus::Started)
			{
				if (GetXTokenOperation->Status == Windows::Foundation::AsyncStatus::Completed)
				{
					UMixerInteractivityUserSettings* UserSettings = GetMutableDefault<UMixerInteractivityUserSettings>();
					UserSettings->AccessToken = GetXTokenOperation->GetResults()->Token->ToString()->Data();

					TSharedRef<IHttpRequest> UserRequest = FHttpModule::Get().CreateRequest();
					UserRequest->SetVerb(TEXT("GET"));
					UserRequest->SetURL(TEXT("https://mixer.com/api/v1/users/current"));
					UserRequest->SetHeader(TEXT("Authorization"), UserSettings->GetAuthZHeaderValue());
					UserRequest->OnProcessRequestComplete().BindRaw(this, &FMixerInteractivityModule::OnUserRequestComplete);
					if (!UserRequest->ProcessRequest())
					{
						LoginError = true;
					}
				}
				else
				{
					LoginError = true;
				}

				GetXTokenOperation = nullptr;
			}
		}
		else  if (XboxUserOperation.IsReady())
		{
			Windows::Xbox::System::User^ ResolvedUser = GetXboxUser();
			if (ResolvedUser != nullptr)
			{
				try
				{
					GetXTokenOperation = ResolvedUser->GetTokenAndSignatureAsync(L"POST", L"https://mixer.com", L"");
				}
				catch (...)
				{
					LoginError = true;
				}
			}
			else
			{
				LoginError = true;
			}
		}

		if (LoginError)
		{
			check(!GetXTokenOperation);
			NetId.Reset();
			UserAuthState = EMixerLoginState::Not_Logged_In;
			LoginAttemptFinished(false);
		}
	}
}
#endif