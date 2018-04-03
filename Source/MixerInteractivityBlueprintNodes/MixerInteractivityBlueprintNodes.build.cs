using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class MixerInteractivityBlueprintNodes : ModuleRules
{
	public MixerInteractivityBlueprintNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MixerInteractivity",
				"SlateCore",
				"Slate",
				"BlueprintGraph",
				"UnrealEd",
				"GraphEditor",
				"InputCore",
				"KismetCompiler",
			});

		PrivateIncludePathModuleNames.Add("Launch");

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	}
}