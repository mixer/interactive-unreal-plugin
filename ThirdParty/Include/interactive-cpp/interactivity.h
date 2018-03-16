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

namespace xbox {
    namespace services {
        namespace system {
            class xbox_live_user;
        }
    }
}

#if TV_API | XBOX_UWP
typedef  Windows::Xbox::System::User^ xbox_live_user_t;
#else
typedef std::shared_ptr<xbox::services::system::xbox_live_user> xbox_live_user_t;
#endif

namespace Microsoft {
    /// <summary>
    /// Contains classes and enumerations that let you incorporate
    /// Interactivity functionality into your title.
    /// </summary>
    namespace mixer {

class interactivity_manager_impl;
class interactivity_manager;
class interactive_participant_impl;
class interactive_scene;
class interactive_scene_impl;
class interactive_group;
class interactive_group_impl;
class interactive_control_builder;
class interactive_control;
class interactive_button_control;
class interactive_joystick_control;
class interactivity_mock_util;
class interactive_button_state;
class interactive_joystick_state;
class interactive_button_count;

/// <summary>
/// Enum that describes the types of control objects.
/// </summary>
enum interactive_control_type
{
    /// <summary>
    /// The button control.
    /// </summary>
    button,

    /// <summary>
    /// The joystick control.
    /// </summary>
    joystick
};

/// <summary>
/// Enum that describes the current state of the interactivity service.
/// </summary>
enum interactivity_state
{
    /// <summary>
    /// The interactivity manager is not initialized.
    /// </summary>
    not_initialized,

    /// <summary>
    /// The interactivity manager is initializing.
    /// </summary>
    initializing,

    /// <summary>
    /// The interactivity manager is initialized.
    /// </summary>
    initialized,

    /// <summary>
    /// The interactivity manager is initialized, but interactivity is not enabled.
    /// </summary>
    interactivity_disabled,

    /// <summary>
    /// The interactivity manager is currently connecting to the interactive service.
    /// </summary>
    interactivity_pending,

    /// <summary>
    /// Interactivity is enabled.
    /// </summary>
    interactivity_enabled
};


/// <summary>
/// Enum representing the current state of the participant
/// </summary>
enum interactive_participant_state
{
    /// <summary>
    /// The participant joined the channel.
    /// </summary>
    joined,

    /// <summary>
    /// The participant's input is disabled.
    /// </summary>
    input_disabled,

    /// <summary>
    /// The participant left the channel.
    /// </summary>
    left
};


/// <summary>
/// This class represents a user who is currently viewing a Mixer interactive stream. This
/// user (also known as an interactive_participant) has both a Mixer account and a Microsoft Security
/// Account (MSA).
/// </summary>
class interactive_participant
{
public:

    /// <summary>
    /// The Mixer ID of the user.
    /// </summary>
    _MIXERIMP uint32_t mixer_id() const;

    /// <summary>
    /// The username of the user.
    /// </summary>
    _MIXERIMP const string_t& username() const;

    /// <summary>
    /// The level of the user.
    /// </summary>
    _MIXERIMP uint32_t level() const;

    /// <summary>
    /// The current state of the participant.
    /// </summary>
    _MIXERIMP const interactive_participant_state state() const;

    /// <summary>
    /// Assigns the user to a specified group. This method 
    /// also updates the list of participants that are in this group.
    /// </summary>
    _MIXERIMP void set_group(std::shared_ptr<interactive_group> group);

    /// <summary>
    /// Returns a pointer to the group that the user is assigned to.
    /// By default, participants are placed in a group named "default".
    /// </summary>
    _MIXERIMP const std::shared_ptr<interactive_group> group();

    /// <summary>
    /// The time (in UTC) at which the user last used the interactive control input.
    /// </summary>
    _MIXERIMP const std::chrono::milliseconds& last_input_at() const;

    /// <summary>
    /// The time (in UTC) at which the user connected to the Interactive stream.
    /// </summary>
    _MIXERIMP const std::chrono::milliseconds& connected_at() const;

    /// <summary>
    /// A Boolean value that indicates whether or not the user input is disabled.
    /// If TRUE, user input has been disabled.
    /// </summary>
    _MIXERIMP bool input_disabled() const;

#if 0
    /// <summary>
    /// Returns a particular button. If the button does not exist,
    /// NULL is returned.
    /// </summary>
    _MIXERIMP const std::shared_ptr<interactive_button_control> button(_In_ const string_t& controlId);

