// Copyright 2026 Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class TurretMind : ModuleRules
{
	public TurretMind(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// One runtime module and nothing else.
		//
		// Deliberately NOT here:
		//   AIModule  - the service has to run in a project that never touches a Behavior Tree or the
		//               perception system. A tower defence turret is not a pawn with a brain component.
		//   UnrealEd  - everything ships; the debug draw is guarded with WITH_EDITOR / ENABLE_DRAW_DEBUG.
		//   UMG       - the statistics box is UCanvas from AHUD, so it survives a cooked Shipping build.
		//
		// RenderCore is here for GWhiteTexture, which the statistics box background is drawn with.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
		});
	}
}
