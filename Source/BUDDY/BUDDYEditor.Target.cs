using UnrealBuildTool;

public class BUDDYEditorTarget : TargetRules
{
	public BUDDYEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("BUDDY");
	}
}