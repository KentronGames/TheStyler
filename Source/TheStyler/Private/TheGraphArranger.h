// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FToolMenuContext;
class SGraphPanel;
class SWidget;
class UEdGraphNode;

/** Static entry point that arranges the nodes of the active graph editor panel. */
class FTheGraphArranger
{
public:
    /** Arrange the active graph: selected nodes, or all nodes if none are selected. Used by the hotkey. */
    static void ArrangeActiveGraph();

    /** Arrange only the wire-connected component(s) of the active graph's selected node(s) (Format Node). */
    static void FormatActiveSelection();

    /** Arrange the graph owned by the asset editor whose toolbar this menu context belongs to. */
    static void ArrangeGraphFromContext(const FToolMenuContext& Context);

    /** True when that asset editor hosts a graph panel — gates the toolbar button to node editors. */
    static bool HasGraphInContext(const FToolMenuContext& Context);

    /** Which shared edge/center the selected nodes align to. */
    enum class ETheAlign : uint8
    {
        Left,
        Right,
        Top,
        Bottom,
        CenterX, // shared vertical center line (equal horizontal centers)
        CenterY, // shared horizontal center line (equal vertical centers)
    };

    /** Axis along which the selected nodes' gaps are evened out. */
    enum class ETheDistribute : uint8
    {
        Horizontal,
        Vertical,
    };

    /** Align the active graph's selected nodes to a shared edge/center (needs 2+ selected nodes). */
    static void AlignActiveSelection(ETheAlign Mode);

    /** Even out the gaps between the active graph's selected nodes along an axis (needs 3+ nodes). */
    static void DistributeActiveSelection(ETheDistribute Axis);

    /** The graph panel the user is currently working in (focus / active tab / active window). */
    static TSharedPtr<SGraphPanel> GetActiveGraphPanel();

    /** Arrange the wire-connected component(s) containing Seeds in the active graph (format-on-connect). */
    static void FormatComponentInActivePanel(const TSet<UEdGraphNode*>& Seeds);

private:
    enum class EArrangeScope : uint8
    {
        SelectedOrAll, // selected nodes, or all if none are selected (whole-graph tidy)
        ConnectedComponentOfSelection, // only the wire-connected component(s) of the selected node(s)
    };

    static void ArrangeGraphPanel(const TSharedPtr<SGraphPanel>& GraphPanel, EArrangeScope Scope, const TSet<UEdGraphNode*>* ExplicitSeeds = nullptr);

    /** BFS the wire graph out from Seeds (both directions), collecting every reachable non-comment node. */
    static void GatherConnectedComponent(const TSet<UEdGraphNode*>& Seeds, TSet<UEdGraphNode*>& OutComponent);

    static TSharedPtr<SGraphPanel> FindActiveGraphPanel();
    static TSharedPtr<SGraphPanel> FindGraphPanelFromFocus();
    static TSharedPtr<SGraphPanel> FindGraphPanelInContext(const FToolMenuContext& Context);
    static TSharedPtr<SGraphPanel> FindGraphPanelRecursive(const TSharedRef<SWidget>& Widget);
};