    /// <summary>
    /// Returns buttons that the participant has interacted with.
    /// </summary>
    _MIXERIMP const std::vector<std::shared_ptr<interactive_button_control>>& buttons();

    /// <summary>
    /// Returns a particular joystick. If the joystick does not exist, 
    /// NULL is returned.
    /// </summary>
    _MIXERIMP const std::shared_ptr<interactive_joystick_control> joystick(_In_ const string_t& controlId);

    /// <summary>
    /// Returns joysticks that the participant has interacted with.
    /// </summary>
    _MIXERIMP const std::vector<std::shared_ptr<interactive_joystick_control>>& joysticks();
#endif

private:

    /// <summary>
    /// Internal function to construct an intreactive_participant.
    /// </summary>
    interactive_participant(
        _In_ uint32_t mixerId,
        _In_ string_t username,
        _In_ uint32_t level,
        _In_ string_t groupId,
        _In_ std::chrono::milliseconds lastInputAt,
        _In_ std::chrono::milliseconds connectedAt,
        _In_ bool disabled
    );


    /// <summary>
    /// Internal function to construct a interactive_participant.
    /// </summary>
    interactive_participant();

    std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactivity_manager> m_interactivityManager;
    std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactive_participant_impl> m_impl;

    friend interactive_participant_impl;
    friend interactivity_manager_impl;
};


/// <summary>
/// Describes the types of interactive message objects.
/// </summary>
enum class interactive_event_type
{
    /// <summary>
    /// An error message object. This object type is returned when the service
	/// or manager encounters an error. The err and err_message members will
	/// contain pertinent info.
    /// </summary>
    error,

    /// <summary>
    /// An interactivity state changed message object.
    /// </summary>
    interactivity_state_changed,

    /// <summary>
    /// A participant state changed message object.
    /// </summary>
    participant_state_changed,

    /// <summary>
    /// A button message object.
    /// </summary>
    button,

    /// <summary>
    /// A joystick message object.
    /// </summary>
    joystick
};


/// <summary>
/// Base class for all interactive event args. Contains information for Interactive events.
/// </summary>
class interactive_event_args
{
public:

    /// <summary>
    /// Constructor for the interactive event args object.
    /// </summary>
    interactive_event_args(){}

    /// <summary>
    /// Virtual destructor for the interactive event args object.
    /// </summary>
    virtual ~interactive_event_args(){}
};

/// <summary>
/// Base class for all Interactive events. Mixer Interactivity
/// is an event-driven service.
/// </summary>
class interactive_event
{
public:

    /// <summary>
    /// Constructor for the interactive event object.
    /// </summary>
    interactive_event(
        _In_ std::chrono::milliseconds time,
        _In_ std::error_code errorCode,
        _In_ string_t errorMessage,
        _In_ interactive_event_type eventType,
        _In_ std::shared_ptr<interactive_event_args> eventArgs
        );

    /// <summary>
    /// The time (in UTC) when this event is raised.
    /// </summary>
    _MIXERIMP const std::chrono::milliseconds& time() const;

    /// <summary>
    /// The error code indicating the result of the operation.
    /// </summary>
    _MIXERIMP const std::error_code& err() const;

    /// <summary>
    /// Returns call specific error message with debug information.
    /// Message is not localized as it is meant to be used for debugging only.
    /// </summary>
    _MIXERIMP const string_t& err_message() const;

    /// <summary>
    /// Type of the event raised.
    /// </summary>
    _MIXERIMP interactive_event_type event_type() const;

    /// <summary>
    /// Returns a pointer to an event argument. Cast the event arg to a specific
    /// event arg class type before retrieving the data.
    /// </summary>
    _MIXERIMP const std::shared_ptr<interactive_event_args>& event_args() const;

private:

    std::chrono::milliseconds m_time;
    std::error_code m_errorCode;
    string_t m_errorMessage;
    interactive_event_type m_eventType;
    std::shared_ptr<interactive_event_args> m_eventArgs;
};


/// <summary>
/// Contains information when the state of interactivity changes.
/// </summary>
class interactivity_state_change_event_args : public interactive_event_args
{
public:

    /// <summary>
    /// The current interactivity state.
    /// </summary>
    _MIXERIMP const interactivity_state new_state() const;

