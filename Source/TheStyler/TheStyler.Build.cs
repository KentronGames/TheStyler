using UnrealBuildTool;

public class TheStyler : ModuleRules
{
	public TheStyler(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Strict only in its home project: a buyer's toolchain or a newer engine must not turn a warning
		// into a hard failure of THEIR build over a plugin they cannot edit.
		bWarningsAsErrors = Target.ProjectFile != null && Target.ProjectFile.GetFileNameWithoutExtension() == "TheGame";

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
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
			"AssetRegistry",
			"ContentBrowser",
			"ContentBrowserData",
			"ToolWidgets",
			"Json",
		});
	}
}
