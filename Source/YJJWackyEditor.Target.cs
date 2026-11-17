using UnrealBuildTool;

public class YJJWackyEditorTarget : TargetRules
{
	public YJJWackyEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("YJJWacky");
	}
}
