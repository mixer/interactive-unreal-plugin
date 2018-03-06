#pragma once

#include "MixerInteractivityProjectAsset.h"

void UMixerProjectAsset::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	ParsedProjectDefinition.FromJson(ProjectDefinitionJson);
#endif
}

#if WITH_EDITORONLY_DATA

void UMixerProjectAsset::GetAllControls(TFunctionRef<bool(const FMixerInteractiveControl&)> Predicate, TArray<FString>& OutControls)
{
	for (const FMixerInteractiveScene& Scene : ParsedProjectDefinition.Controls.Scenes)
	{
		for (const FMixerInteractiveControl& Control : Scene.Controls)
		{
			if (Predicate(Control))
			{
				OutControls.Add(Control.Id);
			}
		}
	}
}


#endif