    /// <summary>
    /// Constructor for the interactivity state change event args object.
    /// </summary>
    interactivity_state_change_event_args(
        _In_ interactivity_state newState
    );

private:

    interactivity_state    m_newState;
};


/// <summary>
/// Contains information for a participant state change event. 
/// The state changes when a participant joins or leaves the channel.
/// </summary>
class interactive_participant_state_change_event_args : public interactive_event_args
{
public:

    /// <summary>
    /// The participant whose state has changed. For example, a 
    /// participant who has just joined the Mixer channel.
    /// </summary>
    _MIXERIMP const std::shared_ptr<interactive_participant>& participant() const;

    /// <summary>
    /// The current state of the participant.
    /// </summary>
    _MIXERIMP const interactive_participant_state& state() const;

    /// <summary>
    /// Constructor for the interactive_participant_state_change_event_args object.
    /// </summary>
    interactive_participant_state_change_event_args(
        _In_ std::shared_ptr<interactive_participant> participant,
        _In_ interactive_participant_state state
    );

private:

    std::shared_ptr<interactive_participant> m_participant;
    interactive_participant_state m_state;
};


/// <summary>
/// Contains information for a button event.
/// </summary>
class interactive_button_event_args : public interactive_event_args
{
public:

    /// <summary>
    /// Unique string identifier for this control
    /// </summary>
    _MIXERIMP const string_t& control_id() const;

    /// <summary>
    /// Unique string identifier for the spark transaction associated with this control event.
    /// </summary>
    _MIXERIMP const string_t& transaction_id() const;

    /// <summary>
    /// Spark cost assigned to the button control.
    /// </summary>
    _MIXERIMP uint32_t cost() const;

    /// <summary>
    /// The user who raised this event.
    /// </summary>
    _MIXERIMP const std::shared_ptr<interactive_participant>& participant() const;

    /// <summary>
    /// Boolean to indicate if the button is up or down.
    /// Returns TRUE if button is down.
    /// </summary>
    _MIXERIMP bool is_pressed() const;

    /// <summary>
    /// Constructor for the interactive button event args object.
    /// </summary>
    interactive_button_event_args(
        _In_ string_t controlId,
        _In_ string_t transaction_id,
        _In_ uint32_t cost,
        _In_ std::shared_ptr<interactive_participant> participant,
        _In_ bool isPressed
    );

private:

    string_t m_controlId;
    string_t m_transactionId;
    uint32_t m_cost;
    std::shared_ptr<interactive_participant> m_participant;
    bool m_isPressed;
};

/// <summary>
/// Contains information for a joystick event. These arguments are sent 
/// at an interval frequency configured via the Mixer Interactive Studio.
/// </summary>
class interactive_joystick_event_args : public interactive_event_args
{
public:

    /// <summary>
    /// Unique string identifier for this control.
    /// </summary>
    _MIXERIMP const string_t& control_id() const;

    /// <summary>
    /// The X coordinate of the joystick, in the range of [-1, 1].
    /// </summary>
    _MIXERIMP double x() const;
    /// <summary>
    /// The Y coordinate of the joystick, in the range of [-1, 1].
    /// </summary>
    _MIXERIMP double y() const;

    /// <summary>
    /// Participant whose action this event represents.
    /// </summary>
    _MIXERIMP const std::shared_ptr<interactive_participant>& participant() const;

    /// <summary>
    /// Constructor for the interactive_joystick_event_args object.
    /// </summary>
    interactive_joystick_event_args(
        _In_ std::shared_ptr<interactive_participant> participant,
        _In_ double x,
        _In_ double y,
        _In_ string_t control_id
    );

private:

    string_t m_controlId;
    double m_x;
    double m_y;
    std::shared_ptr<interactive_participant> m_participant;
};

/// <summary>
/// Base class for all interactive controls.
/// All controls are created and configured using the Mixer Interactive Lab.
/// </summary>
class interactive_control
{
public:

    /// <summary>
    /// The type of control.
    /// </summary>
    _MIXERIMP const interactive_control_type& control_type() const;

    /// <summary>
    /// Unique string identifier for the control.
    /// </summary>
    _MIXERIMP const string_t& control_id() const;

    /// <summary>
    /// Returns the list of meta properties for the control
    ///</summary>
    _MIXERIMP const std::map<string_t, string_t>& meta_properties() const;

protected:

