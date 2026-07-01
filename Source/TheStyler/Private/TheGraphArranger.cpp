#include "TheGraphArranger.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "ScopedTransaction.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "ToolMenuContext.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"

#include "TheStylerModule.h"
#include "TheStylerSettings.h"

#define LOCTEXT_NAMESPACE "TheStyler"

namespace
{
// Default node extent used when a node's widget size is not available yet.
constexpr double DefaultNodeWidth = 200.0;
constexpr double DefaultNodeHeight = 128.0;

struct FArrangeNode
{
    UEdGraphNode* Graph = nullptr;
    FVector2D Size = FVector2D(DefaultNodeWidth, DefaultNodeHeight);
    TArray<int32> Preds; // nodes feeding into this one (output -> input)
    TArray<int32> Succs; // nodes this one feeds into
    int32 Layer = 0;
    int32 Order = 0; // position within its layer
    double BaryKey = 0.0;
    double PosX = 0.0;
    double PosY = 0.0;
};
}

#pragma region Panel discovery

TSharedPtr<SGraphPanel> FTheGraphArranger::FindGraphPanelRecursive(const TSharedRef<SWidget>& Widget)
{
    if(Widget->GetType() == TEXT("SGraphPanel"))
    {
        return StaticCastSharedRef<SGraphPanel>(Widget);
    }

    FChildren* Children = Widget->GetChildren();
    if(Children)
    {
        for(int32 ChildNdx = 0; ChildNdx < Children->Num(); ++ChildNdx)
        {
            const auto Result = FindGraphPanelRecursive(Children->GetChildAt(ChildNdx));
            if(Result.IsValid())
            {
                return Result;
            }
        }
    }
    return nullptr;
}

TSharedPtr<SGraphPanel> FTheGraphArranger::FindGraphPanelFromFocus()
{
    if(!FSlateApplication::IsInitialized())
    {
        return nullptr;
    }

    // Walk up from the keyboard-focused widget. When the user is working inside a graph (e.g. the
    // Shift+Q hotkey) this pins down the exact panel and disambiguates between several open graphs.
    auto Widget = FSlateApplication::Get().GetKeyboardFocusedWidget();
    while(Widget.IsValid())
    {
        if(Widget->GetType() == TEXT("SGraphPanel"))
        {
            return StaticCastSharedRef<SGraphPanel>(Widget.ToSharedRef());
        }
        Widget = Widget->GetParentWidget();
    }
    return nullptr;
}

TSharedPtr<SGraphPanel> FTheGraphArranger::FindActiveGraphPanel()
{
    // 1) The graph under keyboard focus — exact when the user is inside a graph.
    if(const auto Focused = FindGraphPanelFromFocus())
    {
        return Focused;
    }

    // 2) The globally active tab's content — covers editors whose graph lives directly in the active
    //    document tab, e.g. the Blueprint event graph.
    if(const auto ActiveTab = FGlobalTabmanager::Get()->GetActiveTab())
    {
        if(const auto Panel = FindGraphPanelRecursive(ActiveTab->GetContent()))
        {
            return Panel;
        }
    }

    // 3) The active top-level window's whole widget tree — covers editors that dock the graph in a
    //    fixed minor tab GetActiveTab() does not return (the Material editor, our Dialogue editor).
    //    Only the foreground major tab's content is live in the tree, so this resolves to the visible
    //    graph rather than a backgrounded one.
    if(FSlateApplication::IsInitialized())
    {
        if(const auto ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow())
        {
            return FindGraphPanelRecursive(ActiveWindow.ToSharedRef());
        }
    }

    return nullptr;
}

TSharedPtr<SGraphPanel> FTheGraphArranger::FindGraphPanelInContext(const FToolMenuContext& Context)
{
    // Scope discovery to the asset editor whose toolbar hosts the button: the toolbar context carries
    // that editor's toolkit. Its owner tab's content spans every docked tab, so the graph is found no
    // matter which tab role hosts it — a Blueprint document tab, our Dialogue major tab, or the Material
    // editor's fixed Document tab (the global active-tab lookup only sees the latter two).
    const auto ToolkitContext = Context.FindContext<UAssetEditorToolkitMenuContext>();
    if(!ToolkitContext)
    {
        return nullptr;
    }
    const auto Toolkit = ToolkitContext->Toolkit.Pin();
    if(!Toolkit.IsValid())
    {
        return nullptr;
    }
    const auto TabManager = Toolkit->GetTabManager();
    if(!TabManager.IsValid())
    {
        return nullptr;
    }
    const auto OwnerTab = TabManager->GetOwnerTab();
    if(!OwnerTab.IsValid())
    {
        return nullptr;
    }
    return FindGraphPanelRecursive(OwnerTab->GetContent());
}

