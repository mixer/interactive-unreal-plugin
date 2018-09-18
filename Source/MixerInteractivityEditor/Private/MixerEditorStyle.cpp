//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MixerEditorStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

FMixerEditorStyle::FMixerEditorStyle()
	: FSlateStyleSet("MixerEditorStyle")
{
	const FVector2D Icon16x16(16.f, 16.f);
	const FVector2D Icon64x64(64.f, 64.f);

	TSharedPtr<IPlugin> PluginObj = IPluginManager::Get().FindPlugin("Mixer");
	check(PluginObj.IsValid());
	SetContentRoot(PluginObj->GetBaseDir() / TEXT("Resources"));

	Set("ClassIcon.MixerProjectAsset", new FSlateImageBrush(RootToContentDir(TEXT("Mixer_16x.png")), Icon16x16));
	Set("ClassThumbnail.MixerProjectAsset", new FSlateImageBrush(RootToContentDir(TEXT("Mixer_64x.png")), Icon64x64));

	Set("ClassIcon.MixerCustomControl", new FSlateImageBrush(RootToContentDir(TEXT("Mixer_16x.png")), Icon16x16));
	Set("ClassThumbnail.MixerCustomControl", new FSlateImageBrush(RootToContentDir(TEXT("Mixer_64x.png")), Icon64x64));

	Set("ClassIcon.MixerCustomMethods", new FSlateImageBrush(RootToContentDir(TEXT("Mixer_16x.png")), Icon16x16));
	Set("ClassThumbnail.MixerCustomMethods", new FSlateImageBrush(RootToContentDir(TEXT("Mixer_64x.png")), Icon64x64));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FMixerEditorStyle::~FMixerEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
