// Copyright yoomsGames. All Rights Reserved.

using UnrealBuildTool;

public class PTEditor : ModuleRules
{
	public PTEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AnimGraph",
			"AnimGraphRuntime",
			"BlueprintGraph",
			"PT"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"PoseSearch",
			"Slate",
			"SlateCore"
		});
	}
}