    /// <summary>
    /// Internal constructor for the interactive_control object.
    /// </summary>
    interactive_control();

    /// <summary>
    /// Internal virtual destructor for the interactive_control object.
    /// </summary>
    virtual ~interactive_control()
    {
    }

    /// <summary>
    /// Internal constructor for the interactive_control object.
    /// </summary>
    interactive_control(
        _In_ string_t parentScene,
        _In_ string_t controlId,
        _In_ bool disabled
        );

    /// <summary>
    /// Internal function to clear the state of the interactive_control object.
    /// </summary>
    virtual void clear_state() = 0;

    /// <summary>
    /// Internal function to update the state of the interactive_control object.
    /// </summary>
    virtual bool update(web::json::value json, bool overwrite) = 0;

    /// <summary>
    /// Internal function to initialize interactive_control object.
    /// </summary>
    virtual bool init_from_json(_In_ web::json::value json) = 0;

    std::shared_ptr<interactivity_manager> m_interactivityManager;
    string_t m_parentScene;
    interactive_control_type m_type;
    string_t m_controlId;
    bool m_disabled;
    string_t m_etag;
    std::map<string_t, string_t> m_metaProperties;

    friend interactive_control_builder;
    friend interactivity_manager_impl;
};


/// <summary>
/// Represents an interactivity button control. This class is 
/// derived from interactive_control. All controls are created and 
/// configured using the Mixer Interactive Lab.
/// </summary>
class interactive_button_control : public interactive_control
{
public:

    /// <summary>
    /// Text displayed on the button control.
    /// </summary>
    _MIXERIMP const string_t& button_text() const;

    /// <summary>
    /// Spark cost assigned to the button control.
    /// </summary>
    _MIXERIMP uint32_t cost() const;

    /// <summary>
    /// Indicates whether the button is enabled or disabled. If TRUE, 
    /// button is disabled.
    /// </summary>
    _MIXERIMP bool disabled() const;

    /// <summary>
    /// Function to enable or disable the button.
    /// </summary>
    /// <param name="disabled">Value to enable or disable the button. 
    /// Set this value to TRUE to disable the button.</param>
    _MIXERIMP void set_disabled(_In_ bool disabled);

    /// <summary>
    /// Sets the cooldown duration (in milliseconds) required between triggers. 
    /// Disables the button for a period of time.
    /// </summary>
    /// <param name="cooldown">Duration (in milliseconds) required between triggers.</param>
    _MIXERIMP void trigger_cooldown(std::chrono::milliseconds cooldown) const;

    /// <summary>
    /// Time remaining (in milliseconds) before the button can be triggered again.
    /// </summary>
    _MIXERIMP std::chrono::milliseconds remaining_cooldown() const;

    /// <summary>
    /// Current progress of the button control.
    /// </summary>
    _MIXERIMP float progress() const;

    /// <summary>
    /// Sets the progress value for the button control.
    /// </summary>
    /// <param name="progress">The progress value, in the range of 0.0 to 1.0.</param>
    _MIXERIMP void set_progress(_In_ float progress);

    /// <summary>
    /// Returns the total count of button downs since the last call to do_work().
    /// </summary>
    _MIXERIMP uint32_t count_of_button_downs();

    /// <summary>
    /// Returns the total count of button downs by the specified participant
    /// since the last call to do_work().
    /// </summary>
    _MIXERIMP uint32_t count_of_button_downs(_In_ uint32_t mixerId);

    /// <summary>
    /// Returns the total count of button presses since the last call to do_work().
    /// </summary>
    _MIXERIMP uint32_t count_of_button_presses();

    /// <summary>
    /// Returns the total count of button presses by the specified participant
    /// since the last call to do_work().
    /// </summary>
    _MIXERIMP uint32_t count_of_button_presses(_In_ uint32_t mixerId);

    /// <summary>
    /// Returns the total count of button ups since the last call to do_work().
    /// </summary>
    _MIXERIMP uint32_t count_of_button_ups();

    /// <summary>
    /// Returns the total count of button ups by the specified participant
    /// since the last call to do_work().
    /// </summary>
    _MIXERIMP uint32_t count_of_button_ups(_In_ uint32_t mixerId);

    /// <summary>
    /// Returns TRUE if button is currently pressed.
    /// </summary>
    _MIXERIMP bool is_pressed();

