#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

/**
 * Checks that every /Game asset follows the prefix convention in _Docs/asset_structure.md (T_ textures,
 * M_ materials, WBP_ widgets, ...) and reports violations as a clickable Message Log list. Adds a
 * "Check Naming" entry to the shared "The" dropdown in the Content Browser toolbar.
 */
class FTheAssetLinter
{
public:
    static void CheckAssetNaming();
    static void FixAssetNaming();
    static void RegisterMenuEntry();

private:
    // One off-convention asset: its registry data and the prefix its name should start with.
    struct FViolation
    {
        FAssetData Asset;
        FString ExpectedPrefix;
    };

    static void GatherViolations(TArray<FViolation>& OutViolations, int32& OutChecked);
    static bool IsExcludedPackagePath(const FString& PackagePath);
    static bool ExpectedPrefixFor(const FAssetData& Asset, FString& OutPrefix);
};
