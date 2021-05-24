// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

using System;
using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemAccelByte : ModuleRules
{
	public OnlineSubsystemAccelByte(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDefinitions.Add("ONLINESUBSYSTEMACCELBYTE_PACKAGE=1");
		PrivateDefinitions.Add("USE_SIMPLE_SIGNALING=1");

		String PlatformString = Target.Platform.ToString().ToUpper();
		
		if (PlatformString == "PS5")
		{
			bAllowConfidentialPlatformDefines = true;
		}

		if (PlatformString == "WIN64" || 
		    PlatformString == "XBOXONEGDK" ||
		    PlatformString == "XSX" ||
		    PlatformString == "PS4" ||
		    PlatformString == "PS5" )
		{
			PrivateDefinitions.Add("LIBJUICE");
		}
		
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
			}
		);

		
		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"OnlineSubsystemUtils"
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"LibJuice",
				"CoreUObject",
				"Projects",
				"WebSockets",
				"NetCore",
				"Engine",
				"Sockets",
				"OnlineSubsystem",
				"PacketHandler",
				"Json",
				"JsonUtilities"
			}
		);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
