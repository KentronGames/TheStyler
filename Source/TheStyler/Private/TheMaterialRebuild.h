#pragma once

#include "CoreMinimal.h"

struct FToolMenuContext;

/**
 * Adds ImportHLSL / ExportHLSL buttons to the Material editor toolbar (visible only while editing a
 * UMaterial):
 *   ImportHLSL — rebuild the open material from its .hlsl source of truth (TheMCP RebuildMaterialFromSource).
 *   ExportHLSL — write the material's current parameter defaults back into the .hlsl header, so a later
 *                Import keeps the tuned values (TheMCP ExportMaterialDefaultsToSource).
 * Both reopen the editor afterwards so the regenerated graph is shown.
 */
class FTheMaterialRebuild
{
public:
    static void RegisterMenuEntry();

private:
    static class UMaterial* MaterialFromContext(const FToolMenuContext& Context);
    static bool HasMaterialInContext(const FToolMenuContext& Context);
    static void ImportFromContext(const FToolMenuContext& Context);
    static void ExportFromContext(const FToolMenuContext& Context);
    static void ApplyResult(class UMaterial* Material, const FString& Result, const FText& SuccessText);
};
