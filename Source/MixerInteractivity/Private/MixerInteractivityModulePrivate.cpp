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
#include "MixerInteractivityProjectAsset.h"
#include "OnlineChatMixerPrivate.h"
#include "OnlineChatMixerPrivate.h"

#include "HttpModule.h"
#include "PlatformHttp.h"
#include "JsonTypes.h"
#include "JsonPrintPolicy.h"
#include "Policies/CondensedJsonPrintPolicy.h"
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
#elif PLATFORM_NEEDS_OSS_LIVE
#include "OnlineSubsystemUtils.h"
#endif

#if PLATFORM_XBOXONE
#include "XboxOneInputInterface.h"
#endif

DEFINE_LOG_CATEGORY(LogMixerInteractivity);

void FMixerInteractivityModule::StartupModule()
{
	RetryLoginWithUI = false;
	UserAuthState = EMixerLoginState::Not_Logged_In;
	InteractiveConnectionAuthState = EMixerLoginState::Not_Logged_In;
	InteractivityState = EMixerInteractivityState::Not_Interactive;

	ChatInterface = MakeShared<FOnlineChatMixer>();

#if PLATFORM_XBOXONE
	check(FSlateApplication::IsInitialized());
	static_cast<FXboxOneInputInterface*>(FSlateApplication::Get().GetInputInterface())->OnUserRemovedDelegates.AddRaw(this, &FMixerInteractivityModule::OnXboxUserRemoved);
#elif PLATFORM_NEEDS_OSS_LIVE
	IOnlineIdentityPtr IdentityInterface = Online::GetIdentityInterface(nullptr, LIVE_SUBSYSTEM);
	if (IdentityInterface.IsValid())
	{
		FOnLoginCompleteDelegate LoginCompleteDelegate = FOnLoginCompleteDelegate::CreateRaw(this, &FMixerInteractivityModule::OnXTokenRetrievalComplete);
		for (int32 i = 0; i < MAX_LOCAL_PLAYERS; ++i)
		{
			LoginCompleteDelegateHandle[i] = IdentityInterface->AddOnLoginCompleteDelegate_Handle(i, LoginCompleteDelegate);
		}
	}
#endif
}

void FMixerInteractivityModule::ShutdownModule()
{
#if PLATFORM_XBOXONE
	check(FSlateApplication::IsInitialized());
	static_cast<FXboxOneInputInterface*>(FSlateApplication::Get().GetInputInterface())->OnUserRemovedDelegates.RemoveAll(this);
#elif PLATFORM_NEEDS_OSS_LIVE
	IOnlineIdentityPtr IdentityInterface = Online::GetIdentityInterface(nullptr, LIVE_SUBSYSTEM);
	if (IdentityInterface.IsValid())
	{
		for (int32 i = 0; i < MAX_LOCAL_PLAYERS; ++i)
		{
			if (LoginCompleteDelegateHandle[i].IsValid())
			{
				IdentityInterface->ClearOnLoginCompleteDelegate_Handle(i, LoginCompleteDelegateHandle[i]);
			}
		}
	}
#endif
}

bool FMixerInteractivityModule::LoginSilently(TSharedPtr<const FUniqueNetId> UserId)
{
	if (GetLoginState() != EMixerLoginState::Not_Logged_In)
	{
		return false;
	}

	if (!PLATFORM_XBOXONE && !PLATFORM_SUPPORTS_MIXER_OAUTH && !PLATFORM_NEEDS_OSS_LIVE)
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("There is no supported user login flow for this platform."));
		return false;
	}

	if (PLATFORM_XBOXONE && !UserId.IsValid())
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("User id is required to login to Mixer on Xbox."));
		return false;
	}

	return LoginSilentlyInternal(UserId);
}

