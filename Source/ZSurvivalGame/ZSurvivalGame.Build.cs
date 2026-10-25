// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ZSurvivalGame : ModuleRules
{
	public ZSurvivalGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ZSurvivalGame",
			"ZSurvivalGame/Variant_Platforming",
			"ZSurvivalGame/Variant_Platforming/Animation",
			"ZSurvivalGame/Variant_Combat",
			"ZSurvivalGame/Variant_Combat/AI",
			"ZSurvivalGame/Variant_Combat/Animation",
			"ZSurvivalGame/Variant_Combat/Gameplay",
			"ZSurvivalGame/Variant_Combat/Interfaces",
			"ZSurvivalGame/Variant_Combat/UI",
			"ZSurvivalGame/Variant_SideScrolling",
			"ZSurvivalGame/Variant_SideScrolling/AI",
			"ZSurvivalGame/Variant_SideScrolling/Gameplay",
			"ZSurvivalGame/Variant_SideScrolling/Interfaces",
			"ZSurvivalGame/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
