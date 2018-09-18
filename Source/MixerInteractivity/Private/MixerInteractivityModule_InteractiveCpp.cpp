//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "MixerInteractivityModule_InteractiveCpp.h"

#if MIXER_BACKEND_INTERACTIVE_CPP

#include "MixerInteractivityLog.h"
#include "MixerInteractivitySettings.h"
#include "MixerInteractivityUserSettings.h"
#include "MixerInteractivityTypes.h"
#include "MixerInteractivityBlueprintLibrary.h"

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

IMPLEMENT_MODULE(FMixerInteractivityModule_InteractiveCpp, MixerInteractivity);

bool FMixerInteractivityModule_InteractiveCpp::StartInteractiveConnection()
{
	using namespace Microsoft::mixer;

	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("StartInteractiveConnection failed - plugin state %d; interactive-cpp state %d."),
			static_cast<int32>(GetInteractiveConnectionAuthState()), static_cast<int32>(interactivity_manager::get_singleton_instance()->interactivity_state()));
		return false;
	}

#if PLATFORM_XBOXONE
	Windows::Xbox::System::User^ ResolvedUser = GetXboxUser();
	// User should have been resolved by the time we get here.
	check(ResolvedUser != nullptr);
	interactivity_manager::get_singleton_instance()->set_local_user(ResolvedUser);

	// Note that calling set_local_user guarantees that we need to call initialize again.

#elif PLATFORM_SUPPORTS_MIXER_OAUTH
	const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
	interactivity_manager::get_singleton_instance()->set_oauth_token(*UserSettings->AccessToken);
#elif PLATFORM_NEEDS_OSS_LIVE
	const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
	interactivity_manager::get_singleton_instance()->set_xtoken(*UserSettings->AccessToken);
#endif

	// Library may already be initialized, particularly on non-Xbox platforms
	if (interactivity_manager::get_singleton_instance()->interactivity_state() == interactivity_state::not_initialized)
	{
		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		if (!interactivity_manager::get_singleton_instance()->initialize(*FString::FromInt(Settings->GameVersionId), false, *Settings->ShareCode))
		{
			UE_LOG(LogMixerInteractivity, Error, TEXT("Failed to initialize interactive-cpp"));
			return false;
		}
		SetInteractiveConnectionAuthState(EMixerLoginState::Logging_In);
	}
	else
	{
		SetInteractiveConnectionAuthState(EMixerLoginState::Logging_In);
		SetInteractiveConnectionAuthState(EMixerLoginState::Logged_In);
	}

	return true;
}

void FMixerInteractivityModule_InteractiveCpp::StopInteractiveConnection()
{
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
	{
		SetInteractiveConnectionAuthState(EMixerLoginState::Not_Logged_In);
		RemoteParticipantCache.Empty();
	}
}