    /// <summary>
    /// Returns TRUE if the button is currently pressed by the specified participant.
    /// </summary>
    _MIXERIMP bool is_pressed(_In_ uint32_t mixerId);

    /// <summary>
    /// Returns TRUE if button is currently down.
    /// </summary>
    _MIXERIMP bool is_down();

    /// <summary>
    /// Returns TRUE if the button is clicked down by the specified participant.
    /// </summary>
    _MIXERIMP bool is_down(_In_ uint32_t mixerId);

    /// <summary>
    /// Returns TRUE if button is currently up.
    /// </summary>
    _MIXERIMP bool is_up();

    /// <summary>
    /// Returns TRUE if the button is currently up for the specified participant.
    /// </summary>
    _MIXERIMP bool is_up(_In_ uint32_t mixerId);

private:

    /// <summary>
    /// Constructor for interactive_button_control object.
    /// </summary>
    interactive_button_control();

    /// <summary>
    /// Constructor for interactive_button_control object.
    /// </summary>
    interactive_button_control(
        _In_ string_t parentSceneId,
        _In_ string_t controlId,
        _In_ bool enabled,
        _In_ float progress,
        _In_ std::chrono::milliseconds m_cooldownDeadline,
        _In_ string_t buttonText,
        _In_ uint32_t sparkCost
    );

    /// <summary>
    /// Internal function to initialize a interactive_button_control object.
    /// </summary>
    bool init_from_json(web::json::value json);

    /// <summary>
    /// Internal function to clear the state of the interactive_button_control object.
    /// </summary>
    void clear_state();

    /// <summary>
    /// Internal function to update the state of the interactive_control object.
    /// </summary>
    bool update(web::json::value json, bool overwrite);

    float                     m_progress;
    std::chrono::milliseconds m_cooldownDeadline;
    string_t                  m_buttonText;
    uint32_t                  m_sparkCost;
    std::map<uint32_t, std::shared_ptr<interactive_button_state>> m_buttonStateByMixerId;
    std::shared_ptr<interactive_button_count> m_buttonCount;

    friend interactive_control_builder;
    friend interactivity_manager_impl;
};


/// <summary>
/// Represents an interactivity joystick control. This class is derived from interactive_control. 
/// All controls are created and configured using the Mixer Interactive Lab.
/// </summary>
class interactive_joystick_control : public interactive_control
{
public:

    /// <summary>
    /// The current X coordinate of the joystick, in the range of [-1, 1].
    /// </summary>
    _MIXERIMP double x() const;

    /// <summary>
    /// The current X coordinate of the joystick, in the range of [-1, 1] for the specified participant.
    /// </summary>
    _MIXERIMP double x(_In_ uint32_t mixerId);

    /// <summary>
    /// The current Y coordinate of the joystick, in the range of [-1, 1].
    /// </summary>
    _MIXERIMP double y() const;

    /// <summary>
    /// The current Y coordinate of the joystick, in the range of [-1, 1] for the specified participant.
    /// </summary>
    _MIXERIMP double y(_In_ uint32_t mixerId);

    /// <summary>
    /// Internal function to clear the state of the interactive_button_control object.
    /// </summary>
    void clear_state();

private:

    /// <summary>
    /// Constructor for the interactive_joystick_control object.
    /// </summary>
    interactive_joystick_control();

    /// <summary>
    /// Constructor for the interactive_joystick_control object.
    /// </summary>
    interactive_joystick_control(
        _In_ string_t parentSceneId,
        _In_ string_t controlId,
        _In_ bool enabled,
        _In_ double x,
        _In_ double y
        );

    /// <summary>
    /// Internal function to initialize a interactive_joystick_control object.
    /// </summary>
    bool init_from_json(web::json::value json);

    /// <summary>
    /// Internal function to update the state of the interactive_joystick_control object.
    /// </summary>
    bool update(web::json::value json, bool overwrite);

    double m_x;
    double m_y;

    std::map<uint32_t, std::shared_ptr<interactive_joystick_state>> m_joystickStateByMixerId;

    friend interactive_control_builder;
    friend interactivity_manager_impl;
};


/// <summary>
/// Represents an interactive group. This group functionality is used to 
/// segment your audience watching a stream. interactive_group is useful when you want 
/// portions of your audience to be shown a different scene while watching a stream. 
/// Participants can only be assigned to a single group.
/// </summary>
class interactive_group
{
public:

