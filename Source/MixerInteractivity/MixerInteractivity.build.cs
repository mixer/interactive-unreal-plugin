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
			});

		string MixerSdkFolder = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "interactive-cpp");
		PrivateIncludePaths.Add(Path.Combine(MixerSdkFolder, "Include"));
		PrivateIncludePaths.Add(Path.Combine(MixerSdkFolder, "cpprestsdk", "Release", "include"));


		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			Definitions.Add("PLATFORM_SUPPORTS_MIXER_OAUTH=1");
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"WebBrowser",
					"SlateCore",
					"Slate",
					"zlib"
				});

			string MsArchFragment = Target.Platform == UnrealTargetPlatform.Win64 ? "x64" : "Win32";
			string BoostArchFragment = Target.Platform == UnrealTargetPlatform.Win64 ? "address-model-64" : "address-model-32";
			string OpenSslArchFragment = Target.Platform == UnrealTargetPlatform.Win64 ? "x64" : "x86";

			PublicLibraryPaths.Add(Path.Combine(MixerSdkFolder, "Binaries", "Release", MsArchFragment, "Interactivity.Win32.Cpp"));
			PublicLibraryPaths.Add(Path.Combine(MixerSdkFolder, "Binaries", "Release", MsArchFragment, "Casablanca"));
			PublicLibraryPaths.Add(Path.Combine(MixerSdkFolder, "External", "Packages", "boost_system-vc140.1.58.0-vs140rc", "lib", "native", BoostArchFragment, "lib"));
			PublicLibraryPaths.Add(Path.Combine(MixerSdkFolder, "External", "Packages", "boost_date_time-vc140.1.58.0-vs140rc", "lib", "native", BoostArchFragment, "lib"));
			PublicLibraryPaths.Add(Path.Combine(MixerSdkFolder, "External", "Packages", "boost_regex-vc140.1.58.0-vs140rc", "lib", "native", BoostArchFragment, "lib"));
			PublicLibraryPaths.Add(Path.Combine(MixerSdkFolder, "External", "Packages", string.Format("openssl.v140.windesktop.msvcstl.static.rt-dyn.{0}.1.0.2.0", OpenSslArchFragment), "lib", "native", "v140", "windesktop", "msvcstl", "static", "rt-dyn", OpenSslArchFragment, "release"));

			PublicAdditionalLibraries.Add("Interactivity.Win32.Cpp.lib");
			PublicAdditionalLibraries.Add("cpprest140_2_9.lib");
			PublicAdditionalLibraries.Add("winhttp.lib");
			PublicAdditionalLibraries.Add("crypt32.lib");
			PublicAdditionalLibraries.Add("bcrypt.lib");

			// Standard UE version of OpenSSL unfortunately won't do here.
			PublicAdditionalLibraries.Add("libeay32.lib");
			PublicAdditionalLibraries.Add("ssleay32.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			Definitions.Add("PLATFORM_SUPPORTS_MIXER_OAUTH=0");

			PublicLibraryPaths.Add(Path.Combine(MixerSdkFolder, "Binaries", "Release", "Durango", "casablanca140.Xbox"));
			PublicLibraryPaths.Add(Path.Combine(MixerSdkFolder, "Binaries", "Release", "Durango", "Interactivity.Xbox.Cpp"));


			PublicAdditionalLibraries.Add("Interactivity.Xbox.Cpp.lib");
			PublicAdditionalLibraries.Add("casablanca140.xbox.lib");
		}
		else
		{
			Definitions.Add("PLATFORM_SUPPORTS_MIXER_OAUTH=0");
		}

		bEnableExceptions = true;
	}
}