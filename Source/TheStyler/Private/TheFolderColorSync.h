#pragma once

#include "CoreMinimal.h"

/**
 * Syncs Content Browser folder colors to/from Config/EditorFolderColors.json (so they travel with
 * the project) and adds a "Save Colors" entry to the "The" dropdown in the Content Browser toolbar.
 */
class FTheFolderColorSync
{
public:
    static void ApplySavedFolderColors();
    static void SaveCurrentFolderColors();
    static void RainbowCurrentFolder();
    static void ApplyStandardFolderColors();
    static void RegisterMenuEntry();

private:
    static FString GetFolderColorsFilePath();
    static TMap<FString, FLinearColor> LoadEditorConfigFolderColors();
    static int32 PruneMissingFolders(TMap<FString, FLinearColor>& FolderColors);
    static bool DoesFolderExistOnDisk(const FString& FolderPath);
    static bool LoadProjectFolderColors(TMap<FString, FLinearColor>& OutFolderColors);
    static bool WriteProjectFolderColors(const TMap<FString, FLinearColor>& FolderColors);
    static void Notify(const FText& Message, bool bSuccess);
};
