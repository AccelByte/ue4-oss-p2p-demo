// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

using System.IO;
using UnrealBuildTool;

public class MRWebRTC : ModuleRules
{
	public MRWebRTC(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));
			if (Target.Configuration == UnrealTargetConfiguration.Debug ||
			    Target.Configuration == UnrealTargetConfiguration.Development ||
			    Target.Configuration == UnrealTargetConfiguration.DebugGame)
			{
				RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "x64/debug/mrwebrtc.dll"), StagedFileType.NonUFS);
				PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "x64/debug/mrwebrtc.lib"));
			}
			else
			{
				RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "x64/release/mrwebrtc.dll"), StagedFileType.NonUFS);
				PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "x64/release/mrwebrtc.lib"));
			}
			PublicDelayLoadDLLs.Add("mrwebrtc.dll");
        }
	}
}
