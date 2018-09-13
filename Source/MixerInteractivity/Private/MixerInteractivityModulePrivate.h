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

#include "MixerInteractivityModule.h"
#include "MixerInteractivityTypes.h"
#include "Containers/Ticker.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonTypes.h"
#include "DOM/JsonValue.h"
#include "DOM/JsonObject.h"
#include "Policies/JsonPrintPolicy.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonSerializerMacros.h"
#include "Async/Future.h"
#include "Input/Reply.h"
#include "UObject/CoreOnline.h"
#include <memory>

#define PLATFORM_NEEDS_OSS_LIVE !PLATFORM_XBOXONE && !PLATFORM_SUPPORTS_MIXER_OAUTH

#if PLATFORM_NEEDS_OSS_LIVE
#include "OnlineSubsystemTypes.h"
#endif

struct FMixerChannelJsonSerializable : public FMixerChannel, public FJsonSerializable
{
public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("id", Id);
		JSON_SERIALIZE("name", Name);
		JSON_SERIALIZE("viewersCurrent", CurrentViewers);
		JSON_SERIALIZE("viewersTotal", LifetimeUniqueViewers);
		JSON_SERIALIZE("numFollowers", Followers);
		JSON_SERIALIZE("online", IsBroadcasting);
	END_JSON_SERIALIZER
};

struct FMixerLocalUserJsonSerializable : public FMixerLocalUser, public FJsonSerializable
{
public:
	FMixerChannelJsonSerializable Channel;
	double RefreshAtAppTime;

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("username", Name);
		JSON_SERIALIZE("id", Id);
		JSON_SERIALIZE("level", Level);
		JSON_SERIALIZE("experience", Experience);
		JSON_SERIALIZE("sparks", Sparks);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("channel", Channel);
	END_JSON_SERIALIZER

	virtual const FMixerChannel& GetChannel() const override { return Channel; }

	FMixerLocalUserJsonSerializable()
		: RefreshAtAppTime(0.0)
	{
	}
};

class SWindow;
class SOverlay;
class IWebBrowserWindow;
class IWebBrowserPopupFeatures;
class UMixerInteractivityBlueprintEventSource;

class FMixerInteractivityModule :
	public IMixerInteractivityModule,
	public FTickerObjectBase
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	virtual bool LoginSilently(TSharedPtr<const FUniqueNetId> UserId);
	virtual bool LoginWithUI(TSharedPtr<const FUniqueNetId> UserId);
	virtual bool LoginWithAuthCode(const FString& AuthCode, TSharedPtr<const FUniqueNetId> UserId);
	virtual bool Logout();
	virtual EMixerLoginState GetLoginState();

	virtual EMixerInteractivityState GetInteractivityState();

	virtual bool GetCustomControl(UWorld* ForWorld, FName ControlName, TSharedPtr<FJsonObject>& OutControlObject);
	virtual bool GetCustomControl(UWorld* ForWorld, FName ControlName, class UMixerCustomControl*& OutControlObject);
	virtual TSharedPtr<const FMixerLocalUser> GetCurrentUser()				{ return CurrentUser; }

	virtual TSharedPtr<class IOnlineChat> GetChatInterface();
	virtual TSharedPtr<class IOnlineChatMixer> GetExtendedChatInterface();

	virtual FOnLoginStateChanged& OnLoginStateChanged()							{ return LoginStateChanged; }
	virtual FOnInteractivityStateChanged& OnInteractivityStateChanged()			{ return InteractivityStateChanged; }
	virtual FOnParticipantStateChangedEvent& OnParticipantStateChanged()		{ return ParticipantStateChanged; }
	virtual FOnButtonEvent& OnButtonEvent()										{ return ButtonEvent; }
	virtual FOnStickEvent& OnStickEvent()										{ return StickEvent; }
	virtual FOnBroadcastingStateChanged& OnBroadcastingStateChanged()			{ return BroadcastingStateChanged; }
	virtual FOnCustomControlInput& OnCustomControlInput()						{ return CustomControlInputEvent; }
	virtual FOnCustomControlPropertyUpdate& OnCustomControlPropertyUpdate()		{ return CustomControlPropertyUpdate; }
	virtual FOnCustomMethodCall& OnCustomMethodCall()							{ return CustomMethodCall; }
	virtual FOnTextboxSubmitEvent& OnTextboxSubmitEvent()						{ return TextboxSubmitEvent; }

