// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "TheStylerSettings.generated.h"

UENUM()
enum class ETheStylerWireStyle : uint8
{
    Manhattan,
    Metro45,
    Straight
};

/**
 * Project-wide settings a team wants identical in every checkout: which folders get which colour, which
 * extra graph schemas take part, how the arranger spaces nodes. Stored in the project's DefaultEditor.ini.
 */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "#The Styler"))
class THESTYLER_API UTheStylerSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UTheStylerSettings();

    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

#pragma region Content Browser

    /** Label on the plugin's Content Browser toolbar button. Left empty, the button reads "The Styler". */
    UPROPERTY(config, EditAnywhere, Category = "Styler")
    FString ToolbarButtonLabel = TEXT("The Styler");

    /**
     * Content Browser folders kept out of the user's sight — service folders a project neither browses nor
     * edits by hand (e.g. "/Game/Splash", "/Game/Localization"). An entry is a long package path; a bare
     * folder name ("Splash") is read as "/Game/Splash", and an entry under no mounted root is reported in
     * the log rather than ignored. The deny-list goes up when the editor starts and comes down when the
     * plugin unloads, so a change here needs a restart. Ships empty: a project lists its own, and an empty
     * list hides nothing.
     */
    UPROPERTY(config, EditAnywhere, Category = "Styler", meta = (ContentDir, LongPackageName))
    TArray<FString> HiddenFolders;

#pragma endregion

#pragma region Folder colors

    /** Apply saved Content Browser folder colors when the editor starts. */
    UPROPERTY(config, EditAnywhere, Category = "Styler")
    bool bEnableFolderColorSync = false;

    /**
     * Colors used by The -> Standard Colors, which paints known structural folders project-wide.
     * Key = folder leaf name (e.g. "Materials"); a key starting with "*" matches by suffix
     * (e.g. "*_Data" colors every folder ending in "_Data"). Matching is case-insensitive.
     */
    UPROPERTY(config, EditAnywhere, Category = "Styler")
    TMap<FString, FLinearColor> StandardFolderColors;

    /** Auto-apply the matching Standard Color to a folder the moment it is created (matched by leaf name). */
    UPROPERTY(config, EditAnywhere, Category = "Styler")
    bool bAutoColorNewFolders = false;

    /**
     * Content roots that Standard Colors walks. Empty = every mounted project root, which covers a project
     * whose assets live in a content plugin rather than under /Game.
     */
    UPROPERTY(config, EditAnywhere, Category = "Styler")
    TArray<FString> ContentRootsToColor;

#pragma endregion

#pragma region Graphs

    /** Extra graph-schema class names (beyond Blueprint/K2, which is always styled) whose exec wires get
     * the restyle, matched by the schema's exact class name. Ships empty; a project adds its own custom
     * graphs here.
     * Note: only schemas WITHOUT their own connection-drawing policy can be restyled — the engine asks
     * the schema first and its stock editors (Niagara, Behavior Tree, PCG, Material, MetaSound,
     * Control Rig) all provide one or are claimed by an earlier engine factory, so listing them here
     * has no effect (UE 5.8 dispatch order). */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Wires")
    TArray<FString> ExtraWireStylingSchemas;

    /** Settings classes (exact class names) whose bDimUnselectedNodes flag the toolbar Focus toggle
     * flips in sync with this plugin's focus mode. Ships empty; a project adds its own graph editors'
     * settings classes — the toggle never touches a settings class outside this list. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Focus")
    TArray<FString> FocusDimSettingsClasses;

    /** Horizontal gap (graph units) between columns when arranging nodes (Shift+Q). */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Arrange", meta = (ClampMin = "0.0", UIMax = "400.0"))
    float NodeSpacingX = 100.0f;

    /** Vertical gap (graph units) between stacked nodes within a column. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Arrange", meta = (ClampMin = "0.0", UIMax = "200.0"))
    float NodeSpacingY = 32.0f;

    /** Crossing-reduction sweeps over the layout; higher = tidier but slower on huge graphs. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Arrange", meta = (ClampMin = "0", ClampMax = "16"))
    int32 NodeOrderingPasses = 4;

#pragma endregion
};

/**
 * How graphs LOOK to one person at one machine. Stored per user (EditorPerProjectUserSettings), so the
 * toolbar toggles do not dirty a source-controlled file or flip the setting under a teammate.
 */
UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "#The Styler (View)"))
class THESTYLER_API UTheStylerViewSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

#pragma region Wires

    /** Master switch for the exec-wire restyle. Off = engine default wires. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Wires")
    bool bEnableWireStyling = true;

    /** How restyled wires are routed (also cycled by the graph-toolbar Wires button). */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Wires", meta = (EditCondition = "bEnableWireStyling"))
    ETheStylerWireStyle WireStyle = ETheStylerWireStyle::Manhattan;

    /** Also route data wires Manhattan-style. Off (default) = data wires keep the engine spline; they
     * never get flow dots, the arrowhead or the exec color override — only the right-angle routing. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Wires", meta = (EditCondition = "bEnableWireStyling"))
    bool bManhattanDataWires = false;

    /** Multiplier on every wire's thickness (data wires included) for readability. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Wires", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "4.0", EditCondition = "bEnableWireStyling"))
    float WireThicknessScale = 1.0f;

    /** Rounded-corner radius (graph units) at each right-angle bend. 0 = sharp corners. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Wires", meta = (ClampMin = "0.0", UIMax = "48.0", EditCondition = "bEnableWireStyling"))
    float CornerRadius = 12.0f;

    /** Below this end-to-end distance (graph units) a wire is drawn straight instead of elbowed. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Wires", meta = (ClampMin = "0.0", UIMax = "128.0", EditCondition = "bEnableWireStyling"))
    float MinManhattanDistance = 24.0f;

    /** Parallel wires whose vertical corridors land within this distance (graph units) of each other
     * are nudged apart by the same step so they read as separate lines instead of overlapping into
     * one. 0 = off. Applies to the Manhattan corridor and the Metro 45 diagonal. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Wires", meta = (ClampMin = "0.0", UIMax = "32.0", EditCondition = "bEnableWireStyling"))
    float WireCorridorSpacing = 8.0f;

    /** Paint exec wires a fixed color instead of the pin-type color. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Wires", meta = (EditCondition = "bEnableWireStyling"))
    bool bOverrideWireColor = false;

    /** Fixed exec-wire color used when the override is on. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Wires", meta = (EditCondition = "bEnableWireStyling && bOverrideWireColor"))
    FLinearColor WireColor = FLinearColor::White;

#pragma endregion

#pragma region Focus

    /** When one or more nodes are selected, fade the wires that don't touch the selection so the
     * selected node's connections stand out (focus mode). */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Focus", meta = (EditCondition = "bEnableWireStyling"))
    bool bFocusDimOnSelection = true;

    /** Opacity multiplier applied to the faded (non-connected) wires. Lower = dimmer. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Focus", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.05", UIMax = "0.8", EditCondition = "bEnableWireStyling && bFocusDimOnSelection"))
    float FocusDimOpacity = 0.15f;

#pragma endregion

#pragma region Arrange

    /** When on, adding a node (e.g. dragging off a pin) auto-arranges the wire-connected cluster it joins,
     * keeping the graph tidy as you build. Opt-in — off by default so node placement isn't surprising. */
    UPROPERTY(config, EditAnywhere, Category = "Styler|Arrange")
    bool bFormatOnNodeAdded = false;

#pragma endregion
};
