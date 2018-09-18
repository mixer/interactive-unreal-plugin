//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MixerInteractivityEditorModule.h"
#include "MixerInteractivityModule.h"
#include "MixerInteractivityTypes.h"
#include "MixerInteractivitySettings.h"
#include "PropertyEditorModule.h"
#include "ISettingsModule.h"
#include "EditorCategoryUtils.h"
#include "MixerInteractivitySettings.h"
#include "MixerInteractivityUserSettings.h"
#include "MixerInteractivitySettingsCustomization.h"
#include "MixerInteractivityPinFactory.h"
#include "MixerInteractivityJsonTypes.h"
#include "MixerProjectDefinitionCustomization.h"
#include "MixerInteractivityProjectAsset.h"
#include "MixerInteractivityBlueprintLibrary.h"
#include "MixerEditorStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

class FMixerInteractivityEditorModule : public IMixerInteractivityEditorModule
{
public:
	virtual void StartupModule() override;

public:
	virtual bool RequestAvailableInteractiveGames(FOnMixerInteractiveGamesRequestFinished OnFinished);

	virtual bool RequestInteractiveControlsForGameVersion(const FMixerInteractiveGameVersion& Version, FOnMixerInteractiveControlsRequestFinished OnFinished);
	
	virtual const TArray<TSharedPtr<FName>>& GetDesignTimeObjects(FString ObjectKind) { return DesignTimeObjects.FindChecked(ObjectKind); }

	virtual void RefreshDesignTimeObjects();

	virtual FOnDesignTimeObjectsChanged& OnDesignTimeObjectsChanged() { return DesignTimeObjectsChanged; }

private:

	TMap<FString, TArray<TSharedPtr<FName>>> DesignTimeObjects;

	FOnDesignTimeObjectsChanged DesignTimeObjectsChanged;

	TSharedPtr<FMixerEditorStyle> MixerStyle;
};

IMPLEMENT_MODULE(FMixerInteractivityEditorModule, MixerInteractivityEditor)

void FMixerInteractivityEditorModule::StartupModule()
{
	MixerStyle = MakeShared<FMixerEditorStyle>();

	FEditorCategoryUtils::RegisterCategoryKey("MixerInteractivity",
		LOCTEXT("MixerInteractivityCategory", "Mixer|Interactivity"),
		LOCTEXT("MixerInteractivityCategory_Tooltip", "Interactivity features provided by Mixer"));
	
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		"MixerInteractivitySettings",
		FOnGetDetailCustomizationInstance::CreateStatic(&FMixerInteractivitySettingsCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomClassLayout(
		"MixerProjectAsset",
		FOnGetDetailCustomizationInstance::CreateStatic(&FMixerProjectDefinitionCustomization::MakeInstance)
	);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "MixerInteractivity",
			LOCTEXT("MixerRuntimeSettingsName", "Mixer Interactivity"),
			LOCTEXT("MixerRuntimeSettingsDescription", "Configure the Mixer Interactivity plugin"),
			GetMutableDefault<UMixerInteractivitySettings>()
		);
	}

	TSharedPtr<FMixerInteractivityPinFactory> MixerInteractivityPinFactory = MakeShareable(new FMixerInteractivityPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(MixerInteractivityPinFactory);

	RefreshDesignTimeObjects();
}

bool FMixerInteractivityEditorModule::RequestAvailableInteractiveGames(FOnMixerInteractiveGamesRequestFinished OnFinished)
{
	IMixerInteractivityModule& MixerRuntimeModule = IMixerInteractivityModule::Get();
	if (MixerRuntimeModule.GetLoginState() != EMixerLoginState::Logged_In)
	{
		return false;
	}

	const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();
	TSharedPtr<const FMixerLocalUser> MixerUser = MixerRuntimeModule.GetCurrentUser();
	check(MixerUser.IsValid());
	FString GamesListUrl = FString::Printf(TEXT("https://mixer.com/api/v1/interactive/games/owned?user=%d"), MixerUser->Id);

	TSharedRef<IHttpRequest> GamesRequest = FHttpModule::Get().CreateRequest();
	GamesRequest->SetVerb(TEXT("GET"));
	GamesRequest->SetURL(GamesListUrl);
	GamesRequest->SetHeader(TEXT("Authorization"), UserSettings->GetAuthZHeaderValue());
	GamesRequest->OnProcessRequestComplete().BindLambda(
		[OnFinished](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		bool Success = false;
		TArray<FMixerInteractiveGame> GameCollection;
		if (bSucceeded && HttpResponse.IsValid())
		{
			if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
			{
				TArray<TSharedPtr<FJsonValue>> GameCollectionJson;
				TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());
				if (FJsonSerializer::Deserialize(JsonReader, GameCollectionJson))
				{
					Success = true;
					GameCollection.Reserve(GameCollectionJson.Num());
					for (TSharedPtr<FJsonValue>& GameJson : GameCollectionJson)
					{
						FMixerInteractiveGame Game;
						Game.FromJson(GameJson->AsObject());
						GameCollection.Add(Game);
					}
				}
			}
		}
		OnFinished.ExecuteIfBound(Success, GameCollection);
	});
	if (!GamesRequest->ProcessRequest())
	{
		return false;
	}

	return true;
}

