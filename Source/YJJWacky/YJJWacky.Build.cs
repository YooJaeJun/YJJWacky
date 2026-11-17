using UnrealBuildTool;

public class YJJWacky : ModuleRules
{
	public YJJWacky(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// "Physics/YJJWobbler.h" 처럼 모듈 루트 기준으로 include
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "PhysicsCore", "ProceduralMeshComponent" });

		// HUD 글자 크기 측정
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "RenderCore" });
	}
}
