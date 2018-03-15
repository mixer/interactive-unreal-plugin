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
#include "Ticker.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonTypes.h"
#include "JsonValue.h"
#include "JsonObject.h"
#include "JsonPrintPolicy.h"
#include "CondensedJsonPrintPolicy.h"
#include "JsonReader.h"
#include "JsonWriter.h"
#include "JsonSerializer.h"
#include "JsonSerializerMacros.h"
#include "Future.h"
#include "Input/Reply.h"
#include "CoreOnline.h"
#include <memory>

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

	virtual void UpdateRemoteControl(FName SceneName, FName ControlName, TSharedRef<FJsonObject> PropertiesToUpdate);

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

public:
	virtual bool Tick(float DeltaTime);

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

	void HandleCustomMessage(const FString& MessageBodyString);
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

	void HandleControlUpdateMessage(FJsonObject* ParamsJson);
	void HandleCustomControlInputMessage(FJsonObject* ParamsJson);

	void TickLocalUserMaintenance();
	void FlushControlUpdates();


private:

#if PLATFORM_XBOXONE
	TFuture<Windows::Xbox::System::User^> XboxUserOperation;
	Windows::Foundation::IAsyncOperation<Windows::Xbox::System::GetTokenAndSignatureResult^>^ GetXTokenOperation;
	void TickXboxLogin();
	void OnXboxUserRemoved(Windows::Xbox::System::User^ RemovedUser);
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

	TSharedPtr<class FOnlineChatMixer> ChatInterface;

	TMap<FName, TArray<TSharedPtr<FJsonValue>>> PendingControlUpdates;

	bool RetryLoginWithUI;
};