using UnrealBuildTool;

public class TheStyler : ModuleRules
{
	public TheStyler(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Zero-warnings policy is compiler-enforced for project code (owner decision 2026-07-10).
		bWarningsAsErrors = true;

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
