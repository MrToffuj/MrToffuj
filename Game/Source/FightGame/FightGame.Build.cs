using System.IO;
using UnrealBuildTool;

public class FightGame : ModuleRules
{
	public FightGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// FightCore lives in /FightCore and gets compiled in through Private/FightCore/*.cpp.
		// The sources reuse helper names in anonymous namespaces, so no unity build.
		bUseUnity = false;

		string FightCoreRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "FightCore"));
		PublicIncludePaths.Add(Path.Combine(FightCoreRoot, "include"));
		PublicIncludePaths.Add(Path.Combine(FightCoreRoot, "techniques"));
		PrivateIncludePaths.Add(Path.Combine(FightCoreRoot, "src"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "DeveloperSettings", "Sockets", "Networking"
		});
	}
}
