//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MixerInteractivityTypes.h"

FMixerUser::FMixerUser()
	: Id(0)
	, Level(0)
{

}

FMixerChannel::FMixerChannel()
	: CurrentViewers(0)
	, LifetimeUniqueViewers(0)
	, Followers(0)
	, IsBroadcasting(false)
{

}

FMixerLocalUser::FMixerLocalUser()
	: Experience(0)
	, Sparks(0)
{

}

FMixerRemoteUser::FMixerRemoteUser()
	: InputEnabled(false)
	, ConnectedAt(FDateTime::MinValue())
	, InputAt(FDateTime::MinValue())
{

}