// (c) 2026 Kentron Cowboys. All rights reserved.

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
#include "Toolkits/IToolkitHost.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"

#include "TheStylerModule.h"
#include "TheStylerSettings.h"

#define LOCTEXT_NAMESPACE "TheStyler"

namespace
{
constexpr double DefaultNodeWidth = 340.0;
constexpr double DefaultNodeHeight = 150.0;

struct FArrangeNode
{
    UEdGraphNode* Graph = nullptr;
    FVector2D Size = FVector2D(DefaultNodeWidth, DefaultNodeHeight);
    TArray<int32> Preds;
    TArray<int32> Succs;
    int32 Layer = 0;
    int32 Order = 0;
    double BaryKey = 0.0;
    double PosX = 0.0;
    double PosY = 0.0;
};

struct FSelNode
{
    UEdGraphNode* Node = nullptr;
    FVector2D Size = FVector2D(DefaultNodeWidth, DefaultNodeHeight);
};

TArray<FSelNode> GatherSelectedNodes(const TSharedPtr<SGraphPanel>& Panel)
{
    TArray<FSelNode> Out;
    UEdGraph* const Graph = Panel->GetGraphObj();
    if(!IsValid(Graph))
    {
        return Out;
    }
    for(UEdGraphNode* Node : Graph->Nodes)
    {
        if(!IsValid(Node) || Node->IsA<UEdGraphNode_Comment>() || !Panel->SelectionManager.IsNodeSelected(Node))
        {
            continue;
        }
        FSelNode Sel;
        Sel.Node = Node;
        if(const auto NodeWidget = Panel->GetNodeWidgetFromGuid(Node->NodeGuid))
        {
            const FVector2D Desired = NodeWidget->GetDesiredSize();
            if(Desired.X > 1.0 && Desired.Y > 1.0)
            {
                Sel.Size = Desired;
            }
        }
        Out.Add(Sel);
    }
    return Out;
}
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
    if(const auto Focused = FindGraphPanelFromFocus())
    {
        return Focused;
    }

    if(const auto ActiveTab = FGlobalTabmanager::Get()->GetActiveTab())
    {
        if(const auto Panel = FindGraphPanelRecursive(ActiveTab->GetContent()))
        {
            return Panel;
        }
    }

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
    if(const auto TabManager = Toolkit->GetTabManager())
    {
        if(const auto OwnerTab = TabManager->GetOwnerTab())
        {
            if(const auto Panel = FindGraphPanelRecursive(OwnerTab->GetContent()))
            {
                return Panel;
            }
        }
    }

    if(const auto Panel = FindGraphPanelRecursive(Toolkit->GetToolkitHost()->GetParentWidget()))
    {
        return Panel;
    }

