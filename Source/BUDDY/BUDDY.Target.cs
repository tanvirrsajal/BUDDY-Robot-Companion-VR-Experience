using UnrealBuildTool;

public class BUDDYTarget : TargetRules
{
	public BUDDYTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("BUDDY");
	}
}