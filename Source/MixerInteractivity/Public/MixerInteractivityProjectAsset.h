#pragma once

#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPath.h"
#include "MixerInteractivityJsonTypes.h"
#include "MixerInteractivityProjectAsset.generated.h"

USTRUCT()
struct FMixerCustomControlMapping
{
	GENERATED_BODY()

	UPROPERTY()
	FName SceneName;

	UPROPERTY()
	FSoftClassPath Class;
};

UCLASS()
class MIXERINTERACTIVITY_API UMixerProjectAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

public:
#if WITH_EDITORONLY_DATA
	void GetAllControls(TFunctionRef<bool(const FMixerInteractiveControl&)> Predicate, TArray<FString>& OutControls);

public:
	UPROPERTY()
	FString ProjectDefinitionJson;

	FMixerInteractiveGameVersion ParsedProjectDefinition;

	UPROPERTY(EditAnywhere, Category="Scenes")
	TMap<FName, FMixerCustomControlMapping> CustomControlMappings;
#endif
};