#pragma once

#include "CoreMinimal.h"

struct FAssetData;

/**
 * Checks that every /Game asset follows the prefix convention in _Docs/asset_structure.md (T_ textures,
 * M_ materials, WBP_ widgets, ...) and reports violations as a clickable Message Log list. Adds a
 * "Check Naming" entry to the shared "The" dropdown in the Content Browser toolbar.
 */
class FTheAssetLinter
{
public:
    static void CheckAssetNaming();
    static void RegisterMenuEntry();

private:
    static bool IsExcludedPackagePath(const FString& PackagePath);
    static bool ExpectedPrefixFor(const FAssetData& Asset, FString& OutPrefix);
};