    /// <summary>
    /// Constructor for the interactive_group object. If no scene is specified, 
    /// the group is shown the "default" scene.
    /// </summary>
    /// <param name="groupId">The unique string identifier for the group.</param>
    interactive_group(
        _In_ string_t groupId
        );

    /// <summary>
    /// Constructor for the interactive_group object.
    /// </summary>
    /// <param name="groupId">The unique string identifier for the group.</param>
    /// <param name="scene">The scene shown to the group.</param>
    interactive_group(
        _In_ string_t groupId,
        _In_ std::shared_ptr<interactive_scene> scene
        );

    /// <summary>
    /// Unique string identifier for the group.
    /// </summary>
    _MIXERIMP const string_t& group_id() const;

    /// <summary>
    /// Returns a pointer to the scene assigned to the group.
    /// </summary>
    _MIXERIMP std::shared_ptr<interactive_scene> scene();

    /// <summary>
    /// Assigns a scene to the group.
    /// </summary>
    _MIXERIMP void set_scene(std::shared_ptr<interactive_scene> currentScene);

    /// <summary>
    /// Gets all the participants assigned to the group. The list may be empty.
    /// </summary>
    _MIXERIMP const std::vector<std::shared_ptr<interactive_participant>> participants();

private:

    /// <summary>
    /// Internal function to construct a interactive_group object.
    /// </summary>
    interactive_group();

    std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactivity_manager> m_interactivityManager;
    std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactive_group_impl> m_impl;

    friend interactive_group_impl;
    friend interactivity_manager_impl;
    friend interactivity_manager;
};


/// <summary>
/// Represents an interactive scene. These scenes are configured 
/// using the Mixer Interactive Lab.
/// </summary>
class interactive_scene
{
public:

    /// <summary>
    /// Unique string identifier for the scene.
    /// </summary>
    _MIXERIMP const string_t& scene_id() const;

    /// <summary>
    /// Returns all the groups that the scene is assigned to. This list may be empty.
    /// </summary>
    _MIXERIMP const std::vector<string_t> groups();

    /// <summary>
    /// Returns a list of all the buttons in the scene. This list may be empty.
    /// </summary>
    _MIXERIMP const std::vector<std::shared_ptr<interactive_button_control>> buttons();

    /// <summary>
    /// Returns the pointer to the specified button, if it exist.
    /// </summary>
    /// <param name="controlId">The unique string identifier of the button.</param>
    _MIXERIMP const std::shared_ptr<interactive_button_control> button(_In_ const string_t& controlId);

    /// <summary>
    /// Returns a list of all the joysticks in the scene. This list may be empty.
    /// </summary>
    _MIXERIMP const std::vector<std::shared_ptr<interactive_joystick_control>> joysticks();

    /// <summary>
    /// Returns the pointer to the specified joystick, if it exist.
    /// </summary>
    /// <param name="controlId">The unique string identifier of the joystick.</param>
    _MIXERIMP const std::shared_ptr<interactive_joystick_control> joystick(_In_ const string_t& controlId);

private:

    /// <summary>
    /// Constructor for the interactive_scene object.
    /// </summary>
    interactive_scene();

    /// <summary>
    /// Constructor for the interactive_scene object.
    /// </summary>
    interactive_scene(
        _In_ string_t sceneId,
        _In_ bool enabled
        );

    std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactivity_manager> m_interactivityManager;
    std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactive_scene_impl> m_impl;

    friend interactive_scene_impl;
    friend interactivity_manager_impl;
    friend interactivity_manager;
};


/// <summary>
/// Manager service class that handles the interactivity event 
/// experience between the Mixer service and the title.
/// </summary>
class interactivity_manager : public std::enable_shared_from_this<interactivity_manager>
{
public:

    /// <summary>
    /// Gets the singleton instance of interactivity_manager.
    /// </summary>
    _MIXERIMP static std::shared_ptr<interactivity_manager> get_singleton_instance();

