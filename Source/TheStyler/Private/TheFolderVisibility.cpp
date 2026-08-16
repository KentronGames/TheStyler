// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheFolderVisibility.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/NamePermissionList.h"

#include "TheStylerModule.h"
#include "TheStylerSettings.h"

namespace TheFolderVisibility
{
const TCHAR* const DenyListOwner = TEXT("TheStyler");
}

void FTheFolderVisibility::HideConfiguredFolders()
{
    int32 HiddenCount = 0;
    ForEachDenyListEntry(
        [&HiddenCount](const FString& FolderPath)
        {
            IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
            if(const TSharedPtr<FPathPermissionList> FolderPermissionList = AssetTools.GetFolderPermissionList())
            {
                FolderPermissionList->AddDenyListItem(TheFolderVisibility::DenyListOwner, FolderPath);
                ++HiddenCount;
            }
        });

    if(HiddenCount > 0)
    {
        UE_LOG(LogTheStyler, Log, TEXT("Hid %d Content Browser folder path(s)."), HiddenCount);
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
        FString FolderPath = Configured;
        FolderPath.TrimStartAndEndInline();
        FolderPath.RemoveFromEnd(TEXT("/"));
        if(FolderPath.IsEmpty())
        {
            continue;
        }

        Visitor(FolderPath);
        Visitor(FolderPath + TEXT("/"));
    }
}
