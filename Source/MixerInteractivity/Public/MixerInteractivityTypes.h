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

#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "Internationalization/Text.h"
#include "Math/Vector2D.h"
#include "Misc/Guid.h"
#include "Math/Color.h"

/** Base type for all Mixer users */
struct FMixerUser
{
public:
	/** Name for the user, suitable for display in game UI */
	FString Name;

	/** Unique identifier, for internal use */
	int32 Id;

	/** Experience-based level of the user, suitable for display in game UI */
	int32 Level;

	FMixerUser();

	bool operator==(const FMixerUser& Other) const
	{
		return Id == Other.Id;
	}

	bool operator!=(const FMixerUser& Other) const
	{
		return Id != Other.Id;
	}
};

/** Information about a channel owned by a user on Mixer */
struct FMixerChannel
{
public:
	/** A name for the channel, suitable for display in game UI */
	FString Name;

	/** Unique identifier, for internal use */
	int32 Id;

	/** 
	* The number of users currently viewing this channel 
	* Note: this value is periodically updated from the Mixer service.
	*/
	uint32 CurrentViewers;

	/** 
	* The number of unique users who have viewed this channel since its creation 
	* Note: this value is periodically updated from the Mixer service.
	*/
	uint32 LifetimeUniqueViewers;

	/** 
	The number of users who have chosen to follow this channel
	*Note: this value is periodically updated from the Mixer service.
	*/
	uint32 Followers;

	/**
	* Whether or not the channel is currently displaying broadcast video content 
	* Note: when this value changes for the local user's channel the title can 
	* receive a notification via IMixerInteractivityModule::OnBroadcastingStateChanged
	*/
	bool IsBroadcasting;

protected:
	FMixerChannel();
};

/** 
* Represents a user on the local device who is logged in to Mixer
* and potentially operating an interactive session 
*/
struct FMixerLocalUser : public FMixerUser
{
public:
	/** 
	* Experience points measuring progress towards Mixer levels
	* Note: this value is periodically updated from the Mixer service.
	*/
	int32 Experience;

	/** 
	* Sparks owned by the user
	* Note: this value is periodically updated from the Mixer service.
	*/
	int32 Sparks;

	/**
	* Get information about the channel associated with this user, including
	* whether or not it is currently displaying broadcast content.
	*
	* @Return					See FMixerChannel
	*/
	virtual const FMixerChannel& GetChannel() const = 0;

	FMixerLocalUser();
	virtual ~FMixerLocalUser() {}
};

/** Represents a remote user participating in a Mixer interactive session */
struct FMixerRemoteUser : public FMixerUser
{
public:
	/** Time at which the user joined the interactive session */
	FDateTime ConnectedAt;

	/** Time at which the user last submitted interactive input */
	FDateTime InputAt;

	/** Guid for this user's interactive session (appears as participantID in some interactive events) */
	FGuid SessionGuid;

	/** Name of the group that the user belongs to */
	FName Group;

	/** Whether the user is currently able to submit interactive input */
	bool InputEnabled;

	FMixerRemoteUser();
};

/** 
* Represents the Studio-configured properties of a button that
* are immutable during an interactive session 
*/
struct FMixerButtonDescription
{
	/** Text displayed on this button to remote users */
	FText ButtonText;

	/** NOT IMPLEMENTED. Button help text that is displayed to remote users (e.g. as a tooltip). */
	FText HelpText;

	/** Number of Sparks a remote user will be charged for pressing this button */
	uint32 SparkCost;
};

/** 
* Represents the properties of a button that may change during
* an interactive session in response to remote user and/or title interaction.
*/
struct FMixerButtonState
{
	/** Time remaining before the button will be interactive again. */
	FTimespan RemainingCooldown;

	/** NOT IMPLEMENTED */
	float Progress;

	/** Number of remote users currently holding the button down. */
	uint32 DownCount;

	/** Number of remote users who have pressed (down and up) the button in the last interval. */
	uint32 PressCount;

	/** Number of remote users who have released the button in the last interval. */
	uint32 UpCount;

	/** Whether the button is currently enabled. */
	bool Enabled;
};

/** Represents the Studio-configured properties of a joystick that
* are immutable during an interactive session */
struct FMixerStickDescription
{
	/** NOT IMPLEMENTED. Stick help text that is displayed to remote users (e.g. as a tooltip). */
	FText HelpText;
};

/** Represents the properties of a joystick that may change during
* an interactive session in response to remote user and/or title interaction. */
struct FMixerStickState
{
	/** Current aggregate state of the joystick [-1,1] */
	FVector2D Axes;

	/** NOT IMPLEMENTED. Whether the button is currently enabled. */
	bool Enabled;
};

/** Additional information about a button event */
struct FMixerButtonEventDetails
{
	/** 
	* Id for the Spark transaction associated with this button event (empty if none).
	* After handling the event, the charge should be confirmed via
	* IMixerInteractivityModule::CaptureSparkTransaction.
	*/
	FString TransactionId;

	/** Number of sparks that will be charged for this interaction, if confirmed */
	uint32 SparkCost;

	/** Whether the button event represents a press (true) or release (false) */
	bool Pressed;
};

/** Additional information about a textbox event */
struct FMixerTextboxEventDetails
{
	/** Text that was submitted via the textbox */
	FText SubmittedText;

	/**
	* Id for the Spark transaction associated with this textbox event (empty if none).
	* After handling the event, the charge should be confirmed via
	* IMixerInteractivityModule::CaptureSparkTransaction.
	*/
	FString TransactionId;

	/** Number of sparks that will be charged for this interaction, if confirmed */
	uint32 SparkCost;
};

/**
* Represents the Studio-configured properties of a label that
* are expected to change infrequently during an interactive session
*/
struct FMixerLabelDescription
{
	/* Text shown on the label */
	FText Text;

	/* Size of text shown on the label - supports CSS font-size values */
	FString TextSize;

	/* Color of text shown on the label */
	FColor TextColor;

	/* Whether the label text is bold */
	bool Bold;

	/* Whether the label text is underlined */
	bool Underline;

	/* Whether the label text is italicized */
	bool Italic;
};

/**
* Represents the Studio-configured properties of a textbox that
* are expected to change infrequently during an interactive session
*/
struct FMixerTextboxDescription
{
	/* Hint text displayed inside an empty textbox to prompt for user text entry */
	FText Placeholder;

	/* Text displayed on the associated submit button (if in use) */
	FText SubmitText;

	/* Number of Sparks a remote user will be charged for submitting text via this box */
	uint32 SparkCost;

	/* Whether the textbox supports entering multiple lines of text */
	bool Multiline;

	/* Whether the textbox has an associated button that may be pressed to submit text */
	bool HasSubmit;
};

enum class EMixerLoginState : uint8
{
	Not_Logged_In,
	Logging_In,
	Logged_In,
	Logging_Out,
};

enum class EMixerInteractivityState : uint8
{
	Not_Interactive,
	Interactivity_Starting,
	Interactive,
	Interactivity_Stopping,
};

static const FName NAME_DefaultMixerParticipantGroup = "default";