    /// <summary>
    /// Sets up the connection for the Mixer interactivity event experience by
    /// initializing a background task.
    /// </summary>
    /// <returns>Value that indicates whether the initialization request is accepted or not. 
    /// If TRUE, the initialization request is accepted.</returns>
    /// <param name="interactiveVersion"> The version of the Mixer interactivity experience created for the title.</param>
    /// <param name="goInteractive">Value that indicates whether or not to start interactivity immediately. 
    /// If FALSE, you need to actively start_interactive() to intiate interactivity after initialization.</param>
    /// <remarks></remarks>
    _MIXERIMP bool initialize(
        _In_ string_t interactiveVersion,
        _In_ bool goInteractive = true,
        _In_ string_t sharecode = L""
    );

#if TV_API | XBOX_UWP
    /// <summary>
    /// Sets the local user to be used for authentication for the Mixer interactivity experience.
    /// </summary>
    /// <param name="user">The user's Xbox Live identifier.</param>
    /// <returns>Returns an interactive event to report any potential error. A nullptr is returned if there's no error.</returns>
    _MIXERIMP std::shared_ptr<interactive_event> set_local_user(_In_ xbox_live_user_t user);
#else
    /// <summary>
    /// Set an xtoken retrieved from a signed in user. This is used to authenticate into the Mixer interactivity experience.
    /// </summary>
    /// <param name="token">The user's xtoken.</param>
    /// <returns>Returns an interactive event to report any potential error. A nullptr is returned if there's no error.</returns>
    _MIXERIMP std::shared_ptr<interactive_event> set_xtoken(_In_ string_t token);

	/// <summary>
	/// Set an OAuth token for the Mixer user obtained via some flow external to the C++ SDK.
	/// </summary>
	/// <param name="token">The user's OAuth token.</param>
	/// <returns>Returns an interactive event to report any potential error. A nullptr is returned if there's no error.</returns>
	_MIXERIMP std::shared_ptr<interactive_event> set_oauth_token(_In_ string_t token);
#endif

#if 0
    /// <summary>
    /// Requests an OAuth account authorization code from the Mixer services. The title needs to display this 
    /// code and prompt the user to enter it at mixer.com/go. This process allows the user's Mixer account to 
    /// be linked to an interactivity stream.
    /// </summary>
    /// <param name="mixer_id">The Mixer ID of the user.</param>
    _MIXERIMP void request_linking_code(_In_ uint32_t mixer_id) const;
#endif

    /// <summary>
    /// The time of the Mixer interactivity service, in UTC. Used to maintain the 
    /// title's synchronization with the Mixer interactivity experience.
    /// </summary>
    _MIXERIMP const std::chrono::milliseconds get_server_time();

    /// <summary>
    /// Returns the version of the Mixer interactivity experience created.
    /// </summary>
    _MIXERIMP const string_t& interactive_version() const;

    /// <summary>
    /// The enum value that indicates the interactivity state of the Interactivity manager.
    /// </summary>
    _MIXERIMP const interactivity_state interactivity_state();

    /// <summary>
    /// Used by the title to inform the Mixer service that it is ready to receive interactivity input.
    /// </summary>
    /// <remarks></remarks>
    _MIXERIMP bool start_interactive();

    /// <summary>
    /// Used by the title to inform the Mixer service that it is no longer receiving interactivity input.
    /// </summary>
    /// <returns></returns>
    /// <remarks></remarks>
    _MIXERIMP bool stop_interactive();

    /// <summary>
    /// Returns currently active participants of this interactivity experience.
    /// </summary>
    _MIXERIMP std::vector<std::shared_ptr<interactive_participant>> participants();

    /// <summary>
    /// Gets all the groups associated with the current interactivity instance.
    /// Empty list is returned if initialization is not yet completed.
    /// </summary>
    _MIXERIMP std::vector<std::shared_ptr<interactive_group>> groups();

    /// <summary>
    /// Gets the pointer to a specific group. Returns a NULL pointer if initialization
    /// is not yet completed or if the group does not exist.
    /// </summary>
    _MIXERIMP std::shared_ptr<interactive_group> group(_In_ const string_t& group_id = L"default");

    /// <summary>
    /// Gets all the scenes associated with the current interactivity instance.
    /// Returns a NULL pointer if initialization is not yet completed.
    /// </summary>
    _MIXERIMP std::vector<std::shared_ptr<interactive_scene>> scenes();

    /// <summary>
    /// Gets the pointer to a specific scene. Returns a NULL pointer if initialization 
    /// is not yet completed or if the scene does not exist.
    /// </summary>
    _MIXERIMP std::shared_ptr<interactive_scene> scene(_In_ const string_t&  scene_id);