bool FMixerInteractivityEditorModule::RequestInteractiveControlsForGameVersion(const FMixerInteractiveGameVersion& Version, FOnMixerInteractiveControlsRequestFinished OnFinished)
{
	FString ControlsForVersionUrl = FString::Printf(TEXT("https://mixer.com/api/v1/interactive/versions/%d"), Version.Id);

	TSharedRef<IHttpRequest> ControlsRequest = FHttpModule::Get().CreateRequest();
	ControlsRequest->SetVerb(TEXT("GET"));
	ControlsRequest->SetURL(ControlsForVersionUrl);
	ControlsRequest->OnProcessRequestComplete().BindLambda(
		[OnFinished](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		bool Success = false;
		FMixerInteractiveGameVersion VersionWithControls;
		if (bSucceeded && HttpResponse.IsValid())
		{
			if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
			{
				Success = VersionWithControls.FromJson(HttpResponse->GetContentAsString());
			}
		}

		OnFinished.ExecuteIfBound(Success, VersionWithControls);
	});

	if (!ControlsRequest->ProcessRequest())
	{
		return false;
	}

	return true;
}

template <class T>
const FString& GetMixerKind()
{
	return T::StaticStruct()->GetMetaData(MixerObjectKindMetadataTag);
}

void FMixerInteractivityEditorModule::RefreshDesignTimeObjects()
{
	DesignTimeObjects.FindOrAdd(GetMixerKind<FMixerSceneReference>()).Empty();

	// Known control types
	DesignTimeObjects.FindOrAdd(GetMixerKind<FMixerButtonReference>()).Empty();
	DesignTimeObjects.FindOrAdd(GetMixerKind<FMixerStickReference>()).Empty();
	DesignTimeObjects.FindOrAdd(GetMixerKind<FMixerLabelReference>()).Empty();

	DesignTimeObjects.FindOrAdd(GetMixerKind<FMixerCustomControlReference>()).Empty();

	// Custom controls
	TArray<TSharedPtr<FName>>& DesignTimeScenes = DesignTimeObjects.FindChecked(GetMixerKind<FMixerSceneReference>());
	TArray<TSharedPtr<FName>>& DesignTimeUnmappedCustomControls = DesignTimeObjects.FindChecked(GetMixerKind<FMixerCustomControlReference>());
	DesignTimeUnmappedCustomControls.Empty();

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	UMixerProjectAsset* ProjectAsset = Cast<UMixerProjectAsset>(Settings->ProjectDefinition.TryLoad());
	if (ProjectAsset != nullptr)
	{
		for (const FMixerInteractiveScene& Scene : ProjectAsset->ParsedProjectDefinition.Controls.Scenes)
		{
			DesignTimeScenes.Add(MakeShared<FName>(*Scene.Id));
			for (const FMixerInteractiveControl& Control : Scene.Controls)
			{
				FName ControlName = *Control.Id;
				TArray<TSharedPtr<FName>>* DesignTimeKnownControlType = DesignTimeObjects.Find(Control.Kind);
				if (DesignTimeKnownControlType != nullptr)
				{
					DesignTimeKnownControlType->Add(MakeShared<FName>(ControlName));
				}
				else if (!ProjectAsset->CustomControlMappings.Contains(ControlName))
				{
					DesignTimeUnmappedCustomControls.Add(MakeShared<FName>(ControlName));
				}
			}
		}
	}

	TArray<TSharedPtr<FName>>& DesignTimeGroups = DesignTimeObjects.FindOrAdd(GetMixerKind<FMixerGroupReference>());
	DesignTimeGroups.Empty(Settings->DesignTimeGroups.Num() + 1);
	DesignTimeGroups.Add(MakeShared<FName>("default"));
	for (const FMixerPredefinedGroup& Group : Settings->DesignTimeGroups)
	{
		DesignTimeGroups.Add(MakeShared<FName>(Group.Name));
	}

	DesignTimeObjectsChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE