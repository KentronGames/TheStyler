// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheStylerSettings.h"

UTheStylerSettings::UTheStylerSettings()
{
    StandardFolderColors = {
        {TEXT("Meshes"), FLinearColor(0.15f, 0.35f, 0.85f)},
        {TEXT("Materials"), FLinearColor(0.85f, 0.25f, 0.20f)},
        {TEXT("Textures"), FLinearColor(0.20f, 0.70f, 0.35f)},
        {TEXT("FX"), FLinearColor(0.85f, 0.30f, 0.70f)},
        {TEXT("Common"), FLinearColor(0.20f, 0.65f, 0.70f)},
        {TEXT("Resources"), FLinearColor(0.90f, 0.70f, 0.20f)},
        {TEXT("Maps"), FLinearColor(0.55f, 0.45f, 0.25f)},
        {TEXT("Core"), FLinearColor(0.50f, 0.50f, 0.55f)},
        {TEXT("Input"), FLinearColor(0.90f, 0.50f, 0.15f)},
        {TEXT("*_Data"), FLinearColor(0.55f, 0.35f, 0.80f)},
    };
}
