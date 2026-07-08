using UnrealBuildTool;

public class BUDDY : ModuleRules
{
    public BUDDY(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore",
            "HeadMountedDisplay", "UMG", "Slate", "SlateCore",
            "HTTP", "Json", "JsonUtilities", "OculusHMD", "MediaAssets", 
            "AudioMixer", "AudioCapture", "AudioCaptureCore", "Voice"
        });
    }
}


// using UnrealBuildTool;

// public class BUDDY : ModuleRules
// {
// 	public BUDDY(ReadOnlyTargetRules Target) : base(Target)
// 	{
// 		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
// 		PublicDependencyModuleNames.AddRange(new string[]
// 		{
// 			"Core", "CoreUObject", "Engine", "InputCore",
// 			"HeadMountedDisplay", "UMG", "Slate", "SlateCore",
// 			"HTTP", "Json", "JsonUtilities", "OculusHMD", "MediaAssets", "AudioMixer", "AudioCapture", "Voice"
// 		});
// 	}
// }
