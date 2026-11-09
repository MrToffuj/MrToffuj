using UnrealBuildTool;

public class PlayStationPads : ModuleRules
{
	public PlayStationPads(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "ApplicationCore", "InputCore", "InputDevice" });

		// HID access straight through Windows
		if (Target.Platform == UnrealTargetPlatform.Win64)
			PublicSystemLibraries.AddRange(new string[] { "hid.lib", "setupapi.lib" });
	}
}
