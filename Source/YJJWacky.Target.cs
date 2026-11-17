using UnrealBuildTool;

public class YJJWackyTarget : TargetRules
{
	public YJJWackyTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("YJJWacky");
	}
}
