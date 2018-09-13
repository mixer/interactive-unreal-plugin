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

#include "Modules/ModuleManager.h"

struct FMixerUser;
struct FMixerLocalUser;
struct FMixerRemoteUser;
struct FMixerButtonDescription;
struct FMixerButtonState;
struct FMixerStickDescription;
struct FMixerStickState;
struct FMixerLabelDescription;
struct FMixerTextboxDescription;
struct FMixerButtonEventDetails;
struct FMixerTextboxEventDetails;
class FUniqueNetId;
class FJsonObject;

enum class EMixerLoginState : uint8;
enum class EMixerInteractivityParticipantState : uint8;
enum class EMixerInteractivityState : uint8;

/**
* Interface for Mixer Interactivity features.
* Exposes user auth, state control, and remote events for consumption by C++ game code.
*/
class IMixerInteractivityModule : public IModuleInterface
{
public:

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return					Returns singleton instance, loading the module on demand if needed
	*/
	static inline IMixerInteractivityModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IMixerInteractivityModule>("MixerInteractivity");
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return					True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("MixerInteractivity");
	}

public:
	/**
	* Sign a local user into the Mixer service without displaying UI.  The operation will fail if user interaction
	* would be required to complete sign-in.  The operation takes place asynchronously, with changes reported via
	* the OnLoginStateChanged event.
	* 
	* @param	UserId			Network id for the local user whose identity will be used for Mixer login.
	* 
	* @return					True if a login attempt was started; results will be reported via OnLoginStateChanged.
	*/
	virtual bool LoginSilently(TSharedPtr<const FUniqueNetId> UserId = nullptr) = 0;

	/**
	* Sign a local user into the Mixer service displaying OAuth login UI if necessary.  UI will be hosted
	* in a default-styled popup window.  The operation takes place asynchronously, with changes reported via
	* the OnLoginStateChanged event.  Not supported on all platforms.
	*
	* @param	UserId			Network id for the local user whose identity will be used for Mixer login
	*
	* @return					True if a login attempt was started; results will be reported via OnLoginStateChanged.
	*/
	virtual bool LoginWithUI(TSharedPtr<const FUniqueNetId> UserId = nullptr) = 0;

	/**
	* Sign a local user into the Mixer service using an authorization code obtained by the title through
	* some external mechanism (e.g. Mixer shortcode auth flow).  The operation takes place asynchronously, 
	* with changes reported via the OnLoginStateChanged event.
	*
	* @param	AuthCode		The title-obtained OAuth authorization code 
	* @param	UserId			Network id for the local user whose identity will be used for Mixer login
	*
	* @return					True if a login attempt was started; results will be reported via OnLoginStateChanged.
	*/
	virtual bool LoginWithAuthCode(const FString& AuthCode, TSharedPtr<const FUniqueNetId> UserId = nullptr) = 0;

	/**
	* Sign the current Mixer user, if any, out of the Mixer service
	*
	* @return					True 
	*/
	virtual bool Logout() = 0;

	/**
	* Reports whether a user is currently logged in to Mixer, or if a login/logout operation is in progress.
	*
	* @return					See EMixerLoginState.
	*/
	virtual EMixerLoginState GetLoginState() = 0;

	/**
	* Notify the Mixer service that the game is ready for interactive input.  The operation takes
	* place asynchronously, with changes reported via the OnInteractivityStateChanged event.
	*/
	virtual void StartInteractivity() = 0;

	/**
	* Notify the Mixer service that the game is no longer accepting interactive input.  The operation takes
	* place asynchronously, with changes reported via the OnInteractivityStateChanged event.
	*/
	virtual void StopInteractivity() = 0;

	/**
	* Reports the current interactivity state.
	*
	* @return					See EMixerInteractivityState.
	*/
	virtual EMixerInteractivityState GetInteractivityState() = 0;

	/**
	* Request a change in the interactive scene displayed to remote users.
	*
	* @param	Scene			Name of the new interactive scene to display.
	* @param	GroupName		Name of the group to whom the new scene should be shown.  Default group if empty.
	*/
	virtual void SetCurrentScene(FName Scene, FName GroupName = NAME_None) = 0;

	/**
	* Gets the name of the interactive scene currently displayed to remote users.
	*
	* @param	GroupName		Name of the group whose scene should be returned.  Default group if empty.
	*
	* @Return					Name if the interactive scene currently displayed.
	*/
	virtual FName GetCurrentScene(FName GroupName = NAME_None) = 0;

	/**
	* Request that the named button enter a cooldown state for the specified period.  While cooling down
	* the button will be non-interactive.
	*
	* @param	Button			Name of the button that should be on cooldown.
	* @param	CooldownTime	Duration for which the button should be non-interactive.
	*/
	virtual void TriggerButtonCooldown(FName Button, FTimespan CooldownTime) = 0;

	/**
	* Retrieve information about a named button that is independent of its current state.
	* See FMixerButtonDescription for details.
	*
	* @param	Button			Name of the button for which information should be returned.
	* @param	OutDesc			Out parameter filled in with information about the button upon success.
	*
	* @Return					True if button was found and OutDesc is valid.
	*/
	virtual bool GetButtonDescription(FName Button, FMixerButtonDescription& OutDesc) = 0;

	/**
	* Retrieve information about a named button that is dependent on remote user and title interactions.
	* See FMixerButtonState for details.
	* This overload reports the aggregate state of the button over all participants.
	*
	* @param	Button			Name of the button for which information should be returned.
	* @param	OutState		Out parameter filled in with information about the button upon success.
	*
	* @Return					True if button was found and OutState is valid.
	*/
	virtual bool GetButtonState(FName Button, FMixerButtonState& OutState) = 0;

	/**
	* Retrieve information about a named button that is dependent on remote user and title interactions.
	* See FMixerButtonState for details.
	* This overload reports the state of the button for a single participant.
	*
	* @param	Button			Name of the button for which information should be returned.
	* @param	ParticipantId	Mixer id of the remote user whose view of the button should be returned.
	* @param	OutState		Out parameter filled in with information about the button upon success.
	*
	* @Return					True if button was found and OutState is valid.
	*/
	virtual bool GetButtonState(FName Button, uint32 ParticipantId, FMixerButtonState& OutState) = 0;

	/**
	* Retrieve information about a named joystick that is independent of its current state.
	* See FMixerStickDescription for details.
	*
	* @param	Stick			Name of the joystick for which information should be returned.
	* @param	OutDesc			Out parameter filled in with information about the joystick upon success.
	*
	* @Return					True if joystick was found and OutDesc is valid.
	*/
	virtual bool GetStickDescription(FName Stick, FMixerStickDescription& OutDesc) = 0;

	/**
	* Retrieve information about a named joystick that is dependent on remote user and title interactions.
	* See FMixerStickState for details.
	* This overload reports the aggregate state of the stick over all participants.
	*
	* @param	Stick			Name of the joystick for which information should be returned.
	* @param	OutState		Out parameter filled in with information about the joystick upon success.
	*
	* @Return					True if joystick was found and OutState is valid.
	*/
	virtual bool GetStickState(FName Stick, FMixerStickState& OutState) = 0;

	/**
	* Retrieve information about a named joystick that is dependent on remote user and title interactions.
	* See FMixerStickState for details.
	* This overload reports the state of the stick for a single participant.
	*
	* @param	Stick			Name of the joystick for which information should be returned.
	* @param	ParticipantId	Mixer id of the remote user whose view of the stick should be returned.
	* @param	OutState		Out parameter filled in with information about the joystick upon success.
	*
	* @Return					True if joystick was found and OutState is valid.
	*/
	virtual bool GetStickState(FName Stick, uint32 ParticipantId, FMixerStickState& OutState) = 0;

	/**
	* Change the text that will be displayed to remote users on the named label.
	*
	* @param	Label			Name of the label for which text should be set.
	* @param	DisplayText		New text to display on the label.
	*/
	virtual void SetLabelText(FName Label, const FText& DisplayText) = 0;

	/**
	* Retrieve information about properties of a label that are configured at design time and
	* are expected to change infrequently (or not at all) during runtime.
	* See FMixerLabelDescription for details.
	*
	* @param	Label			Name of the label for which information should be returned.
	* @param	OutDesc			Out parameter filled in with information about the label upon success.
	*
	* @Return					True if label was found and OutDesc is valid.
	*/
	virtual bool GetLabelDescription(FName Label, FMixerLabelDescription& OutDesc) = 0;

	/**
	* Retrieve information about properties of a textbox that are configured at design time and
	* are expected to change infrequently (or not at all) during runtime.
	* See FMixerTextboxDescription for details.
	*
	* @param	Textbox			Name of the textbox for which information should be returned.
	* @param	OutDesc			Out parameter filled in with information about the textbox upon success.
	*
	* @Return					True if textbox was found and OutDesc is valid.
	*/
	virtual bool GetTextboxDescription(FName Textbox, FMixerTextboxDescription& OutDesc) = 0;

	/**
	* Retrieve information about a named custom control.  Information may include both static 
	* (similar to Get*Description methods above) and dynamic (similar to Get*State methods) data.
	* There is no concept of data being isolated per participant.
	* This overload expects the named control to be an unmapped custom control, that is, not mapped
	* directly on the client to a custom type derived from UMixerCustomControl.
	*
	* @param	ForWorld			World context for this query.  Only game worlds have fully instantiated custom controls.
	* @param	ControlName			Name of the control for which information should be returned.
	* @param	OutControlObject	Json property bag describing the control.
	*
	* @Return						True if the control was found as an unmapped control.
	*/
	virtual bool GetCustomControl(UWorld* ForWorld, FName ControlName, TSharedPtr<FJsonObject>& OutControlObject) = 0;

	/**
	* Retrieve information about a named custom control.  Information may include both static
	* (similar to Get*Description methods above) and dynamic (similar to Get*State methods) data.
	* There is no concept of data being isolated per participant.
	* This overload expects the named control to be mapped directly on the client to a custom type
	* derived from UMixerCustomControl.
	*
	* @param	ForWorld			World context for this query.  Only game worlds have fully instantiated custom controls.
	* @param	ControlName			Name of the control for which information should be returned.
	* @param	OutControlObject	See UMixerCustomControl.
	*
	* @Return						True if the control was found as a mapped control.
	*/
	virtual bool GetCustomControl(UWorld* ForWorld, FName ControlName, class UMixerCustomControl*& OutControlObject) = 0;

	/**
	* Retrieve a structure describing the local user currently signed in to the Mixer service.
	*
	* @Return					See FMixerLocalUser.  Invalid shared pointer when no user is signed in.
	*/
	virtual TSharedPtr<const FMixerLocalUser> GetCurrentUser() = 0;

	/**
	* Retrieve a structure describing a remote user currently interacting with the title on the Mixer service.
	*
	* @param	ParticipantId	Mixer id of the remote user.  
	* @Return					See FMixerRemoteUser.  Invalid shared pointer when no user could be found.
	*/
	virtual TSharedPtr<const FMixerRemoteUser> GetParticipant(uint32 ParticipantId) = 0;

	/**
	* Create a user group with the given name.  Groups may be used to show different scenes to different
	* segments of the participating audience.  No-op if the group already exists.
	*
	* @param	GroupName		Name for the new group.
	* @param	InitialScene	Interactive scene that this new group should see.
	*
	* @Return					True if group is newly created.  False if it already existed (interactive scene will not be changed).
	*/
	virtual bool CreateGroup(FName GroupName, FName InitialScene) = 0;

	/**
	* Retrieve the collection of participants that belong to the named group.
	*
	* @param	GroupName		Name of the group for which to retrieve participants.
	* @param	OutParticipants	Out parameter filled with the members of the named group.
	*
	* @Return					True if the group exists (even if it is empty).  False otherwise.
	*/
	virtual bool GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants) = 0;

	/**
	* Move a single participant to the named group.
	*
	* @param	GroupName		Name of the group to which the participant should be moved.
	* @param	ParticipantId	Id of the user to be moved.
	*
	* @Return					True if the destination group exists.  False otherwise.
	*/
	virtual bool MoveParticipantToGroup(FName GroupName, uint32 ParticipantId) = 0;

	/**
	* Captures a given interactive event transaction, charging the sparks to the appropriate remote participant. 
	*
	* @param	TransactionId	Id of the transaction for which sparks should be charged (obtained from event)
	*/
	virtual void CaptureSparkTransaction(const FString& TransactionId) = 0;

	virtual void UpdateRemoteControl(FName SceneName, FName ControlName, TSharedRef<FJsonObject> PropertiesToUpdate) = 0;

	virtual void CallRemoteMethod(const FString& MethodName, const TSharedRef<FJsonObject> MethodParams) = 0;

	/**
	* Get access to Mixer chat via UE's standard IOnlineChat interface.
	* Sending messages requires a logged in user.
	*
	* @Return					See IOnlineChat.
	*/
	virtual TSharedPtr<class IOnlineChat> GetChatInterface() = 0;

	/**
	* Get access to Mixer chat via IOnlineChatMixer which supports additional
	* interactions that extend the standard IOnlineChat interface.
	* Sending messages requires a logged in user.
	*
	* @Return					See IOnlineChatMixer.
	*/
	virtual TSharedPtr<class IOnlineChatMixer> GetExtendedChatInterface() = 0;

	// Events
	DECLARE_EVENT_OneParam(IMixerInteractivityModule, FOnLoginStateChanged, EMixerLoginState);
	virtual FOnLoginStateChanged& OnLoginStateChanged() = 0;

	DECLARE_EVENT_OneParam(IMixerInteractivityModule, FOnInteractivityStateChanged, EMixerInteractivityState);
	virtual FOnInteractivityStateChanged& OnInteractivityStateChanged() = 0;

	DECLARE_EVENT_TwoParams(IMixerInteractivityModule, FOnParticipantStateChangedEvent, TSharedPtr<const FMixerRemoteUser>, EMixerInteractivityParticipantState);
	virtual FOnParticipantStateChangedEvent& OnParticipantStateChanged() = 0;

	DECLARE_EVENT_ThreeParams(IMixerInteractivityModule, FOnButtonEvent, FName, TSharedPtr<const FMixerRemoteUser>, const FMixerButtonEventDetails&);
	virtual FOnButtonEvent& OnButtonEvent() = 0;

	DECLARE_EVENT_ThreeParams(IMixerInteractivityModule, FOnStickEvent, FName, TSharedPtr<const FMixerRemoteUser>, FVector2D);
	virtual FOnStickEvent& OnStickEvent() = 0;

	DECLARE_EVENT_ThreeParams(IMixerInteractivityModule, FOnTextboxSubmitEvent, FName, TSharedPtr<const FMixerRemoteUser>, const FMixerTextboxEventDetails&);
	virtual FOnTextboxSubmitEvent& OnTextboxSubmitEvent() = 0;

	DECLARE_EVENT_OneParam(IMixerInteractivityModule, FOnBroadcastingStateChanged, bool);
	virtual FOnBroadcastingStateChanged& OnBroadcastingStateChanged() = 0;

	DECLARE_EVENT_FourParams(IMixerInteractivityModule, FOnCustomControlInput, FName, FName, TSharedPtr<const FMixerRemoteUser>, const TSharedRef<FJsonObject>);
	virtual FOnCustomControlInput& OnCustomControlInput() = 0;

	DECLARE_EVENT_TwoParams(IMixerInteractivityModule, FOnCustomControlPropertyUpdate, FName, const TSharedRef<FJsonObject>);
	virtual FOnCustomControlPropertyUpdate& OnCustomControlPropertyUpdate() = 0;

	DECLARE_EVENT_TwoParams(IMixerInteractivityModule, FOnCustomMethodCall, FName, const TSharedPtr<FJsonObject>);
	virtual FOnCustomMethodCall& OnCustomMethodCall() = 0;
};