    return nullptr;
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

TSharedPtr<SGraphPanel> FTheGraphArranger::GetActiveGraphPanel()
{
    return FindActiveGraphPanel();
}

void FTheGraphArranger::FormatComponentInActivePanel(const TSet<UEdGraphNode*>& Seeds)
{
    if(Seeds.Num() == 0)
    {
        return;
    }
    ArrangeGraphPanel(FindActiveGraphPanel(), EArrangeScope::ConnectedComponentOfSelection, &Seeds);
}

void FTheGraphArranger::ArrangeGraphFromContext(const FToolMenuContext& Context)
{
    auto Panel = FindGraphPanelInContext(Context);
    if(!Panel.IsValid())
    {
        Panel = FindActiveGraphPanel();
    }
    ArrangeGraphPanel(Panel, EArrangeScope::SelectedOrAll);
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

void FTheGraphArranger::ArrangeGraphPanel(const TSharedPtr<SGraphPanel>& GraphPanel, EArrangeScope Scope, const TSet<UEdGraphNode*>* ExplicitSeeds)
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

    const auto& Settings = *GetDefault<UTheStylerSettings>();
    const double SpacingX = Settings.NodeSpacingX;
    const double SpacingY = Settings.NodeSpacingY;
    const int32 NumOrderingPasses = Settings.NodeOrderingPasses;

    const bool bHasSelection = GraphPanel->SelectionManager.SelectedNodes.Num() > 0;

    TSet<UEdGraphNode*> Component;
    if(Scope == EArrangeScope::ConnectedComponentOfSelection)
    {
        TSet<UEdGraphNode*> Seeds;
        if(ExplicitSeeds)
        {
            for(UEdGraphNode* Seed : *ExplicitSeeds)
            {
                if(IsValid(Seed) && !Seed->IsA<UEdGraphNode_Comment>() && Seed->GetGraph() == Graph)
                {
                    Seeds.Add(Seed);
                }
            }
        }
        else
        {
            for(UEdGraphNode* GraphNode : Graph->Nodes)
            {
                if(IsValid(GraphNode) && !GraphNode->IsA<UEdGraphNode_Comment>() && GraphPanel->SelectionManager.IsNodeSelected(GraphNode))
                {
                    Seeds.Add(GraphNode);
                }
            }
        }
        if(Seeds.Num() == 0)
        {
            UE_LOG(LogTheStyler, Verbose, TEXT("Format: no seed nodes."));
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

    TArray<uint8> State;
    State.Init(0, Nodes.Num());

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
                    State[PredNdx] = 1;
                    Stack.Push({PredNdx, 0});
                    bDescended = true;
                    break;
                }
                if(State[PredNdx] == 2)
                {
                    Nodes[NodeNdx].Layer = FMath::Max(Nodes[NodeNdx].Layer, Nodes[PredNdx].Layer + 1);
                }
                ++Frame.NextPred;
            }

            if(bDescended)
            {
                continue;
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

#pragma region Align & Distribute

void FTheGraphArranger::AlignActiveSelection(ETheAlign Mode)
{
    const auto Panel = FindActiveGraphPanel();
    if(!Panel.IsValid())
    {
        UE_LOG(LogTheStyler, Warning, TEXT("Align: no active graph panel found."));
        return;
    }
    UEdGraph* const Graph = Panel->GetGraphObj();
    if(!IsValid(Graph))
    {
        return;
    }

    TArray<FSelNode> Selection = GatherSelectedNodes(Panel);
    if(Selection.Num() < 2)
    {
        UE_LOG(LogTheStyler, Verbose, TEXT("Align: select two or more nodes."));
        return;
    }

    double MinX = TNumericLimits<double>::Max();
    double MinY = TNumericLimits<double>::Max();
    double MaxRight = TNumericLimits<double>::Lowest();
    double MaxBottom = TNumericLimits<double>::Lowest();
    for(const FSelNode& Sel : Selection)
    {
        MinX = FMath::Min(MinX, static_cast<double>(Sel.Node->NodePosX));
        MinY = FMath::Min(MinY, static_cast<double>(Sel.Node->NodePosY));
        MaxRight = FMath::Max(MaxRight, Sel.Node->NodePosX + Sel.Size.X);
        MaxBottom = FMath::Max(MaxBottom, Sel.Node->NodePosY + Sel.Size.Y);
    }
    const double CenterX = (MinX + MaxRight) * 0.5;
    const double CenterY = (MinY + MaxBottom) * 0.5;

    const FScopedTransaction Transaction(LOCTEXT("AlignNodesTransaction", "Align Nodes"));
    Graph->Modify();
    for(const FSelNode& Sel : Selection)
    {
        Sel.Node->Modify();
        switch(Mode)
        {
            case ETheAlign::Left:
                Sel.Node->NodePosX = FMath::RoundToInt32(MinX);
                break;
            case ETheAlign::Right:
                Sel.Node->NodePosX = FMath::RoundToInt32(MaxRight - Sel.Size.X);
                break;
            case ETheAlign::Top:
                Sel.Node->NodePosY = FMath::RoundToInt32(MinY);
                break;
            case ETheAlign::Bottom:
                Sel.Node->NodePosY = FMath::RoundToInt32(MaxBottom - Sel.Size.Y);
                break;
            case ETheAlign::CenterX:
                Sel.Node->NodePosX = FMath::RoundToInt32(CenterX - Sel.Size.X * 0.5);
                break;
            case ETheAlign::CenterY:
                Sel.Node->NodePosY = FMath::RoundToInt32(CenterY - Sel.Size.Y * 0.5);
                break;
        }
    }
    Graph->NotifyGraphChanged();
}

void FTheGraphArranger::DistributeActiveSelection(ETheDistribute Axis)
{
    const auto Panel = FindActiveGraphPanel();
    if(!Panel.IsValid())
    {
        UE_LOG(LogTheStyler, Warning, TEXT("Distribute: no active graph panel found."));
        return;
    }
    UEdGraph* const Graph = Panel->GetGraphObj();
    if(!IsValid(Graph))
    {
        return;
    }

    TArray<FSelNode> Selection = GatherSelectedNodes(Panel);
    if(Selection.Num() < 3)
    {
        UE_LOG(LogTheStyler, Verbose, TEXT("Distribute: select three or more nodes."));
        return;
    }

    const bool bHorizontal = Axis == ETheDistribute::Horizontal;
    Selection.Sort([bHorizontal](const FSelNode& A, const FSelNode& B) { return bHorizontal ? A.Node->NodePosX < B.Node->NodePosX : A.Node->NodePosY < B.Node->NodePosY; });

    const FSelNode& First = Selection[0];
    const FSelNode& Last = Selection.Last();
    const double SpanStart = bHorizontal ? First.Node->NodePosX : First.Node->NodePosY;
    const double SpanEnd = bHorizontal ? (Last.Node->NodePosX + Last.Size.X) : (Last.Node->NodePosY + Last.Size.Y);
    double TotalSize = 0.0;
    for(const FSelNode& Sel : Selection)
    {
        TotalSize += bHorizontal ? Sel.Size.X : Sel.Size.Y;
    }
    const double Gap = (SpanEnd - SpanStart - TotalSize) / (Selection.Num() - 1);

    const FScopedTransaction Transaction(LOCTEXT("DistributeNodesTransaction", "Distribute Nodes"));
    Graph->Modify();
    double Cursor = SpanStart;
    for(const FSelNode& Sel : Selection)
    {
        Sel.Node->Modify();
        if(bHorizontal)
        {
            Sel.Node->NodePosX = FMath::RoundToInt32(Cursor);
            Cursor += Sel.Size.X + Gap;
        }
        else
        {
            Sel.Node->NodePosY = FMath::RoundToInt32(Cursor);
            Cursor += Sel.Size.Y + Gap;
        }
    }
    Graph->NotifyGraphChanged();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
