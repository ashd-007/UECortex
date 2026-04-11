using UnrealBuildTool;

public class UECortexIKRetarget : ModuleRules
{
	public UECortexIKRetarget(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Json",
			"JsonUtilities",
			"AssetRegistry",
			"AssetTools",
			"UECortex",      // FMCPToolRegistry + FMCPToolBase
			"IKRig",         // UIKRigDefinition, UIKRetargeter
			"IKRigEditor",   // UIKRetargeterController
		});
	}
}
