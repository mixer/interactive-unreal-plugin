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

#include "Templates/SharedPointer.h"
#include "MixerCustomControl.generated.h"

struct MIXERINTERACTIVITY_API FMixerSimpleCustomControl
{
	TMap<FString, TSharedPtr<class FJsonValue>> PropertyBag;
};

UCLASS(Blueprintable, BlueprintType)
class MIXERINTERACTIVITY_API UMixerCustomControl : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Category=Mixer))
	FString ControlId;


	virtual void PostInitProperties() override;

	bool Tick(float DeltaTime);

protected:

	FDelegateHandle TickerDelegate;

private:

};