bool FTheGraphArranger::HasGraphInContext(const FToolMenuContext& Context)
{
    return FindGraphPanelInContext(Context).IsValid();
}

#pragma endregion

#pragma region Arrange

void FTheGraphArranger::ArrangeActiveGraph()
{
    ArrangeGraphPanel(FindActiveGraphPanel(), EArrangeScope::SelectedOrAll);
}

void FTheGraphArranger::FormatActiveSelection()
{
    ArrangeGraphPanel(FindActiveGraphPanel(), EArrangeScope::ConnectedComponentOfSelection);
}

void FTheGraphArranger::ArrangeGraphFromContext(const FToolMenuContext& Context)
{
    ArrangeGraphPanel(FindGraphPanelInContext(Context), EArrangeScope::SelectedOrAll);
}

void FTheGraphArranger::GatherConnectedComponent(const TSet<UEdGraphNode*>& Seeds, TSet<UEdGraphNode*>& OutComponent)
{
    OutComponent.Append(Seeds);

    TArray<UEdGraphNode*> Queue = Seeds.Array();
    while(Queue.Num() > 0)
    {
        const auto Node = Queue.Pop();
        for(UEdGraphPin* Pin : Node->Pins)
        {
            if(!Pin)
            {
                continue;
            }
            for(UEdGraphPin* Linked : Pin->LinkedTo)
            {
                const auto Other = Linked ? Linked->GetOwningNodeUnchecked() : nullptr;
                if(!Other || Other->IsA<UEdGraphNode_Comment>())
                {
                    continue;
                }
                bool bAlreadyInSet = false;
                OutComponent.Add(Other, &bAlreadyInSet);
                if(!bAlreadyInSet)
                {
                    Queue.Add(Other);
                }
            }
        }
    }
}

