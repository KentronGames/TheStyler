// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheFolderVisibility.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/NamePermissionList.h"
#include "Misc/PackageName.h"

#include "TheStylerModule.h"
#include "TheStylerSettings.h"

namespace TheFolderVisibility
{
const TCHAR* const DenyListOwner = TEXT("TheStyler");
const TCHAR* const DefaultContentRoot = TEXT("/Game/");

/**
 * A deny-list entry hides something only when it is an absolute long package path. The permission list keeps its
 * entries in a TDirectoryTree, whose contract says relative and absolute paths are never resolved against each
 * other and never compare equal, so a configured "Splash" sits in the tree beside /Game/Splash and matches nothing.
 * Nothing complains either: the list only format-checks entries when it holds class paths.
 *
 * A bare folder name is what a person types, and it means /Game/<name>. Conform it rather than drop it silently.
 * A trailing separator is ignored by the tree, so entries never need to be added twice.
 */
bool ConformFolderPath(const FString& Configured, FString& OutFolderPath)
{
    OutFolderPath = Configured;
    OutFolderPath.TrimStartAndEndInline();
    OutFolderPath.ReplaceInline(TEXT("\\"), TEXT("/"));
    while(OutFolderPath.RemoveFromEnd(TEXT("/")))
    {
    }
    if(OutFolderPath.IsEmpty())
    {
        return false;
    }
    if(!OutFolderPath.StartsWith(TEXT("/")))
    {
        OutFolderPath = DefaultContentRoot + OutFolderPath;
    }
    return true;
}
}

void FTheFolderVisibility::HideConfiguredFolders()
{
    TArray<FString> Hidden;
    TArray<FString> Unmounted;

    ForEachDenyListEntry(
        [&Hidden, &Unmounted](const FString& FolderPath)
        {
            IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
            const TSharedPtr<FPathPermissionList> FolderPermissionList = AssetTools.GetFolderPermissionList();
            if(!FolderPermissionList)
            {
                return;
            }

            // An entry outside every mounted root is accepted by the list and hides nothing, which is exactly
            // how a typo survives unnoticed. Report it; a root mounted later still gets its entry.
            if(FPackageName::GetPackageMountPoint(FolderPath).IsNone())
            {
                Unmounted.Add(FolderPath);
            }

            FolderPermissionList->AddDenyListItem(TheFolderVisibility::DenyListOwner, FolderPath);
            Hidden.Add(FolderPath);
        });

    if(Hidden.Num() > 0)
    {
        UE_LOG(LogTheStyler, Log, TEXT("Hid %d Content Browser folder path(s): %s"), Hidden.Num(), *FString::Join(Hidden, TEXT(", ")));
    }

    for(const FString& FolderPath : Unmounted)
    {
        UE_LOG(LogTheStyler, Warning, TEXT("HiddenFolders entry '%s' is not under a mounted content root and hides nothing."), *FolderPath);
    }
}

void FTheFolderVisibility::Shutdown()
{
    if(!FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
    {
        return;
    }

    ForEachDenyListEntry(
        [](const FString& FolderPath)
        {
            IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
            if(const TSharedPtr<FPathPermissionList> FolderPermissionList = AssetTools.GetFolderPermissionList())
            {
                FolderPermissionList->RemoveDenyListItem(TheFolderVisibility::DenyListOwner, FolderPath);
            }
        });
}

void FTheFolderVisibility::ForEachDenyListEntry(const TFunctionRef<void(const FString&)>& Visitor)
{
    for(const FString& Configured : GetDefault<UTheStylerSettings>()->HiddenFolders)
    {
        FString FolderPath;
        if(TheFolderVisibility::ConformFolderPath(Configured, FolderPath))
        {
            Visitor(FolderPath);
        }
    }
}
