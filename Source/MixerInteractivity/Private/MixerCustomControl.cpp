//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "MixerCustomControl.h"
#include "MixerDynamicDelegateBinding.h"

void UMixerCustomControl::PostInitProperties()
{
	Super::PostInitProperties();
}

bool UMixerCustomControl::Tick(float DeltaTime)
{
	return true;
}

void UMixerTestControl::RemoteEventHandler1(int32 Param1, FString Param2)
{

}
