#pragma once

#include "CoreMinimal.h"

struct FToolMenuContext;
class SGraphPanel;
class SWidget;

/** Static entry point that arranges the nodes of the active graph editor panel. */
class FTheGraphArranger
{
public:
    /** Arrange the active graph: selected nodes, or all nodes if none are selected. Used by the hotkey. */
    static void ArrangeActiveGraph();

    /** Arrange the graph owned by the asset editor whose toolbar this menu context belongs to. */
    static void ArrangeGraphFromContext(const FToolMenuContext& Context);

    /** True when that asset editor hosts a graph panel — gates the toolbar button to node editors. */
    static bool HasGraphInContext(const FToolMenuContext& Context);

private:
    static void ArrangeGraphPanel(const TSharedPtr<SGraphPanel>& GraphPanel);

    static TSharedPtr<SGraphPanel> FindActiveGraphPanel();
    static TSharedPtr<SGraphPanel> FindGraphPanelFromFocus();
    static TSharedPtr<SGraphPanel> FindGraphPanelInContext(const FToolMenuContext& Context);
    static TSharedPtr<SGraphPanel> FindGraphPanelRecursive(const TSharedRef<SWidget>& Widget);
};