bool FMixerInteractivityModule_InteractiveCpp::Tick(float DeltaTime)
{
	using namespace Microsoft::mixer;

	FMixerInteractivityModule::Tick(DeltaTime);

	std::vector<interactive_event> EventsThisFrame = interactivity_manager::get_singleton_instance()->do_work();

	// interactive-cpp doesn't support a true shutdown.  We'll approximate one to external code
	// by ignoring events when we're not supposed to have an interactive connection.
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
	{
		for (auto& MixerEvent : EventsThisFrame)
		{
			switch (MixerEvent.event_type())
			{
			case interactive_event_type::error:
				// If the error landed us in not_initialized then we respond by giving up
				// This is better than handling the state_changed event directly since
				// transitions to not_initialized can occur during normal operation (e.g.
				// Xbox user change) where we wouldn't want to abandon the regular state flow.
				UE_LOG(LogMixerInteractivity, Warning, TEXT("%s"), MixerEvent.err_message().c_str());
				if (interactivity_manager::get_singleton_instance()->interactivity_state() == interactivity_state::not_initialized)
				{
					SetInteractiveConnectionAuthState(EMixerLoginState::Not_Logged_In);
				}
				break;

			case interactive_event_type::interactivity_state_changed:
			{
				auto StateChangeArgs = std::static_pointer_cast<interactivity_state_change_event_args>(MixerEvent.event_args());
				switch (StateChangeArgs->new_state())
				{
				case interactivity_state::not_initialized:
					// Leave auth state alone - the auth path on Xbox can pop to this state.
					SetInteractivityState(EMixerInteractivityState::Not_Interactive);
					break;

				case interactivity_state::initializing:
					SetInteractiveConnectionAuthState(EMixerLoginState::Logging_In);
					SetInteractivityState(EMixerInteractivityState::Not_Interactive);
					// Ensure the default group has a non-null representation
					CreateGroup(NAME_DefaultMixerParticipantGroup);
					break;

				case interactivity_state::interactivity_pending:
					SetInteractiveConnectionAuthState(EMixerLoginState::Logged_In);
					break;

				case interactivity_state::interactivity_disabled:
					SetInteractiveConnectionAuthState(EMixerLoginState::Logged_In);
					SetInteractivityState(EMixerInteractivityState::Not_Interactive);
					break;

				case interactivity_state::interactivity_enabled:
					SetInteractiveConnectionAuthState(EMixerLoginState::Logged_In);
					SetInteractivityState(EMixerInteractivityState::Interactive);
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
					OnParticipantStateChanged().Broadcast(RemoteParticipant, EMixerInteractivityParticipantState::Joined);
					break;

				case interactive_participant_state::left:
					OnParticipantStateChanged().Broadcast(RemoteParticipant, EMixerInteractivityParticipantState::Left);
					break;

				case interactive_participant_state::input_disabled:
					OnParticipantStateChanged().Broadcast(RemoteParticipant, EMixerInteractivityParticipantState::Input_Disabled);
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
				OnButtonEvent().Broadcast(FName(OriginalButtonArgs->control_id().c_str()), RemoteParticipant, Details);
			}
			break;

			case interactive_event_type::joystick:
			{
				auto OriginalStickArgs = std::static_pointer_cast<interactive_joystick_event_args>(MixerEvent.event_args());
				TSharedPtr<const FMixerRemoteUser> RemoteParticipant = CreateOrUpdateCachedParticipant(OriginalStickArgs->participant());
				OnStickEvent().Broadcast(FName(OriginalStickArgs->control_id().c_str()), RemoteParticipant, FVector2D(OriginalStickArgs->x(), OriginalStickArgs->y()));
				break;
			}

			default:
				break;
			}
		}
	}

	TickParticipantCacheMaintenance();

	return true;
}


void FMixerInteractivityModule_InteractiveCpp::StartInteractivity()
{
	using namespace Microsoft::mixer;

	switch (interactivity_manager::get_singleton_instance()->interactivity_state())
	{
	case interactivity_disabled:
		check(GetInteractivityState() == EMixerInteractivityState::Not_Interactive || GetInteractivityState() == EMixerInteractivityState::Interactivity_Stopping);
		interactivity_manager::get_singleton_instance()->start_interactive();
		SetInteractivityState(EMixerInteractivityState::Interactivity_Starting);
		break;

	case interactivity_enabled:
	case interactivity_pending:
		check(GetInteractivityState() == EMixerInteractivityState::Interactivity_Starting || GetInteractivityState() == EMixerInteractivityState::Interactive);
		// No-op, but not a problem
		break;

	case not_initialized:
	case initializing:
		check(GetInteractivityState() == EMixerInteractivityState::Not_Interactive || GetInteractivityState() == EMixerInteractivityState::Interactivity_Stopping);
		// Caller should wait!
		// @TODO: tell them so.
		break;

	default:
		// Internal error in state management
		check(false);
		break;
	}
}

void FMixerInteractivityModule_InteractiveCpp::StopInteractivity()
{
	using namespace Microsoft::mixer;

	switch (interactivity_manager::get_singleton_instance()->interactivity_state())
	{
	case interactivity_enabled:
	case interactivity_pending:
		check(GetInteractivityState() == EMixerInteractivityState::Interactivity_Starting ||
			GetInteractivityState() == EMixerInteractivityState::Interactivity_Stopping ||
			GetInteractivityState() == EMixerInteractivityState::Interactive);
		interactivity_manager::get_singleton_instance()->stop_interactive();
		SetInteractivityState(EMixerInteractivityState::Interactivity_Stopping);
		break;

	case interactivity_disabled:
		check(GetInteractivityState() == EMixerInteractivityState::Not_Interactive || GetInteractivityState() == EMixerInteractivityState::Interactivity_Stopping);
		// No-op, but not a problem
		break;

	case not_initialized:
	case initializing:
		check(GetInteractivityState() == EMixerInteractivityState::Not_Interactive || GetInteractivityState() == EMixerInteractivityState::Interactivity_Stopping);
		// Caller should wait!
		// @TODO: tell them so.
		break;

	default:
		// Internal error in state management
		check(false);
		break;
	}
}

void FMixerInteractivityModule_InteractiveCpp::SetCurrentScene(FName Scene, FName GroupName)
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

FName FMixerInteractivityModule_InteractiveCpp::GetCurrentScene(FName GroupName)
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

void FMixerInteractivityModule_InteractiveCpp::TriggerButtonCooldown(FName Button, FTimespan CooldownTime)
{
	using namespace Microsoft::mixer;

	if (GetInteractivityState() == EMixerInteractivityState::Interactive)
	{
		std::chrono::milliseconds CooldownTimeInMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double, std::milli>(CooldownTime.GetTotalMilliseconds()));
		interactivity_manager::get_singleton_instance()->get_singleton_instance()->trigger_cooldown(*Button.ToString(), CooldownTimeInMs);
	}
}

