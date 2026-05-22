// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DerbyDemo : ModuleRules
{
	public DerbyDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ChaosVehicles",
			"PhysicsCore",
			"UMG",
			"Slate",
			"GeometryCollectionEngine",
			"AIModule"
		});

		PublicIncludePaths.AddRange(new string[] {
			"DerbyDemo",
			"DerbyDemo/SportsCar",
			"DerbyDemo/OffroadCar",
			"DerbyDemo/Variant_Offroad",
			"DerbyDemo/Variant_TimeTrial",
			"DerbyDemo/Variant_TimeTrial/UI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
