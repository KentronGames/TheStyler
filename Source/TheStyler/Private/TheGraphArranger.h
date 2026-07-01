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

private:
    enum class EArrangeScope : uint8
    {
        SelectedOrAll, // selected nodes, or all if none are selected (whole-graph tidy)
        ConnectedComponentOfSelection, // only the wire-connected component(s) of the selected node(s)
    };

    static void ArrangeGraphPanel(const TSharedPtr<SGraphPanel>& GraphPanel, EArrangeScope Scope);

    /** BFS the wire graph out from Seeds (both directions), collecting every reachable non-comment node. */
    static void GatherConnectedComponent(const TSet<UEdGraphNode*>& Seeds, TSet<UEdGraphNode*>& OutComponent);

    static TSharedPtr<SGraphPanel> FindActiveGraphPanel();
    static TSharedPtr<SGraphPanel> FindGraphPanelFromFocus();
    static TSharedPtr<SGraphPanel> FindGraphPanelInContext(const FToolMenuContext& Context);
    static TSharedPtr<SGraphPanel> FindGraphPanelRecursive(const TSharedRef<SWidget>& Widget);
};
