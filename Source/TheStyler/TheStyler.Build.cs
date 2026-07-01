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
			"DeveloperSettings",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"GraphEditor",
			"BlueprintGraph",
			"ToolMenus",
			"MainFrame",
			"MessageLog",
			// Folder-color sync
			"AssetTools",
			"AssetRegistry",
			"ContentBrowser",
			"ContentBrowserData",
			"ToolWidgets",
			"Json",
		});
	}
}
