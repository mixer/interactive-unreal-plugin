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

namespace Microsoft
{
	namespace mixer
	{
		class interactive_button_control;
		class interactive_joystick_control;
		class interactive_participant;
		enum interactivity_state : int;
	}
}

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

struct FMixerRemoteUserCached : public FMixerRemoteUser
{
public:
	FMixerRemoteUserCached(std::shared_ptr<Microsoft::mixer::interactive_participant> InParticipant);

	void UpdateFromSourceParticipant();

	std::shared_ptr<Microsoft::mixer::interactive_participant> GetSourceParticipant() { return SourceParticipant; }
private:
	std::shared_ptr<Microsoft::mixer::interactive_participant> SourceParticipant;
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

public:
	virtual bool LoginSilently(TSharedPtr<const FUniqueNetId> UserId);
	virtual bool LoginWithUI(TSharedPtr<const FUniqueNetId> UserId);
	virtual bool LoginWithAuthCode(const FString& AuthCode, TSharedPtr<const FUniqueNetId> UserId);
	virtual bool Logout();
	virtual EMixerLoginState GetLoginState();

	virtual void StartInteractivity();
	virtual void StopInteractivity();
	virtual EMixerInteractivityState GetInteractivityState();

	virtual void SetCurrentScene(FName Scene, FName GroupName = NAME_None);
	virtual FName GetCurrentScene(FName GroupName = NAME_None);
	virtual void TriggerButtonCooldown(FName Button, FTimespan CooldownTime);
	virtual bool GetButtonDescription(FName Button, FMixerButtonDescription& OutDesc);
	virtual bool GetButtonState(FName Button, FMixerButtonState& OutState);
	virtual bool GetButtonState(FName Button, uint32 ParticipantId, FMixerButtonState& OutState);
	virtual bool GetStickDescription(FName Stick, FMixerStickDescription& OutDesc);
	virtual bool GetStickState(FName Stick, FMixerStickState& OutState);
	virtual bool GetStickState(FName Stick, uint32 ParticipantId, FMixerStickState& OutState);
	virtual bool GetCustomControl(UWorld* ForWorld, FName ControlName, TSharedPtr<FJsonObject>& OutControlObject);
	virtual bool GetCustomControl(UWorld* ForWorld, FName ControlName, class UMixerCustomControl*& OutControlObject);

	virtual TSharedPtr<const FMixerLocalUser> GetCurrentUser()
	{
		return CurrentUser;
	}
	virtual TSharedPtr<const FMixerRemoteUser> GetParticipant(uint32 ParticipantId);

	virtual bool CreateGroup(FName GroupName, FName InitialScene = NAME_None);
	virtual bool GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants);
	virtual bool MoveParticipantToGroup(FName GroupName, uint32 ParticipantId);
	virtual void CaptureSparkTransaction(const FString& TransactionId);

	virtual void UpdateRemoteControl(FName SceneName, FName ControlName, TSharedRef<FJsonObject> PropertiesToUpdate);
	virtual void CallRemoteMethod(const FString& MethodName, const TSharedRef<FJsonObject> MethodParams);

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

private:
	bool LoginWithAuthCodeInternal(const FString& AuthCode, TSharedPtr<const FUniqueNetId> UserId);

	void OnTokenRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void OnUserRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void OnUserMaintenanceRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	FReply OnAuthCodeReady(const FString& AuthCode);
	void OnLoginUIFlowFinished(bool WasSuccessful);
	void OnLoginWindowClosed(const TSharedRef<SWindow>&);

	bool NeedsClientLibraryActive();
	void InitDesignTimeGroups();

	void HandleCustomMessage(const FString& MessageBodyString);
	void HandleCustomControlUpdateMessage(FJsonObject* ParamsJson);
	void HandleCustomControlInputMessage(FJsonObject* ParamsJson);

	std::shared_ptr<Microsoft::mixer::interactive_button_control> FindButton(FName Name);
	std::shared_ptr<Microsoft::mixer::interactive_joystick_control> FindStick(FName Name);
	TSharedPtr<FMixerRemoteUserCached> CreateOrUpdateCachedParticipant(std::shared_ptr<Microsoft::mixer::interactive_participant> Participant);

	void TickParticipantCacheMaintenance();
	void TickClientLibrary();
	void TickLocalUserMaintenance();
	void FlushControlUpdates();

	void LoginAttemptFinished(bool Success);

private:

#if PLATFORM_XBOXONE
	TFuture<Windows::Xbox::System::User^> PlatformUser;
	Windows::Foundation::IAsyncOperation<Windows::Xbox::System::GetTokenAndSignatureResult^>^ GetXTokenOperation;
	void TickXboxLogin();
#endif

	TSharedPtr<SWindow> LoginWindow;
	TSharedPtr<SOverlay> LoginBrowserPanes;

	TSharedPtr<const FUniqueNetId> NetId;
	TSharedPtr<FMixerLocalUserJsonSerializable> CurrentUser;

	TMap<uint32, TSharedPtr<FMixerRemoteUserCached>> RemoteParticipantCache;

	EMixerLoginState UserAuthState;
	Microsoft::mixer::interactivity_state ClientLibraryState;
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
	bool HasCreatedGroups;
};