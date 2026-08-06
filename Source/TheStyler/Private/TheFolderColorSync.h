// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class FTheFolderColorSync
{
public:
    static void ApplySavedFolderColors();
    static void SaveCurrentFolderColors();
    static void RainbowCurrentFolder();
    static void ApplyStandardFolderColors();
    static void RegisterMenuEntry();

    static void RegisterAutoColorHandler();

private:
    static void HandlePathAdded(const FString& Path);
    static bool FindStandardColorForPath(const FString& Path, FLinearColor& OutColor);
    static FString GetFolderColorsFilePath();
    static TMap<FString, FLinearColor> LoadEditorConfigFolderColors();
    static int32 PruneMissingFolders(TMap<FString, FLinearColor>& FolderColors);
    static bool DoesFolderExistOnDisk(const FString& FolderPath);
    static bool LoadProjectFolderColors(TMap<FString, FLinearColor>& OutFolderColors);
    static bool WriteProjectFolderColors(const TMap<FString, FLinearColor>& FolderColors);
    static void Notify(const FText& Message, bool bSuccess);
};
