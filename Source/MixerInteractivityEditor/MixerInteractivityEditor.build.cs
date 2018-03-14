using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class MixerInteractivityEditor : ModuleRules
{
	public MixerInteractivityEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MixerInteractivity",
				"MixerInteractivityBlueprintNodes",
				"PropertyEditor",
				"SlateCore",
				"Slate",
				"HTTP",
				"Json",
				"EditorStyle",
				"BlueprintGraph",
				"UnrealEd",
				"GraphEditor",
				"ContentBrowser",
				"InputCore",
				"KismetCompiler",
				"AssetTools",
				"Projects",
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"Settings",
			});
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	}
}