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
				"SlateCore",
				"Slate",
				"UMG",
			});

		// Need Version.h
		PrivateIncludePathModuleNames.Add("Launch");

		string ThirdPartyFolder = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty");
		PrivateIncludePaths.Add(Path.Combine(ThirdPartyFolder, "Include"));
		PublicLibraryPaths.Add(Path.Combine(ThirdPartyFolder, "Lib", Target.Platform.ToString()));

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			Definitions.Add("PLATFORM_SUPPORTS_MIXER_OAUTH=1");
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

			PublicAdditionalLibraries.Add("Interactivity.Xbox.Cpp.lib");
			PublicAdditionalLibraries.Add("casablanca140.xbox.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
		{
			Definitions.Add("PLATFORM_SUPPORTS_MIXER_OAUTH=0");
			PublicAdditionalLibraries.Add("Interactivity.UWP.Cpp.lib");
			PublicAdditionalLibraries.Add("cpprest140_uwp_2_9.lib");

			PrivateDependencyModuleNames.Add("OnlineSubsystemUtils");
		}
		else
		{
			Definitions.Add("PLATFORM_SUPPORTS_MIXER_OAUTH=0");
		}

		bEnableExceptions = true;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	}
}