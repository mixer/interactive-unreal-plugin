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
#include "MixerBindingUtils.h"
#include "OnlineChatMixer.h"

#include "HttpModule.h"
#include "PlatformHttp.h"
#include "JsonTypes.h"
#include "JsonPrintPolicy.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
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
#pragma warning(disable:4596)
#pragma pack(push)
#pragma pack(8)
#include <interactivity_types.h>
#include <interactivity.h>
#pragma pack(pop)
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
	ClientLibraryState = Microsoft::mixer::not_initialized;
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
#else
	if (UserAuthState == EMixerLoginState::Logged_In)
	{
		check(NeedsClientLibraryActive());

		// User is already logged in, but client library not initialized.
		// This case will occur during PIE when logging in for interactivity with the same Mixer user that owns the Editor settings.
		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
		Microsoft::mixer::interactivity_manager::get_singleton_instance()->set_oauth_token(*UserSettings->AccessToken);
		if (Microsoft::mixer::interactivity_manager::get_singleton_instance()->initialize(*FString::FromInt(Settings->GameVersionId), false, *Settings->ShareCode))
		{
			// Set this immediately so that polling for state matches the event
			ClientLibraryState = Microsoft::mixer::initializing;
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
		if (WasSuccessful)
		{
			LoginWindow->SetOnWindowClosed(FOnWindowClosed());
		}

		TSharedRef<SWindow> WindowToClose = LoginWindow.ToSharedRef();
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
			switch (ClientLibraryState)
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

bool FMixerInteractivityModule::Tick(float DeltaTime)
{
#if PLATFORM_XBOXONE
	TickXboxLogin();
#endif

	TickParticipantCacheMaintenance();
	TickClientLibrary();
	TickLocalUserMaintenance();

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
			// Errors that impact our login state are accompanied by an interactivity_state_changed event, so
			// dealing with them here is just double counting.  Stick to outputting the message.
			UE_LOG(LogMixerInteractivity, Warning, TEXT("%s"), MixerEvent.err_message().c_str());
			break;

		case interactive_event_type::interactivity_state_changed:
		{
			auto StateChangeArgs = std::static_pointer_cast<interactivity_state_change_event_args>(MixerEvent.event_args());
			EMixerLoginState PreviousLoginState = GetLoginState();
			ClientLibraryState = StateChangeArgs->new_state();
			switch (StateChangeArgs->new_state())
			{
			case interactivity_state::not_initialized:
				InteractivityState = EMixerInteractivityState::Not_Interactive;
				switch (PreviousLoginState)
				{
				case EMixerLoginState::Logging_In:
					// On Xbox a pop to not_initialized is expected as a result of calling set_local_user when there
					// was already a previous user.  In any case on all platforms we can safely postpone dealing with
					// the client library state until user auth is finished.
					if (UserAuthState != EMixerLoginState::Logging_In)
					{
						LoginAttemptFinished(false);
					}
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
				if (PreviousLoginState == EMixerLoginState::Logging_In)
				{
					LoginAttemptFinished(true);
				}
				if (!HasCreatedGroups)
				{
					InitDesignTimeGroups();
					InitCustomControls();
				}
				break;

			case interactivity_state::interactivity_disabled:
				InteractivityState = EMixerInteractivityState::Not_Interactive;
				if (PreviousLoginState == EMixerLoginState::Logging_In)
				{
					LoginAttemptFinished(true);
				}
				if (!HasCreatedGroups)
				{
					InitDesignTimeGroups();
					InitCustomControls();
				}
				break;

			case interactivity_state::interactivity_enabled:
				InteractivityState = EMixerInteractivityState::Interactive;
				if (PreviousLoginState == EMixerLoginState::Logging_In)
				{
					LoginAttemptFinished(true);
				}
				if (!HasCreatedGroups)
				{
					InitDesignTimeGroups();
					InitCustomControls();
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
			FMixerButtonEventDetails Details;
			Details.Pressed = OriginalButtonArgs->is_pressed();
			Details.TransactionId = OriginalButtonArgs->transaction_id().c_str();
			Details.SparkCost = OriginalButtonArgs->cost();
			ButtonEvent.Broadcast(FName(OriginalButtonArgs->control_id().c_str()), RemoteParticipant, Details);
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
					Microsoft::mixer::interactivity_manager::get_singleton_instance()->set_oauth_token(*UserSettings->AccessToken);
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
			const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
			switch (ClientLibraryState)
			{
			case Microsoft::mixer::not_initialized:
				if (!Microsoft::mixer::interactivity_manager::get_singleton_instance()->initialize(*FString::FromInt(Settings->GameVersionId), false, *Settings->ShareCode))
				{
					LoginAttemptFinished(false);
				}
				else
				{
					// Set this immediately to avoid a temporary pop to Not_Logged_In
					ClientLibraryState = Microsoft::mixer::initializing;
				}
				break;

			case Microsoft::mixer::initializing:
				// Should naturally progress and we'll handle it in Tick
				break;

			default:
				// We're already logged in
				LoginAttemptFinished(true);
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
		check(InteractivityState == EMixerInteractivityState::Interactivity_Starting || 
				InteractivityState == EMixerInteractivityState::Interactivity_Stopping || 
				InteractivityState == EMixerInteractivityState::Interactive);
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
		OutState.Progress = ButtonControl->progress();
		OutState.PressCount = ButtonControl->count_of_button_presses();
		OutState.DownCount = ButtonControl->count_of_button_downs();
		OutState.UpCount = ButtonControl->count_of_button_ups();
		OutState.Enabled = !ButtonControl->disabled();
		return true;
	}
	return false;
}

bool FMixerInteractivityModule::GetButtonState(FName Button, uint32 ParticipantId, FMixerButtonState& OutState)
{
	using namespace Microsoft::mixer;

	std::shared_ptr<interactive_button_control> ButtonControl = FindButton(Button);
	if (ButtonControl)
	{
		OutState.RemainingCooldown = FTimespan::FromMilliseconds(ButtonControl->remaining_cooldown().count());
		OutState.Progress = ButtonControl->progress();
		OutState.PressCount = ButtonControl->is_pressed(ParticipantId) ? 1 : 0;
		OutState.DownCount = ButtonControl->is_down(ParticipantId) ? 1 : 0;
		OutState.UpCount = ButtonControl->is_up(ParticipantId) ? 1 : 0;
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

bool FMixerInteractivityModule::GetStickState(FName Stick, uint32 ParticipantId, FMixerStickState& OutState)
{
	using namespace Microsoft::mixer;

	std::shared_ptr<interactive_joystick_control> StickControl = FindStick(Stick);
	if (StickControl)
	{
		OutState.Axes = FVector2D(static_cast<float>(StickControl->x(ParticipantId)), static_cast<float>(StickControl->y(ParticipantId)));
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
		InitCustomControls();

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

bool FMixerInteractivityModule::CreateGroup(FName GroupName, FName InitialScene)
{
	using namespace Microsoft::mixer;

	FString GroupNameAsString = GroupName.ToString();
	std::shared_ptr<interactive_group> FoundGroup = interactivity_manager::get_singleton_instance()->group(*GroupName.ToString());
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

void FMixerInteractivityModule::CaptureSparkTransaction(const FString& TransactionId)
{
	Microsoft::mixer::interactivity_manager::get_singleton_instance()->capture_transaction(*TransactionId);
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

void FMixerInteractivityModule::InitCustomControls()
{
	if (NeedsClientLibraryActive())
	{
		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		for (TMap<FName, TSubclassOf<UMixerCustomControl>>::TConstIterator It(Settings->CachedCustomControls); It; ++It)
		{
			if (It->Value.Get())
			{
				AdvancedCustomControls.Add(It->Key, NewObject<UMixerCustomControl>(GetTransientPackage(), It->Value));
			}
			else
			{
				SimpleCustomControls.Add(It->Key, MakeShared<FMixerSimpleCustomControl>());
			}
		}
	}
}

void FMixerInteractivityModule::HandleCustomMessage(const FString& MessageBodyString)
{
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(MessageBodyString);
	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		FString Method;
		if (JsonObject->TryGetStringField(TEXT("method"), Method))
		{
			const TSharedPtr<FJsonObject> *ParamsObject;
			if (JsonObject->TryGetObjectField(TEXT("params"), ParamsObject))
			{
				if (Method == TEXT("giveInput"))
				{
					HandleCustomControlInputMessage(ParamsObject->Get());
				}
				else if (Method == TEXT("onControlUpdate"))
				{
					HandleCustomControlUpdateMessage(ParamsObject->Get());
				}
				else
				{
					OnCustomMethodCall().Broadcast(*Method, ParamsObject->Get());
				}
			}
		}
	}
}

void FMixerInteractivityModule::HandleCustomControlUpdateMessage(FJsonObject* ParamsJson)
{
	const TArray<TSharedPtr<FJsonValue>> *UpdatedControls;
	if (ParamsJson->TryGetArrayField(TEXT("controls"), UpdatedControls))
	{
		for (const TSharedPtr<FJsonValue> Control : *UpdatedControls)
		{
			const TSharedPtr<FJsonObject> ControlObject = Control->AsObject();
			if (ControlObject.IsValid())
			{
				FString ControlIdRaw;
				if (ControlObject->TryGetStringField(TEXT("controlID"), ControlIdRaw))
				{
					FName ControlId = *ControlIdRaw;
					if (TSharedPtr<FMixerSimpleCustomControl>* SimpleControl = SimpleCustomControls.Find(ControlId))
					{
						check(SimpleControl->IsValid());

						(*SimpleControl)->PropertyBag.Append(ControlObject->Values);
						OnUnhandledCustomControlPropertyUpdate().Broadcast(ControlId, *SimpleControl);
					}
					else if (UMixerCustomControl** AdvancedControl = AdvancedCustomControls.Find(ControlId))
					{
						FJsonObjectConverter::JsonObjectToUStruct(ControlObject.ToSharedRef(), (*AdvancedControl)->GetClass(), *AdvancedControl);
						(*AdvancedControl)->OnServerPropertiesUpdated();
					}
					else
					{
						UE_LOG(LogMixerInteractivity, Warning, TEXT("Ignoring proprty updates for unknown custom control %s.  Update your bindings in the Mixer Interactivity project settings."), *ControlIdRaw);
					}
				}
			}
		}
	}
}

void FMixerInteractivityModule::HandleCustomControlInputMessage(FJsonObject* ParamsJson)
{
	// @TODO - get participant and transaction (if any)

	const TSharedPtr<FJsonObject> *InputObject;
	if (ParamsJson->TryGetObjectField(TEXT("input"), InputObject))
	{
		FString ControlIdRaw;
		if ((*InputObject)->TryGetStringField(TEXT("controlID"), ControlIdRaw))
		{
			FString EventTypeRaw;
			if ((*InputObject)->TryGetStringField(TEXT("event"), EventTypeRaw))
			{
				FName ControlId = *ControlIdRaw;
				FName EventType = *EventTypeRaw;
				if (TSharedPtr<FMixerSimpleCustomControl>* SimpleControl = SimpleCustomControls.Find(ControlId))
				{
					check(SimpleControl->IsValid());
					OnUnhandledCustomControlInputEvent().Broadcast(ControlId, EventType, nullptr, *SimpleControl, InputObject->Get());
				}
				else if (UMixerCustomControl** AdvancedControl = AdvancedCustomControls.Find(ControlId))
				{
					UFunction* HandlerMethod = (*AdvancedControl)->FindFunction(EventType);
					if (HandlerMethod != nullptr)
					{
						void* ParamStorage = FMemory_Alloca(HandlerMethod->ParmsSize);
						if (ParamStorage != nullptr)
						{
							MixerBindingUtils::ExtractCustomEventParamsFromMessage(InputObject->Get(), HandlerMethod, ParamStorage, HandlerMethod->ParmsSize);
							(*AdvancedControl)->ProcessEvent(HandlerMethod, ParamStorage);
							MixerBindingUtils::DestroyCustomEventParams(HandlerMethod, ParamStorage, HandlerMethod->ParmsSize);
						}
					}
					else
					{
						UE_LOG(LogMixerInteractivity, Warning, TEXT("Received unknown input event %s for custom control %s.  Event will be ignored.  Provide a UFUNCTION matching the event name in order to handle this event."), *EventTypeRaw, *ControlIdRaw);
					}
				}
				else
				{
					UE_LOG(LogMixerInteractivity, Warning, TEXT("Ignoring input event %s for unknown custom control %s.  Update your bindings in the Mixer Interactivity project settings."), *EventTypeRaw, *ControlIdRaw);
				}
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

void FMixerInteractivityModule::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TMap<FName, UMixerCustomControl*>::TIterator It(AdvancedCustomControls); It; ++It)
	{
		Collector.AddReferencedObject(It->Value);
	}
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
		else  if (PlatformUser.IsReady())
		{
			Windows::Xbox::System::User^ ResolvedUser = PlatformUser.Get();
			if (ResolvedUser != nullptr)
			{
				Microsoft::mixer::interactivity_manager::get_singleton_instance()->set_local_user(PlatformUser.Get());

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

			PlatformUser = TFuture<Windows::Xbox::System::User^>();
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