bool FMixerInteractivityModule_InteractiveCpp::GetButtonDescription(FName Button, FMixerButtonDescription& OutDesc)
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

bool FMixerInteractivityModule_InteractiveCpp::GetButtonState(FName Button, FMixerButtonState& OutState)
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

bool FMixerInteractivityModule_InteractiveCpp::GetButtonState(FName Button, uint32 ParticipantId, FMixerButtonState& OutState)
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

bool FMixerInteractivityModule_InteractiveCpp::GetStickDescription(FName Stick, FMixerStickDescription& OutDesc)
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

bool FMixerInteractivityModule_InteractiveCpp::GetStickState(FName Stick, FMixerStickState& OutState)
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

bool FMixerInteractivityModule_InteractiveCpp::GetStickState(FName Stick, uint32 ParticipantId, FMixerStickState& OutState)
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

void FMixerInteractivityModule_InteractiveCpp::SetLabelText(FName Label, const FText& DisplayText)
{
	UE_LOG(LogMixerInteractivity, Error, TEXT("This implementation does not support setting label text."));
}

bool FMixerInteractivityModule_InteractiveCpp::GetLabelDescription(FName Label, FMixerLabelDescription& OutDesc)
{
	return false;
}

bool FMixerInteractivityModule_InteractiveCpp::GetTextboxDescription(FName Textbox, FMixerTextboxDescription& OutDesc)
{
	return false;
}

std::shared_ptr<Microsoft::mixer::interactive_button_control> FMixerInteractivityModule_InteractiveCpp::FindButton(FName Name)
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

std::shared_ptr<Microsoft::mixer::interactive_joystick_control> FMixerInteractivityModule_InteractiveCpp::FindStick(FName Name)
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

TSharedPtr<const FMixerRemoteUser> FMixerInteractivityModule_InteractiveCpp::GetParticipant(uint32 ParticipantId)
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

bool FMixerInteractivityModule_InteractiveCpp::CreateGroup(FName GroupName, FName InitialScene)
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

bool FMixerInteractivityModule_InteractiveCpp::GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants)
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

bool FMixerInteractivityModule_InteractiveCpp::MoveParticipantToGroup(FName GroupName, uint32 ParticipantId)
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

void FMixerInteractivityModule_InteractiveCpp::CaptureSparkTransaction(const FString& TransactionId)
{
	Microsoft::mixer::interactivity_manager::get_singleton_instance()->capture_transaction(*TransactionId);
}

void FMixerInteractivityModule_InteractiveCpp::TickParticipantCacheMaintenance()
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

TSharedPtr<FMixerRemoteUserCached> FMixerInteractivityModule_InteractiveCpp::CreateOrUpdateCachedParticipant(std::shared_ptr<Microsoft::mixer::interactive_participant> Participant)
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

void FMixerInteractivityModule_InteractiveCpp::CallRemoteMethod(const FString& MethodName, const TSharedRef<FJsonObject> MethodParams)
{
	FString SerializedParams;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&SerializedParams, 0);
	FJsonSerializer::Serialize(MethodParams, Writer);

	// Not available in the version of interactive-cpp currently in use
	//Microsoft::mixer::interactivity_manager::get_singleton_instance()->send_rpc_message(*MethodName, *SerializedParams);
}

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

#endif // MIXER_BACKEND_INTERACTIVE_CPP

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
int32 MixerInteractiveCppLinkerHelper;