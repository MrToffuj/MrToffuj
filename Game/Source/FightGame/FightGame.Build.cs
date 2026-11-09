using System.IO;
using UnrealBuildTool;

public class FightGame : ModuleRules
{
	public FightGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// FightCore (/FightCore im Repo) ist reines C++ und wird über Private/FightCore/*.cpp mitkompiliert.
		// Die Dateien nutzen pro Datei gleiche Namen in anonymen Namespaces -> kein Unity-Build.
		bUseUnity = false;

		string FightCoreRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "FightCore"));
		PublicIncludePaths.Add(Path.Combine(FightCoreRoot, "include"));
		PublicIncludePaths.Add(Path.Combine(FightCoreRoot, "techniques"));
		PrivateIncludePaths.Add(Path.Combine(FightCoreRoot, "src"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "DeveloperSettings"
		});
	}
}