public:
	virtual bool Tick(float DeltaTime);

public:
	void UpdateRemoteControl(FName SceneName, FName ControlName, TSharedRef<FJsonObject> PropertiesToUpdate);

protected:
	virtual bool StartInteractiveConnection() = 0;
	virtual void StopInteractiveConnection() = 0;
	EMixerLoginState GetInteractiveConnectionAuthState() const			{ return InteractiveConnectionAuthState; }
	void SetInteractiveConnectionAuthState(EMixerLoginState InState);
	EMixerInteractivityState GetInteractivityState() const				{ return InteractivityState; }
	void SetInteractivityState(EMixerInteractivityState InState)		{ InteractivityState = InState; InteractivityStateChanged.Broadcast(InState); }
#if PLATFORM_XBOXONE
	Windows::Xbox::System::User^ GetXboxUser()							{ return XboxUserOperation.Get(); }
#endif

	bool HandleControlUpdateMessage(FJsonObject* ParamsJson);
	void HandleCustomControlInputMessage(FJsonObject* ParamsJson);

	virtual bool HandleSingleControlUpdate(FName ControlId, const TSharedRef<FJsonObject> ControlData) { return false; }

private:
	EMixerLoginState GetUserAuthState() const { return UserAuthState; }
	void SetUserAuthState(EMixerLoginState InState);
	void HandleLoginStateChange(EMixerLoginState OldState, EMixerLoginState NewState);

	bool LoginSilentlyInternal(TSharedPtr<const FUniqueNetId> UserId);
	void LoginWithUIInternal(TSharedPtr<const FUniqueNetId> UserId);
	bool LoginWithAuthCodeInternal(const FString& AuthCode, TSharedPtr<const FUniqueNetId> UserId);

	void OnTokenRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void OnUserRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void OnUserMaintenanceRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	FReply OnAuthCodeReady(const FString& AuthCode);
	void OnLoginUIFlowFinished(bool WasSuccessful);
	void OnLoginWindowClosed(const TSharedRef<SWindow>&);

	bool NeedsClientLibraryActive();
	void InitDesignTimeGroups();

	void TickLocalUserMaintenance();
	void FlushControlUpdates();

private:

#if PLATFORM_XBOXONE
	TFuture<Windows::Xbox::System::User^> XboxUserOperation;
	Windows::Foundation::IAsyncOperation<Windows::Xbox::System::GetTokenAndSignatureResult^>^ GetXTokenOperation;
	void TickXboxLogin();
	void OnXboxUserRemoved(Windows::Xbox::System::User^ RemovedUser);
#elif !PLATFORM_SUPPORTS_MIXER_OAUTH
	void OnXTokenRetrievalComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& ErrorMessage);
	FDelegateHandle LoginCompleteDelegateHandle[MAX_LOCAL_PLAYERS];
#endif

	TSharedPtr<SWindow> LoginWindow;
	TSharedPtr<SOverlay> LoginBrowserPanes;

	TSharedPtr<const FUniqueNetId> NetId;
	TSharedPtr<FMixerLocalUserJsonSerializable> CurrentUser;

	EMixerLoginState UserAuthState;
	EMixerLoginState InteractiveConnectionAuthState;
	EMixerInteractivityState InteractivityState;

	FOnLoginStateChanged LoginStateChanged;
	FOnInteractivityStateChanged InteractivityStateChanged;
	FOnParticipantStateChangedEvent ParticipantStateChanged;
	FOnButtonEvent ButtonEvent;
	FOnStickEvent StickEvent;
	FOnBroadcastingStateChanged BroadcastingStateChanged;
	FOnCustomControlInput CustomControlInputEvent;
	FOnCustomControlPropertyUpdate CustomControlPropertyUpdate;
	FOnCustomMethodCall CustomMethodCall;
	FOnTextboxSubmitEvent TextboxSubmitEvent;

	TSharedPtr<class FOnlineChatMixer> ChatInterface;

	TMap<FName, TArray<TSharedPtr<FJsonValue>>> PendingControlUpdates;

	bool RetryLoginWithUI;
};