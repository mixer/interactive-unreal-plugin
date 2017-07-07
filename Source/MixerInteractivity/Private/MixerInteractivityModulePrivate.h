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
#include <memory>

namespace Microsoft
{
	namespace mixer
	{
		class interactive_button_control;
		class interactive_joystick_control;
		class interactive_participant;
	}
}

struct FMixerLocalUserJsonSerializable : public FMixerLocalUser, public FJsonSerializable
{
public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("username", Name);
		JSON_SERIALIZE("id", Id);
		JSON_SERIALIZE("level", Level);
		JSON_SERIALIZE("experience", Experience);
		JSON_SERIALIZE("sparks", Sparks);
	END_JSON_SERIALIZER
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
	virtual bool GetStickDescription(FName Stick, FMixerStickDescription& OutDesc);
	virtual bool GetStickState(FName Stick, FMixerStickState& OutState);

	virtual TSharedPtr<const FMixerLocalUser> GetCurrentUser()
	{
		return CurrentUser;
	}
	virtual TSharedPtr<const FMixerRemoteUser> GetParticipant(uint32 ParticipantId);

	virtual bool CreateGroup(FName GroupName, FName InitialScene = NAME_None);
	virtual bool GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants);
	virtual bool MoveParticipantToGroup(FName GroupName, uint32 ParticipantId);
	virtual void CaptureSparkTransaction(const FString& TransactionId);

	virtual FOnLoginStateChanged& OnLoginStateChanged()						{ return LoginStateChanged; }
	virtual FOnInteractivityStateChanged& OnInteractivityStateChanged()		{ return InteractivityStateChanged; }
	virtual FOnParticipantStateChangedEvent& OnParticipantStateChanged()	{ return ParticipantStateChanged; }
	virtual FOnButtonEvent& OnButtonEvent()									{ return ButtonEvent; }
	virtual FOnStickEvent& OnStickEvent()									{ return StickEvent; }

public:

	virtual bool Tick(float DeltaTime);

private:
	bool LoginWithAuthCodeInternal(const FString& AuthCode, TSharedPtr<const FUniqueNetId> UserId);

	void OnTokenRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void OnUserRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	void OnBrowserUrlChanged(const FText& NewUrl);
	void OnBrowserWindowClosed(const TSharedRef<SWindow>&);
	bool OnBrowserPopupWindow(const TWeakPtr<IWebBrowserWindow>& NewBrowserWindow, const TWeakPtr<IWebBrowserPopupFeatures>& PopupFeatures);

	bool NeedsClientLibraryActive();
	void InitDesignTimeGroups();

	std::shared_ptr<Microsoft::mixer::interactive_button_control> FindButton(FName Name);
	std::shared_ptr<Microsoft::mixer::interactive_joystick_control> FindStick(FName Name);
	TSharedPtr<FMixerRemoteUserCached> CreateOrUpdateCachedParticipant(std::shared_ptr<Microsoft::mixer::interactive_participant> Participant);

	void TickParticipantCacheMaintenance();
	void TickClientLibrary();

	void LoginAttemptFinished(bool Success);

	FString GetRedirectUri();

private:

#if PLATFORM_XBOXONE
	TFuture<Windows::Xbox::System::User^> PlatformUser;
#endif

	TSharedPtr<SWindow> LoginWindow;
	TSharedPtr<SOverlay> LoginBrowserPanes;

	TSharedPtr<const FUniqueNetId> NetId;
	TSharedPtr<FMixerLocalUserJsonSerializable> CurrentUser;

	TMap<uint32, TSharedPtr<FMixerRemoteUserCached>> RemoteParticipantCache;

	EMixerLoginState UserAuthState;
	EMixerInteractivityState InteractivityState;

	FOnLoginStateChanged LoginStateChanged;
	FOnInteractivityStateChanged InteractivityStateChanged;
	FOnParticipantStateChangedEvent ParticipantStateChanged;
	FOnButtonEvent ButtonEvent;
	FOnStickEvent StickEvent;

	bool RetryLoginWithUI;
	bool HasCreatedGroups;
};