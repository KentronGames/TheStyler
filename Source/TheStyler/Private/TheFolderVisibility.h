// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class FTheFolderVisibility
{
public:
    static void HideConfiguredFolders();
    static void Shutdown();

private:
    static void ForEachDenyListEntry(const TFunctionRef<void(const FString&)>& Visitor);
};
