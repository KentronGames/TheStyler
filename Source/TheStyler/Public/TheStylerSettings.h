#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "TheStylerSettings.generated.h"

/**
 * Editor settings for the TheStyler plugin — Project Settings -> Plugins -> "The Styler".
 * Tunes the Manhattan wire restyle, the animated flow dots along exec wires, node auto-arrange
 * spacing (Shift+Q), and the Content Browser folder-color sync. Values are project-shared
 * (Config/DefaultEditor.ini) so the look is reproducible from the repository.
 */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "*** The Styler"))
class THESTYLER_API UTheStylerSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UTheStylerSettings();

    // This is an editor plugin, so its settings live under Project Settings -> Plugins, not a game category.
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

    /** Apply saved Content Browser folder colors (Config/EditorFolderColors.json) when the editor starts. */
    UPROPERTY(config, EditAnywhere, Category = "The")
    bool bEnableFolderColorSync = true;

    /**
     * Colors used by The -> Standard Colors, which paints known structural folders project-wide.
     * Key = folder leaf name (e.g. "Materials"); a key starting with "*" matches by suffix
     * (e.g. "*_Data" colors every folder ending in "_Data"). Matching is case-insensitive.
     */
    UPROPERTY(config, EditAnywhere, Category = "The")
    TMap<FString, FLinearColor> StandardFolderColors;

#pragma region Wires

    /** Master switch for the right-angle (Manhattan) restyle of exec wires. Off = engine default wires. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires")
    bool bEnableWireStyling = true;

    /** Multiplier on every wire's thickness (data wires included) for readability. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "4.0", EditCondition = "bEnableWireStyling"))
    float WireThicknessScale = 1.6f;

    /** Rounded-corner radius (graph units) at each right-angle bend. 0 = sharp corners. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (ClampMin = "0.0", UIMax = "48.0", EditCondition = "bEnableWireStyling"))
    float CornerRadius = 12.0f;

    /** Below this end-to-end distance (graph units) a wire is drawn straight instead of elbowed. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (ClampMin = "0.0", UIMax = "128.0", EditCondition = "bEnableWireStyling"))
    float MinManhattanDistance = 24.0f;

    /** Paint exec wires a fixed color instead of the pin-type color. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (EditCondition = "bEnableWireStyling"))
    bool bOverrideWireColor = false;

    /** Fixed exec-wire color used when the override is on. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (EditCondition = "bEnableWireStyling && bOverrideWireColor"))
    FLinearColor WireColor = FLinearColor::White;

#pragma endregion

#pragma region Bubbles

    /** Draw the animated flow dots that run along exec wires. */
    UPROPERTY(config, EditAnywhere, Category = "The|Bubbles", meta = (EditCondition = "bEnableWireStyling"))
    bool bShowExecBubbles = true;

    /** Distance (graph units, pre-zoom) between consecutive flow dots. */
    UPROPERTY(config, EditAnywhere, Category = "The|Bubbles", meta = (ClampMin = "1.0", UIMin = "8.0", UIMax = "256.0", EditCondition = "bEnableWireStyling && bShowExecBubbles"))
    float BubbleSpacing = 64.0f;

    /** Travel speed (graph units per second, pre-zoom) of the flow dots. */
    UPROPERTY(config, EditAnywhere, Category = "The|Bubbles", meta = (ClampMin = "0.0", UIMax = "512.0", EditCondition = "bEnableWireStyling && bShowExecBubbles"))
    float BubbleSpeed = 192.0f;

    /** Flow-dot size as a fraction of the bubble image, further scaled by wire thickness. */
    UPROPERTY(config, EditAnywhere, Category = "The|Bubbles", meta = (ClampMin = "0.01", UIMax = "1.0", EditCondition = "bEnableWireStyling && bShowExecBubbles"))
    float BubbleSizeScale = 0.2f;

    /** Tint the flow dots a fixed color instead of matching the wire. */
    UPROPERTY(config, EditAnywhere, Category = "The|Bubbles", meta = (EditCondition = "bEnableWireStyling && bShowExecBubbles"))
    bool bOverrideBubbleColor = false;

    /** Fixed flow-dot color used when the override is on. */
    UPROPERTY(config, EditAnywhere, Category = "The|Bubbles", meta = (EditCondition = "bEnableWireStyling && bShowExecBubbles && bOverrideBubbleColor"))
    FLinearColor BubbleColor = FLinearColor::White;

    /** Draw a direction arrowhead near each exec wire's input pin. */
    UPROPERTY(config, EditAnywhere, Category = "The|Bubbles", meta = (EditCondition = "bEnableWireStyling"))
    bool bShowDirectionArrow = true;

#pragma endregion

#pragma region Arrange

    /** Horizontal gap (graph units) between columns when arranging nodes (Shift+Q). */
    UPROPERTY(config, EditAnywhere, Category = "The|Arrange", meta = (ClampMin = "0.0", UIMax = "400.0"))
    float NodeSpacingX = 100.0f;

    /** Vertical gap (graph units) between stacked nodes within a column. */
    UPROPERTY(config, EditAnywhere, Category = "The|Arrange", meta = (ClampMin = "0.0", UIMax = "200.0"))
    float NodeSpacingY = 32.0f;

    /** Crossing-reduction sweeps over the layout; higher = tidier but slower on huge graphs. */
    UPROPERTY(config, EditAnywhere, Category = "The|Arrange", meta = (ClampMin = "0", ClampMax = "16"))
    int32 NodeOrderingPasses = 4;

#pragma endregion
};
