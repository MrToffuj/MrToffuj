using UnrealBuildTool;

public class AnimeLook : ModuleRules
{
	public AnimeLook(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "Projects", "RenderCore", "Renderer", "RHI"
		});
	}
}
