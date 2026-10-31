// Copyright yoomsGames. All Rights Reserved.

using UnrealBuildTool;

public class PT : ModuleRules
{
	public PT(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory + "/Public");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"NetCore",
			"CoreOnline",
			"DeveloperSettings",

			// Gameplay Ability System
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",

			// Motion Matching
			"PoseSearch",
			"AnimGraphRuntime",
			"AnimationCore",
			"BlendStack",
			"MotionWarping",

			// Online / Session
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UMG",
			"RenderCore",
			"RHI",
			"EngineSettings"
		});

		// OnlineSubsystemNull / Steam are loaded dynamically through DefaultPlatformService (DefaultEngine.ini)
		DynamicallyLoadedModuleNames.AddRange(new string[]
		{
			"OnlineSubsystemNull",
			"OnlineSubsystemSteam"
		});
	}
}
