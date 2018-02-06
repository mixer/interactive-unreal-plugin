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

#include "MixerInteractivityUserSettings.generated.h"

UCLASS(config=Game, globaluserconfig)
class MIXERINTERACTIVITY_API UMixerInteractivityUserSettings : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Config, Category = "Auth")
	FString RefreshToken;

	UPROPERTY(Transient)
	FString AccessToken;

public:

	FString GetAuthZHeaderValue() const
	{
		return (!AccessToken.IsEmpty() && PLATFORM_SUPPORTS_MIXER_OAUTH)
			? FString(TEXT("Bearer ")) + AccessToken
			: AccessToken;
	}
};