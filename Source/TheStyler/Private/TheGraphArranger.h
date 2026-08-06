// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FToolMenuContext;
class SGraphPanel;
class SWidget;
class UEdGraphNode;

class FTheGraphArranger
{
public:
    static void ArrangeActiveGraph();

    static void FormatActiveSelection();

    static void ArrangeGraphFromContext(const FToolMenuContext& Context);

    static bool HasGraphInContext(const FToolMenuContext& Context);

    enum class ETheAlign : uint8
    {
        Left,
        Right,
        Top,
        Bottom,
        CenterX,
        CenterY,
    };

    enum class ETheDistribute : uint8
    {
        Horizontal,
        Vertical,
    };

    static void AlignActiveSelection(ETheAlign Mode);

    static void DistributeActiveSelection(ETheDistribute Axis);

    static TSharedPtr<SGraphPanel> GetActiveGraphPanel();

    static void FormatComponentInActivePanel(const TSet<UEdGraphNode*>& Seeds);

private:
    enum class EArrangeScope : uint8
    {
        SelectedOrAll,
        ConnectedComponentOfSelection,
    };

    static void ArrangeGraphPanel(const TSharedPtr<SGraphPanel>& GraphPanel, EArrangeScope Scope, const TSet<UEdGraphNode*>* ExplicitSeeds = nullptr);

    static void GatherConnectedComponent(const TSet<UEdGraphNode*>& Seeds, TSet<UEdGraphNode*>& OutComponent);

    static TSharedPtr<SGraphPanel> FindActiveGraphPanel();
    static TSharedPtr<SGraphPanel> FindGraphPanelFromFocus();
    static TSharedPtr<SGraphPanel> FindGraphPanelInContext(const FToolMenuContext& Context);
    static TSharedPtr<SGraphPanel> FindGraphPanelRecursive(const TSharedRef<SWidget>& Widget);
};
