#pragma once

#include "Engine/DataAsset.h"
#include "SoftObjectPath.h"
#include "MixerInteractivityJsonTypes.h"
#include "MixerInteractivityProjectAsset.generated.h"

UCLASS()
class MIXERINTERACTIVITY_API UMixerProjectAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

public:
#if WITH_EDITORONLY_DATA
	void GetAllControls(TFunctionRef<bool(const FMixerInteractiveControl&)> Predicate, TArray<FString>& OutControls);
#endif

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString ProjectDefinitionJson;

	FMixerInteractiveGameVersion ParsedProjectDefinition;
#endif

	UPROPERTY(EditAnywhere, Category="Scenes")
	TMap<FName, FSoftClassPath> CustomControlBindings;
};