// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class coral_larvae_abm : ModuleRules
{
	public coral_larvae_abm(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				//Path.Combine(ModuleDirectory, "../ThirdParty/Eigen")
			}
		);
		
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Niagara", "ApplicationCore" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });
		
		PublicDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
