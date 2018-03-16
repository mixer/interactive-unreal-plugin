//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MixerInteractivitySettings.h"
#include "MixerInteractivityProjectAsset.h"
#include "MixerInteractivityJsonTypes.h"

UMixerInteractivitySettings::UMixerInteractivitySettings()
	: bPerParticipantStateCaching(true)
{

}

#if WITH_EDITORONLY_DATA

void UMixerInteractivitySettings::GetAllControls(const FString& Kind, TArray<FString>& OutControls)
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	UMixerProjectAsset* ProjectAsset = Cast<UMixerProjectAsset>(Settings->ProjectDefinition.TryLoad());
	if (ProjectAsset != nullptr)
	{
		ProjectAsset->GetAllControls([Kind](const FMixerInteractiveControl& C) { return C.Kind == Kind; }, OutControls);
	}
}

void UMixerInteractivitySettings::GetAllUnmappedCustomControls(TArray<FString>& OutCustomControls)
{
	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	UMixerProjectAsset* ProjectAsset = Cast<UMixerProjectAsset>(Settings->ProjectDefinition.TryLoad());
	if (ProjectAsset != nullptr)
	{
		auto IsSimpleCustomControl = [ProjectAsset](const FMixerInteractiveControl& C)
		{
			return C.IsCustom() && !ProjectAsset->CustomControlMappings.Contains(*C.Id);
		};
		ProjectAsset->GetAllControls(IsSimpleCustomControl, OutCustomControls);
	}
}

#endif