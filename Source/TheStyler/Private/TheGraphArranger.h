#pragma once

#include "CoreMinimal.h"

class SGraphPanel;
class SWidget;

/** Static entry point that arranges the nodes of the active graph editor panel. */
class FTheGraphArranger
{
public:
    /** Arrange the active graph: selected nodes, or all nodes if none are selected. */
    static void ArrangeActiveGraph();

    /** True when the active editor tab hosts a graph panel — used to gate the toolbar button to node editors. */
    static bool HasActiveGraph();

private:
    static TSharedPtr<SGraphPanel> FindActiveGraphPanel();
    static TSharedPtr<SGraphPanel> FindGraphPanelRecursive(const TSharedRef<SWidget>& Widget);
};
