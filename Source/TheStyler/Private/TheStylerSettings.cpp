// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheStylerSettings.h"

UTheStylerSettings::UTheStylerSettings()
{
    // Default palette for The -> Standard Colors (matches the /Game structure in .claude/docs/asset_structure.md).
    // Distinct, readable hues; the owner can retune or extend these in Project Settings.
    StandardFolderColors = {
        {TEXT("Meshes"), FLinearColor(0.15f, 0.35f, 0.85f)}, // blue
        {TEXT("Materials"), FLinearColor(0.85f, 0.25f, 0.20f)}, // red
        {TEXT("Textures"), FLinearColor(0.20f, 0.70f, 0.35f)}, // green
        {TEXT("FX"), FLinearColor(0.85f, 0.30f, 0.70f)}, // magenta
        {TEXT("Common"), FLinearColor(0.20f, 0.65f, 0.70f)}, // teal
        {TEXT("Resources"), FLinearColor(0.90f, 0.70f, 0.20f)}, // amber
        {TEXT("Maps"), FLinearColor(0.55f, 0.45f, 0.25f)}, // brown
        {TEXT("Core"), FLinearColor(0.50f, 0.50f, 0.55f)}, // gray
        {TEXT("Input"), FLinearColor(0.90f, 0.50f, 0.15f)}, // orange
        {TEXT("*_Data"), FLinearColor(0.55f, 0.35f, 0.80f)}, // violet (suffix match)
    };
}