#if PLATFORM_SUPPORTS_MIXER_OAUTH
bool FMixerInteractivityModule::LoginSilentlyInternal(TSharedPtr<const FUniqueNetId> UserId)
{
	if (GetUserAuthState() == EMixerLoginState::Logged_In)
	{
		check(NeedsClientLibraryActive());
		return StartInteractiveConnection();
	}

	const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
	if (UserSettings->RefreshToken.IsEmpty())
	{
		SetUserAuthState(EMixerLoginState::Not_Logged_In);
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
		SetUserAuthState(EMixerLoginState::Not_Logged_In);
		return false;
	}

	SetUserAuthState(EMixerLoginState::Logging_In);
	NetId = UserId;

	return true;
}
#endif

bool FMixerInteractivityModule::LoginWithUI(TSharedPtr<const FUniqueNetId> UserId)
{
	if (!PLATFORM_SUPPORTS_MIXER_OAUTH)
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("LoginWithUI uses OAuth login flow which is not supported on this platform."));
		return false;
	}

	if (GetLoginState() != EMixerLoginState::Not_Logged_In)
	{
		return false;
	}

	LoginWithUIInternal(UserId);

	return true;
}

void FMixerInteractivityModule::LoginWithUIInternal(TSharedPtr<const FUniqueNetId> UserId)
{
	check(PLATFORM_SUPPORTS_MIXER_OAUTH);

#if PLATFORM_SUPPORTS_MIXER_OAUTH
	if (!RetryLoginWithUI && LoginSilentlyInternal(UserId))
	{
		RetryLoginWithUI = true;
	}
	else
	{
		// This is already a retry attempt.  Don't allow another.
		RetryLoginWithUI = false;

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
		SetUserAuthState(EMixerLoginState::Logging_In);
	}
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
		SetUserAuthState(EMixerLoginState::Not_Logged_In);
		LoginWindow.Reset();
	}
}

bool FMixerInteractivityModule::LoginWithAuthCode(const FString& AuthCode, TSharedPtr<const FUniqueNetId> UserId)
{
	if (!PLATFORM_SUPPORTS_MIXER_OAUTH)
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("LoginWithAuthCode is part of OAuth login flow which is not supported on this platform."));
		return false;
	}

	if (GetLoginState() != EMixerLoginState::Not_Logged_In)
	{
		return false;
	}

	return LoginWithAuthCodeInternal(AuthCode, UserId);
}

bool FMixerInteractivityModule::Logout()
{
	switch (GetLoginState())
	{
	case EMixerLoginState::Logged_In:
	case EMixerLoginState::Logging_In:
		StopInteractivity();
		SetUserAuthState(EMixerLoginState::Logging_Out);
		StopInteractiveConnection();
		SetUserAuthState(EMixerLoginState::Not_Logged_In);
		return true;

	default:
		return false;
	}
}

EMixerLoginState FMixerInteractivityModule::GetLoginState()
{
	EMixerLoginState UserState = GetUserAuthState();
	switch (UserState)
	{
	case EMixerLoginState::Logged_In:
		check(CurrentUser.IsValid());
		return NeedsClientLibraryActive() ? GetInteractiveConnectionAuthState() : UserState;

	case EMixerLoginState::Logging_In:
	case EMixerLoginState::Logging_Out:
	case EMixerLoginState::Not_Logged_In:
		return UserState;

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
	FlushControlUpdates();

	if (!NeedsClientLibraryActive())
	{
		StopInteractivity();
		StopInteractiveConnection();
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
		SetUserAuthState(EMixerLoginState::Not_Logged_In);
		return false;
	}

	NetId = UserId;
	SetUserAuthState(EMixerLoginState::Logging_In);
#endif
	return true;
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
			SetUserAuthState(EMixerLoginState::Not_Logged_In);
		}
	}
	else
	{
		SetUserAuthState(EMixerLoginState::Not_Logged_In);
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

	SetUserAuthState(CurrentUser.IsValid() ? EMixerLoginState::Logged_In : EMixerLoginState::Not_Logged_In);
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

bool FMixerInteractivityModule::GetCustomControl(UWorld* ForWorld, FName ControlName, TSharedPtr<FJsonObject>& OutControlObj)
{
	OutControlObj = UMixerInteractivityBlueprintEventSource::GetBlueprintEventSource(ForWorld)->GetUnmappedCustomControl(ControlName);
	return OutControlObj.IsValid();
}

bool FMixerInteractivityModule::GetCustomControl(UWorld* ForWorld, FName ControlName, UMixerCustomControl*& OutControlObj)
{
	OutControlObj = UMixerInteractivityBlueprintEventSource::GetBlueprintEventSource(ForWorld)->GetMappedCustomControl(ControlName);
	return OutControlObj != nullptr;
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

bool FMixerInteractivityModule::HandleControlUpdateMessage(FJsonObject* ParamsJson)
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
					const TSharedRef<FJsonObject> ControlJsonRef = ControlObject.ToSharedRef();
					if (!HandleSingleControlUpdate(ControlId, ControlJsonRef))
					{
						OnCustomControlPropertyUpdate().Broadcast(ControlId, ControlJsonRef);
					}
				}
			}
		}
	}

	return true;
}