void FTheGraphArranger::ArrangeGraphPanel(const TSharedPtr<SGraphPanel>& GraphPanel, EArrangeScope Scope)
{
    if(!GraphPanel.IsValid())
    {
        UE_LOG(LogTheStyler, Warning, TEXT("Arrange Nodes: no active graph panel found."));
        return;
    }

    UEdGraph* Graph = GraphPanel->GetGraphObj();
    if(!IsValid(Graph))
    {
        UE_LOG(LogTheStyler, Verbose, TEXT("Arrange Nodes: active graph panel has no valid graph object."));
        return;
    }

    // Layout tuning is user-configurable (Project Settings -> Plugins -> The Styler).
    const auto& Settings = *GetDefault<UTheStylerSettings>();
    const double SpacingX = Settings.NodeSpacingX; // horizontal gap between layers (columns)
    const double SpacingY = Settings.NodeSpacingY; // vertical gap between stacked nodes in a column
    const int32 NumOrderingPasses = Settings.NodeOrderingPasses;

    // Collect the target set. Comment nodes are left untouched in this version.
    //   SelectedOrAll  : selected nodes, or all if nothing is selected.
    //   ConnectedComponentOfSelection (Format Node) : the wire-connected component(s) of the selection.
    const bool bHasSelection = GraphPanel->SelectionManager.SelectedNodes.Num() > 0;

    TSet<UEdGraphNode*> Component;
    if(Scope == EArrangeScope::ConnectedComponentOfSelection)
    {
        TSet<UEdGraphNode*> Seeds;
        for(UEdGraphNode* GraphNode : Graph->Nodes)
        {
            if(IsValid(GraphNode) && !GraphNode->IsA<UEdGraphNode_Comment>() && GraphPanel->SelectionManager.IsNodeSelected(GraphNode))
            {
                Seeds.Add(GraphNode);
            }
        }
        if(Seeds.Num() == 0)
        {
            UE_LOG(LogTheStyler, Verbose, TEXT("Format Node: select a node first."));
            return;
        }
        GatherConnectedComponent(Seeds, Component);
    }

    TArray<FArrangeNode> Nodes;
    TMap<UEdGraphNode*, int32> IndexMap;
    for(UEdGraphNode* GraphNode : Graph->Nodes)
    {
        if(!IsValid(GraphNode) || GraphNode->IsA<UEdGraphNode_Comment>())
        {
            continue;
        }
        if(Scope == EArrangeScope::ConnectedComponentOfSelection)
        {
            if(!Component.Contains(GraphNode))
            {
                continue;
            }
        }
        else if(bHasSelection && !GraphPanel->SelectionManager.IsNodeSelected(GraphNode))
        {
            continue;
        }

        FArrangeNode Entry;
        Entry.Graph = GraphNode;
        if(const auto NodeWidget = GraphPanel->GetNodeWidgetFromGuid(GraphNode->NodeGuid))
        {
            const FVector2D Desired = NodeWidget->GetDesiredSize();
            if(Desired.X > 1.0 && Desired.Y > 1.0)
            {
                Entry.Size = Desired;
            }
        }
        IndexMap.Add(GraphNode, Nodes.Num());
        Nodes.Add(MoveTemp(Entry));
    }

    if(Nodes.Num() < 2)
    {
        UE_LOG(LogTheStyler, Verbose, TEXT("Arrange Nodes: fewer than two arrangeable nodes; nothing to do."));
        return;
    }

    // Build directed edges from output pins to the input pins they connect to.
    for(int32 SrcNdx = 0; SrcNdx < Nodes.Num(); ++SrcNdx)
    {
        for(UEdGraphPin* Pin : Nodes[SrcNdx].Graph->Pins)
        {
            if(!Pin || Pin->Direction != EGPD_Output)
            {
                continue;
            }
            for(UEdGraphPin* Linked : Pin->LinkedTo)
            {
                if(!Linked)
                {
                    continue;
                }
                UEdGraphNode* Owner = Linked->GetOwningNodeUnchecked();
                if(!Owner)
                {
                    continue;
                }
                if(const int32* DstNdxPtr = IndexMap.Find(Owner))
                {
                    const int32 DstNdx = *DstNdxPtr;
                    if(DstNdx != SrcNdx)
                    {
                        Nodes[SrcNdx].Succs.AddUnique(DstNdx);
                        Nodes[DstNdx].Preds.AddUnique(SrcNdx);
                    }
                }
            }
        }
    }

    // Layer assignment: longest path from roots, skipping back edges to survive cycles.
    // Iterative DFS (explicit stack) so depth is heap-bounded — a long node chain must not
    // overflow the call stack. Node.Layer doubles as the running max and its final value.
    TArray<uint8> State;
    State.Init(0, Nodes.Num()); // 0 = unvisited, 1 = on stack, 2 = done

    struct FLayerFrame
    {
        int32 NodeNdx;
        int32 NextPred;
    };
    TArray<FLayerFrame> Stack;
    for(int32 RootNdx = 0; RootNdx < Nodes.Num(); ++RootNdx)
    {
        if(State[RootNdx] != 0)
        {
            continue;
        }
        State[RootNdx] = 1;
        Stack.Push({RootNdx, 0});

        while(Stack.Num() > 0)
        {
            FLayerFrame& Frame = Stack.Last();
            const int32 NodeNdx = Frame.NodeNdx;
            const TArray<int32>& Preds = Nodes[NodeNdx].Preds;

            bool bDescended = false;
            while(Frame.NextPred < Preds.Num())
            {
                const int32 PredNdx = Preds[Frame.NextPred];
                if(State[PredNdx] == 0)
                {
                    // Descend without advancing — revisit this pred (now done) on the way back up.
                    State[PredNdx] = 1;
                    Stack.Push({PredNdx, 0});
                    bDescended = true;
                    break;
                }
                if(State[PredNdx] == 2)
                {
                    Nodes[NodeNdx].Layer = FMath::Max(Nodes[NodeNdx].Layer, Nodes[PredNdx].Layer + 1);
                }
                // State == 1: back edge — skip.
                ++Frame.NextPred;
            }

            if(bDescended)
            {
                continue; // Frame reference is now stale after the push; re-fetch next loop.
            }
            State[NodeNdx] = 2;
            Stack.Pop();
        }
    }

    int32 MaxLayer = 0;
    for(const FArrangeNode& Node : Nodes)
    {
        MaxLayer = FMath::Max(MaxLayer, Node.Layer);
    }

    // Group node indices per layer, seeded with their original vertical order for stability.
    TArray<TArray<int32>> Layers;
    Layers.SetNum(MaxLayer + 1);
    for(int32 NodeNdx = 0; NodeNdx < Nodes.Num(); ++NodeNdx)
    {
        Layers[Nodes[NodeNdx].Layer].Add(NodeNdx);
    }
    for(TArray<int32>& Layer : Layers)
    {
        Layer.Sort(
            [&Nodes](int32 A, int32 B)
            {
                const UEdGraphNode* NodeA = Nodes[A].Graph;
                const UEdGraphNode* NodeB = Nodes[B].Graph;
                return NodeA->NodePosY != NodeB->NodePosY ? NodeA->NodePosY < NodeB->NodePosY : NodeA->NodePosX < NodeB->NodePosX;
            });
        for(int32 Pos = 0; Pos < Layer.Num(); ++Pos)
        {
            Nodes[Layer[Pos]].Order = Pos;
        }
    }

    // Crossing reduction: barycenter sweeps, alternating direction.
    const auto OrderLayerByNeighbors = [&Nodes](TArray<int32>& Layer, bool bUsePreds)
    {
        for(const int32 NodeNdx : Layer)
        {
            const TArray<int32>& Neighbors = bUsePreds ? Nodes[NodeNdx].Preds : Nodes[NodeNdx].Succs;
            if(Neighbors.Num() == 0)
            {
                Nodes[NodeNdx].BaryKey = static_cast<double>(Nodes[NodeNdx].Order);
                continue;
            }
            double Sum = 0.0;
            for(const int32 NeighborNdx : Neighbors)
            {
                Sum += Nodes[NeighborNdx].Order;
            }
            Nodes[NodeNdx].BaryKey = Sum / Neighbors.Num();
        }
        Layer.Sort([&Nodes](int32 A, int32 B) { return Nodes[A].BaryKey < Nodes[B].BaryKey; });
        for(int32 Pos = 0; Pos < Layer.Num(); ++Pos)
        {
            Nodes[Layer[Pos]].Order = Pos;
        }
    };
    for(int32 Pass = 0; Pass < NumOrderingPasses; ++Pass)
    {
        if(Pass % 2 == 0)
        {
            for(int32 LayerNdx = 1; LayerNdx <= MaxLayer; ++LayerNdx)
            {
                OrderLayerByNeighbors(Layers[LayerNdx], /*bUsePreds*/ true);
            }
        }
        else
        {
            for(int32 LayerNdx = MaxLayer - 1; LayerNdx >= 0; --LayerNdx)
            {
                OrderLayerByNeighbors(Layers[LayerNdx], /*bUsePreds*/ false);
            }
        }
    }

    // X per layer (cumulative widths), Y by stacking within a layer and centering the column.
    TArray<double> LayerWidth;
    LayerWidth.Init(0.0, MaxLayer + 1);
    for(const FArrangeNode& Node : Nodes)
    {
        LayerWidth[Node.Layer] = FMath::Max(LayerWidth[Node.Layer], Node.Size.X);
    }
    TArray<double> LayerX;
    LayerX.Init(0.0, MaxLayer + 1);
    for(int32 LayerNdx = 1; LayerNdx <= MaxLayer; ++LayerNdx)
    {
        LayerX[LayerNdx] = LayerX[LayerNdx - 1] + LayerWidth[LayerNdx - 1] + SpacingX;
    }

    for(int32 LayerNdx = 0; LayerNdx <= MaxLayer; ++LayerNdx)
    {
        double RunningY = 0.0;
        for(const int32 NodeNdx : Layers[LayerNdx])
        {
            Nodes[NodeNdx].PosX = LayerX[LayerNdx];
            Nodes[NodeNdx].PosY = RunningY;
            RunningY += Nodes[NodeNdx].Size.Y + SpacingY;
        }
        if(Layers[LayerNdx].Num() > 0)
        {
            const double ColumnHeight = RunningY - SpacingY;
            const double Shift = -ColumnHeight * 0.5;
            for(const int32 NodeNdx : Layers[LayerNdx])
            {
                Nodes[NodeNdx].PosY += Shift;
            }
        }
    }

    // Anchor the new layout to the original top-left so the graph does not jump on screen.
    double OrigMinX = TNumericLimits<double>::Max();
    double OrigMinY = TNumericLimits<double>::Max();
    double NewMinX = TNumericLimits<double>::Max();
    double NewMinY = TNumericLimits<double>::Max();
    for(const FArrangeNode& Node : Nodes)
    {
        OrigMinX = FMath::Min(OrigMinX, static_cast<double>(Node.Graph->NodePosX));
        OrigMinY = FMath::Min(OrigMinY, static_cast<double>(Node.Graph->NodePosY));
        NewMinX = FMath::Min(NewMinX, Node.PosX);
        NewMinY = FMath::Min(NewMinY, Node.PosY);
    }
    const double OffsetX = OrigMinX - NewMinX;
    const double OffsetY = OrigMinY - NewMinY;

    // Apply inside a single undoable transaction.
    const FScopedTransaction Transaction(LOCTEXT("ArrangeNodesTransaction", "Arrange Nodes"));
    Graph->Modify();
    for(const FArrangeNode& Node : Nodes)
    {
        Node.Graph->Modify();
        Node.Graph->NodePosX = FMath::RoundToInt32(Node.PosX + OffsetX);
        Node.Graph->NodePosY = FMath::RoundToInt32(Node.PosY + OffsetY);
    }
    Graph->NotifyGraphChanged();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
