using UnrealBuildTool;

public class TheStyler : ModuleRules
{
	public TheStyler(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"GraphEditor",
			"BlueprintGraph",
			"ToolMenus",
			"MainFrame",
			// Folder-color sync
			"AssetTools",
			"ContentBrowser",
			"ToolWidgets",
			"Json",
		});
	}
}