void FMixerInteractivityModule::HandleCustomControlInputMessage(FJsonObject* ParamsJson)
{
	// @TODO - get participant and transaction (if any)

	const TSharedPtr<FJsonObject> *InputObject;
	if (ParamsJson->TryGetObjectField(TEXT("input"), InputObject))
	{
		FString ControlId;
		if ((*InputObject)->TryGetStringField(TEXT("controlID"), ControlId))
		{
			FString EventType;
			if ((*InputObject)->TryGetStringField(TEXT("event"), EventType))
			{
				OnCustomControlInput().Broadcast(*ControlId, *EventType, nullptr, InputObject->ToSharedRef());
			}
		}
	}
}

void FMixerInteractivityModule::UpdateRemoteControl(FName SceneName, FName ControlName, TSharedRef<FJsonObject> PropertiesToUpdate)
{
	// @TODO - centralize field name constants
	static const FString ControlIdField = TEXT("controlID");

	FString ControlNameString = ControlName.ToString();

	TArray<TSharedPtr<FJsonValue>>& ControlsForScene = PendingControlUpdates.FindOrAdd(SceneName);
	for (TSharedPtr<FJsonValue>& ExistingControlUpdate : ControlsForScene)
	{
		TSharedPtr<FJsonObject> UpdateObject = ExistingControlUpdate->AsObject();
		if (UpdateObject->GetStringField(ControlIdField) == ControlNameString)
		{
			UpdateObject->Values.Append(PropertiesToUpdate->Values);
			return;
		}
	}

	PropertiesToUpdate->SetStringField(ControlIdField, ControlName.ToString());
	ControlsForScene.Add(MakeShared<FJsonValueObject>(PropertiesToUpdate));
}

void FMixerInteractivityModule::FlushControlUpdates()
{
	for (TMap<FName, TArray<TSharedPtr<FJsonValue>>>::TIterator It(PendingControlUpdates); It; ++It)
	{
		TSharedRef<FJsonObject> UpdateMethodParams = MakeShared<FJsonObject>();

		// Special case - 'default' is used all over the place as a name, but with 'D'
		UpdateMethodParams->SetStringField(TEXT("sceneID"), It->Key != NAME_DefaultMixerParticipantGroup ? It->Key.ToString() : TEXT("default"));
		UpdateMethodParams->SetArrayField(TEXT("controls"), It->Value);

		CallRemoteMethod(TEXT("updateControls"), UpdateMethodParams);
	}

	PendingControlUpdates.Empty();
}

TSharedPtr<IOnlineChat> FMixerInteractivityModule::GetChatInterface()
{
	return ChatInterface;
}

TSharedPtr<IOnlineChatMixer> FMixerInteractivityModule::GetExtendedChatInterface()
{
	return ChatInterface;
}