    /// <summary>
    /// Function to enable or disable the button.
    /// </summary>
    /// <param name="control_id">The unique string identifier of the control.</param>
    /// <param name="disabled">Value to enable or disable the button. 
    /// Set this value to TRUE to disable the button.</param>
    _MIXERIMP void set_disabled(_In_ const string_t& control_id, _In_ bool disabled) const;

    /// <summary>
    /// Current progress of the button control.
    /// </summary>
    _MIXERIMP float progress() const;

    /// <summary>
    /// Sets the progress value for the button control.
    /// </summary>
    /// <param name="progress">The progress value, in the range of 0.0 to 1.0.</param>
    _MIXERIMP void set_progress(_In_ const string_t& control_id, _In_ float progress);

    /// <summary>
    /// Disables a specific control for a period of time, specified in milliseconds.
    /// </summary>
    /// <param name="control_id">The unique string identifier of the control.</param>
    /// <param name="cooldown">Cooldown duration (in milliseconds).</param>
    _MIXERIMP void trigger_cooldown(_In_ const string_t& control_id, _In_ const std::chrono::milliseconds& cooldown) const;

    /// <summary>
    /// Captures a given interactive event transaction, charging the sparks to the appropriate Participant.
    /// </summary>
    /// <param name="transaction_id">The unique string identifier of the transaction to be captured.</param>
    _MIXERIMP void capture_transaction(_In_ const string_t& transaction_id) const;

    /// <summary>
    /// Manages and maintains proper state updates between the title and the Interactivity Service.
    /// To ensure best performance, do_work() must be called frequently, such as once per frame.
    /// Title needs to be thread safe when calling do_work() since this is when states are changed.
    /// This also clears the state of the input controls.
    /// </summary>
    /// <returns>A list of all the events the title has to handle. Empty if no events have been triggered
    /// during this update.</returns>
    _MIXERIMP std::vector<MICROSOFT_MIXER_NAMESPACE::interactive_event> do_work();

private:

    /// <summary>
    /// Internal function
    /// </summary>
    interactivity_manager();

    std::shared_ptr<interactivity_manager_impl> m_impl;

    friend interactive_control_builder;
    friend interactivity_mock_util;
    friend interactive_scene_impl;
    friend interactive_group;
    friend interactive_group_impl;
    friend interactive_participant_impl;
};

#if 0
/// <summary>
/// Represents mock Interactive events. This class mocks events between the Interactivity
/// service and participants.
/// </summary>
class interactivity_mock_util : public std::enable_shared_from_this<interactivity_mock_util>
{
public:

    /// <summary>
    /// Returns the singleton instance of the mock class.
    /// </summary>
    _MIXERIMP static std::shared_ptr<interactivity_mock_util> get_singleton_instance();

    /// <summary>
    /// Sets the OAuth token; skips the linking code API.
    /// </summary>
    /// <param name="token">The token.</param>
    _MIXERIMP void set_oauth_token(_In_ string_t token);

    /// <summary>
    /// Creates a mock button event. Note: A mocked participant must first join before a mock input 
    /// such as a mock button event can be sent. Else, it will be ignored by the Interactivity manager.
    /// </summary>
    /// <param name="mixerId">The Mixer ID of the user.</param>
    /// <param name="buttonId">The unique string identifier of the button control.</param>
    /// <param name="is_down">TValue that indicates the button is down or not. If TRUE, the button is down.</param>
    _MIXERIMP void mock_button_event(_In_ uint32_t mixerId, _In_ string_t buttonId, _In_ bool is_down);

    /// <summary>
    /// Simulates a specific mock participant joining the interactivity.
    /// </summary>
    /// <param name="mixerId">The Mixer ID of the user.</param>
    /// <param name="username">The Mixer username of the user.</param>
    _MIXERIMP void mock_participant_join(_In_ uint32_t mixerId, _In_ string_t username);

    /// <summary>
    /// Simulates a specific mock participant leaving interactivity.
    /// </summary>
    /// <param name="mixerId">The Mixer ID of the user.</param>
    /// <param name="username">The Mixer username of the user.</param>
    _MIXERIMP void mock_participant_leave(_In_ uint32_t mixerId, _In_ string_t username);

private:

    /// <summary>
    /// Internal function
    /// </summary>
    interactivity_mock_util();

    std::shared_ptr<MICROSOFT_MIXER_NAMESPACE::interactivity_manager_impl> m_interactiveManagerImpl;

    friend interactive_control_builder;
    friend interactivity_manager_impl;
};
#endif
}}
