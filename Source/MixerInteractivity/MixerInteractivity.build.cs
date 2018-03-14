using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class MixerInteractivity : ModuleRules
{
	public MixerInteractivity(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HTTP",
				"Json",
				"JsonUtilities",
				"SlateCore",
				"Slate",
				"UMG",
				"WebSockets",
			});

		// Need Version.h
		PrivateIncludePathModuleNames.Add("Launch");
		PrivateIncludePathModuleNames.Add("OnlineSubsystem");

		string ThirdPartyFolder = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty");
		PrivateIncludePaths.Add(ThirdPartyFolder);
		PrivateIncludePaths.Add(Path.Combine(ThirdPartyFolder, "Include"));
		PublicLibraryPaths.Add(Path.Combine(ThirdPartyFolder, "Lib", Target.Platform.ToString()));

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			Definitions.Add("PLATFORM_SUPPORTS_MIXER_OAUTH=1");
			Definitions.Add("MIXER_BACKEND_INTERACTIVE_CPP=0");
			Definitions.Add("MIXER_BACKEND_NULL=0");
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"WebBrowser",
				});

			PublicAdditionalLibraries.Add("Interactivity.Win32.Cpp.lib");
			PublicAdditionalLibraries.Add("cpprest140_2_9.lib");
			PublicAdditionalLibraries.Add("winhttp.lib");
			PublicAdditionalLibraries.Add("crypt32.lib");
			PublicAdditionalLibraries.Add("bcrypt.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			Definitions.Add("PLATFORM_SUPPORTS_MIXER_OAUTH=0");
			Definitions.Add("MIXER_BACKEND_INTERACTIVE_CPP=1");
			Definitions.Add("MIXER_BACKEND_NULL=0");

			PublicAdditionalLibraries.Add("Interactivity.Xbox.Cpp.lib");
			PublicAdditionalLibraries.Add("casablanca140.xbox.lib");
		}
		else
		{
			Definitions.Add("PLATFORM_SUPPORTS_MIXER_OAUTH=0");
			Definitions.Add("MIXER_BACKEND_NULL=1");
			Definitions.Add("MIXER_BACKEND_INTERACTIVE_CPP=0");
		}

		bEnableExceptions = true;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	}
}