void FMixerInteractivityModule::SetInteractiveConnectionAuthState(EMixerLoginState InState)
{
	// Check for illegal transitions (indicate a logic error in plugin code)
	switch (InteractiveConnectionAuthState)
	{
	case EMixerLoginState::Not_Logged_In:
		check(InState != EMixerLoginState::Logged_In);
		check(InState != EMixerLoginState::Logging_Out);
		break;
	case EMixerLoginState::Logging_In:
		check(InState != EMixerLoginState::Logging_Out);
		break;
	case EMixerLoginState::Logged_In:
		check(InState != EMixerLoginState::Logging_In);
		break;
	case EMixerLoginState::Logging_Out:
		check(InState != EMixerLoginState::Logging_In);
		check(InState != EMixerLoginState::Logged_In);
		break;
	default:
		break;
	}

	// Outside the editor we should run a full logout if we lose the interactive connection.
	if (!GIsEditor)
	{
		if (InteractiveConnectionAuthState != EMixerLoginState::Not_Logged_In && InState == EMixerLoginState::Not_Logged_In)
		{
			Logout();
		}
	}

	EMixerLoginState PreviousFullLoginState = GetLoginState();
	InteractiveConnectionAuthState = InState;
	HandleLoginStateChange(PreviousFullLoginState, GetLoginState());
}

void FMixerInteractivityModule::SetUserAuthState(EMixerLoginState InState)
{
	// Check for illegal transitions (indicate a logic error in plugin code)
	switch (UserAuthState)
	{
	case EMixerLoginState::Not_Logged_In:
		check(InState != EMixerLoginState::Logged_In);
		check(InState != EMixerLoginState::Logging_Out);
		break;
	case EMixerLoginState::Logging_In:
		check(InState != EMixerLoginState::Logging_Out);
		break;
	case EMixerLoginState::Logged_In:
		check(InState != EMixerLoginState::Logging_In);
		break;
	case EMixerLoginState::Logging_Out:
		check(InState != EMixerLoginState::Logging_In);
		check(InState != EMixerLoginState::Logged_In);
		break;
	default:
		break;
	}

	// If we need an interactive connection then kick if off as soon as the user auth portion is completed.
	if (NeedsClientLibraryActive())
	{
		if (UserAuthState == EMixerLoginState::Logging_In && InState == EMixerLoginState::Logged_In)
		{
			StartInteractiveConnection();
		}
	}

	EMixerLoginState PreviousFullLoginState = GetLoginState();
	UserAuthState = InState;
	HandleLoginStateChange(PreviousFullLoginState, GetLoginState());
}

void FMixerInteractivityModule::HandleLoginStateChange(EMixerLoginState OldState, EMixerLoginState NewState)
{
	if (OldState != NewState)
	{
		TSharedPtr<const FUniqueNetId> RetryWithNetId = NetId;
		if (GetUserAuthState() == EMixerLoginState::Not_Logged_In)
		{
			CurrentUser.Reset();
			NetId.Reset();

#if PLATFORM_SUPPORTS_MIXER_OAUTH
			UMixerInteractivityUserSettings* UserSettings = GetMutableDefault<UMixerInteractivityUserSettings>();
			UserSettings->AccessToken = TEXT("");
			UserSettings->RefreshToken = TEXT("");
			UserSettings->SaveConfig();
#endif
#if PLATFORM_XBOXONE
			XboxUserOperation = TFuture<Windows::Xbox::System::User^>();
			GetXTokenOperation = nullptr;
#endif
		}

		switch (OldState)
		{
		case EMixerLoginState::Not_Logged_In:
			check(NewState == EMixerLoginState::Logging_In);
			OnLoginStateChanged().Broadcast(NewState);
			break;

		case EMixerLoginState::Logging_In:
			if (NewState == EMixerLoginState::Logged_In)
			{
				RetryLoginWithUI = false;

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
			else if (RetryLoginWithUI)
			{
				LoginWithUIInternal(RetryWithNetId);
			}
			else
			{
				OnLoginStateChanged().Broadcast(NewState);
			}
			break;

		case EMixerLoginState::Logged_In:
			check(NewState != EMixerLoginState::Logging_In || (GIsEditor && NeedsClientLibraryActive()));
			OnLoginStateChanged().Broadcast(NewState);
			break;

		case EMixerLoginState::Logging_Out:
			check(NewState == EMixerLoginState::Not_Logged_In);
			OnLoginStateChanged().Broadcast(NewState);
			break;
		}
	}
}

#if PLATFORM_XBOXONE
bool FMixerInteractivityModule::LoginSilentlyInternal(TSharedPtr<const FUniqueNetId> UserId)
{
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

	SetUserAuthState(EMixerLoginState::Logging_In);
	NetId = UserId;

	return true;
}

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
			SetUserAuthState(EMixerLoginState::Not_Logged_In);
		}
	}
}

void FMixerInteractivityModule::OnXboxUserRemoved(Windows::Xbox::System::User^ RemovedUser)
{
	if (RemovedUser != nullptr && NetId.IsValid())
	{
		if (NetId->ToString() == RemovedUser->XboxUserId->Data())
		{
			Logout();
		}
	}
}
#elif PLATFORM_NEEDS_OSS_LIVE
bool FMixerInteractivityModule::LoginSilentlyInternal(TSharedPtr<const FUniqueNetId> UserId)
{
	// Non-Xbox platform using XToken auth.  Requires custom version of OnlineSubsystemLive
	IOnlineIdentityPtr IdentityInterface = Online::GetIdentityInterface(nullptr, LIVE_SUBSYSTEM);
	if (!IdentityInterface.IsValid())
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("Currently only Xbox Live XToken signin is supported for non-oauth platforms.  This requires OnlineSubsystemLive."));
		return false;
	}

	FPlatformUserId LocalUserNum = IdentityInterface->GetPlatformUserIdFromUniqueNetId(*UserId);
	if (LocalUserNum < 0 || LocalUserNum > MAX_LOCAL_PLAYERS)
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("Could not map user id %s to a local player index."), *UserId->ToString());
		return false;
	}
	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("https://mixer.com");
	if (!IdentityInterface->Login(LocalUserNum, Credentials))
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("Unexpected error performing XToken retrieval for Mixer login."));
		return false;
	}

	check(LoginCompleteDelegateHandle[LocalUserNum].IsValid());
	
	SetUserAuthState(EMixerLoginState::Logging_In);
	NetId = UserId;	
	
	return true;
}

void FMixerInteractivityModule::OnXTokenRetrievalComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& ErrorMessage)
{
	if (UserId == *NetId && UserAuthState == EMixerLoginState::Logging_In)
	{
		bool MovedToNextLoginPhase = false;
		if (bWasSuccessful)
		{
			IOnlineIdentityPtr IdentityInterface = Online::GetIdentityInterface(nullptr, LIVE_SUBSYSTEM);
			check(IdentityInterface.IsValid());
			TSharedPtr<FUserOnlineAccount> UserAccount = IdentityInterface->GetUserAccount(UserId);
			if (UserAccount.IsValid())
			{
				FString XToken;
				if (UserAccount->GetAuthAttribute(TEXT("https://mixer.com"), XToken))
				{
					UMixerInteractivityUserSettings* UserSettings = GetMutableDefault<UMixerInteractivityUserSettings>();
					UserSettings->AccessToken = *XToken;

					TSharedRef<IHttpRequest> UserRequest = FHttpModule::Get().CreateRequest();
					UserRequest->SetVerb(TEXT("GET"));
					UserRequest->SetURL(TEXT("https://mixer.com/api/v1/users/current"));
					UserRequest->SetHeader(TEXT("Authorization"), XToken);
					UserRequest->OnProcessRequestComplete().BindRaw(this, &FMixerInteractivityModule::OnUserRequestComplete);
					MovedToNextLoginPhase = UserRequest->ProcessRequest();
				}
			}
		}

		if (!MovedToNextLoginPhase)
		{
			SetUserAuthState(EMixerLoginState::Not_Logged_In);
		}
	}
}

#endif