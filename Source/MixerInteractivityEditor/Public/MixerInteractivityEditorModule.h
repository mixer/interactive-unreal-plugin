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

struct FMixerInteractiveGame;
struct FMixerInteractiveGameVersion;

DECLARE_DELEGATE_TwoParams(FOnMixerInteractiveGamesRequestFinished, bool, const TArray<FMixerInteractiveGame>&);
DECLARE_DELEGATE_TwoParams(FOnMixerInteractiveControlsRequestFinished, bool, const FMixerInteractiveGameVersion&);

class IMixerInteractivityEditorModule : public IModuleInterface
{
public:
	static inline IMixerInteractivityEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IMixerInteractivityEditorModule>("MixerInteractivityEditor");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("MixerInteractivityEditor");
	}

public:

	virtual bool RequestAvailableInteractiveGames(FOnMixerInteractiveGamesRequestFinished OnFinished) = 0;

	virtual bool RequestInteractiveControlsForGameVersion(const FMixerInteractiveGameVersion& Version, FOnMixerInteractiveControlsRequestFinished OnFinished) = 0;

	virtual const TArray<TSharedPtr<FName>>& GetDesignTimeObjects(FString ObjectKind) = 0;

	virtual void RefreshDesignTimeObjects() = 0;

	DECLARE_EVENT(IMixerInteractivityEditorModule, FOnDesignTimeObjectsChanged);
	virtual FOnDesignTimeObjectsChanged& OnDesignTimeObjectsChanged() = 0;
};