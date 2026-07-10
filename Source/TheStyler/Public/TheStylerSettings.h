// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "TheStylerSettings.generated.h"

/** Routing style for the restyled exec wires. */
UENUM()
enum class ETheWireStyle : uint8
{
    /** Right-angle elbows with rounded corners. */
    Manhattan,
    /** Horizontal leads joined by a 45-degree diagonal (metro-map look). */
    Metro45,
    /** Straight pin-to-pin lines. */
    Straight
};

/**
 * Editor settings for the TheStyler plugin — Project Settings -> Plugins -> "The Styler".
 * Tunes the exec-wire restyle (Manhattan/Metro/Straight), node auto-arrange spacing (Shift+Q),
 * and the Content Browser folder-color sync. Values are project-shared
 * (Config/DefaultEditor.ini) so the look is reproducible from the repository.
 */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "#The Styler"))
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

    /** Auto-apply the matching Standard Color to a folder the moment it is created (matched by leaf name). */
    UPROPERTY(config, EditAnywhere, Category = "The")
    bool bAutoColorNewFolders = true;

#pragma region Wires

    /** Master switch for the exec-wire restyle. Off = engine default wires. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires")
    bool bEnableWireStyling = true;

    /** Extra graph-schema class names (beyond Blueprint/K2, which is always styled) whose exec wires get
     * the restyle, matched by the schema's exact class name. This project ships dialogue/quest graphs;
     * a standalone project can clear this list or add its own custom graphs.
     * Note: only schemas WITHOUT their own connection-drawing policy can be restyled — the engine asks
     * the schema first and its stock editors (Niagara, Behavior Tree, PCG, Material, MetaSound,
     * Control Rig) all provide one or are claimed by an earlier engine factory, so listing them here
     * has no effect (UE 5.8 dispatch order). */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (EditCondition = "bEnableWireStyling"))
    TArray<FString> ExtraWireStylingSchemas = {TEXT("TheDialogueGraphSchema"), TEXT("TheQuestGraphSchema")};

    /** How restyled wires are routed (also cycled by the graph-toolbar Wires button). */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (EditCondition = "bEnableWireStyling"))
    ETheWireStyle WireStyle = ETheWireStyle::Manhattan;

    /** Also route data wires Manhattan-style. Off (default) = data wires keep the engine spline; they
     * never get flow dots, the arrowhead or the exec color override — only the right-angle routing. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (EditCondition = "bEnableWireStyling"))
    bool bManhattanDataWires = false;

    /** Multiplier on every wire's thickness (data wires included) for readability. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "4.0", EditCondition = "bEnableWireStyling"))
    float WireThicknessScale = 1.6f;

    /** Rounded-corner radius (graph units) at each right-angle bend. 0 = sharp corners. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (ClampMin = "0.0", UIMax = "48.0", EditCondition = "bEnableWireStyling"))
    float CornerRadius = 12.0f;

    /** Below this end-to-end distance (graph units) a wire is drawn straight instead of elbowed. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (ClampMin = "0.0", UIMax = "128.0", EditCondition = "bEnableWireStyling"))
    float MinManhattanDistance = 24.0f;

    /** Parallel wires whose vertical corridors land within this distance (graph units) of each other
     * are nudged apart by the same step so they read as separate lines instead of overlapping into
     * one. 0 = off. Applies to the Manhattan corridor and the Metro 45 diagonal. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (ClampMin = "0.0", UIMax = "32.0", EditCondition = "bEnableWireStyling"))
    float WireCorridorSpacing = 8.0f;

    /** Paint exec wires a fixed color instead of the pin-type color. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (EditCondition = "bEnableWireStyling"))
    bool bOverrideWireColor = false;

    /** Fixed exec-wire color used when the override is on. */
    UPROPERTY(config, EditAnywhere, Category = "The|Wires", meta = (EditCondition = "bEnableWireStyling && bOverrideWireColor"))
    FLinearColor WireColor = FLinearColor::White;

#pragma endregion

#pragma region Focus

    /** When one or more nodes are selected, fade the wires that don't touch the selection so the
     * selected node's connections stand out (focus mode). */
    UPROPERTY(config, EditAnywhere, Category = "The|Focus", meta = (EditCondition = "bEnableWireStyling"))
    bool bFocusDimOnSelection = true;

    /** Opacity multiplier applied to the faded (non-connected) wires. Lower = dimmer. */
    UPROPERTY(config, EditAnywhere, Category = "The|Focus", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.05", UIMax = "0.8", EditCondition = "bEnableWireStyling && bFocusDimOnSelection"))
    float FocusDimOpacity = 0.15f;

    /** Settings classes (exact class names) whose bDimUnselectedNodes flag the toolbar Focus toggle
     * flips in sync with this plugin's focus mode. This project ships the dialogue/quest graph
     * editors; a standalone project clears this list or adds its own editors' settings classes —
     * the toggle never touches (or persists) a settings class outside this list. */
    UPROPERTY(config, EditAnywhere, Category = "The|Focus", meta = (EditCondition = "bEnableWireStyling && bFocusDimOnSelection"))
    TArray<FString> FocusDimSettingsClasses = {TEXT("TheDialogueEditorSettings"), TEXT("TheQuestEditorSettings")};

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

    /** When on, adding a node (e.g. dragging off a pin) auto-arranges the wire-connected cluster it joins,
     * keeping the graph tidy as you build. Opt-in — off by default so node placement isn't surprising. */
    UPROPERTY(config, EditAnywhere, Category = "The|Arrange")
    bool bFormatOnNodeAdded = false;

#pragma endregion
};
