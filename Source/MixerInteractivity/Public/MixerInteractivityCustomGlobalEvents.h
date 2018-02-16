#pragma once

#include "MixerInteractivityCustomGlobalEvents.generated.h"

UCLASS(Blueprintable, NotBlueprintType, NotPlaceable, NotEditInlineNew, HideDropdown)
class MIXERINTERACTIVITY_API UMixerCustomGlobalEventCollection : public UObject
{
public:
	GENERATED_BODY()

	virtual bool IsEditorOnly() const { return true; }
};