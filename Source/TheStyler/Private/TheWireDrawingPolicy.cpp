#include "TheWireDrawingPolicy.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "GraphEditorSettings.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

#include "TheStylerSettings.h"

namespace
{
// Cubic tangent length factor for an ~circular 90-degree corner.
const float CornerTangentFactor = 4.0f * (FMath::Sqrt(2.0f) - 1.0f);

bool IsExecPin(const UEdGraphPin* Pin)
{
    return Pin != nullptr && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}

}

FConnectionDrawingPolicy* FTheWireConnectionFactory::CreateConnectionPolicy(const UEdGraphSchema* Schema,
    int32 InBackLayerID,
    int32 InFrontLayerID,
    float InZoomFactor,
    const FSlateRect& InClippingRect,
    FSlateWindowElementList& InDrawElements,
    UEdGraph* InGraphObj) const
{
    if(!GetDefault<UTheStylerSettings>()->bEnableWireStyling)
    {
        return nullptr; // styling disabled -> engine default policy for every graph
    }

    // Blueprint (K2) graphs, and our dialogue/quest graphs — their nodes use PC_Exec pins laid out
    // horizontally, so the same exec-Manhattan path applies. Matched by schema class name to keep
    // this plugin decoupled from the project's editor module.
    const bool bTheGraph = Schema && (Schema->GetClass()->GetFName() == TEXT("TheDialogueGraphSchema") || Schema->GetClass()->GetFName() == TEXT("TheQuestGraphSchema"));
    if(Schema && (Schema->IsA(UEdGraphSchema_K2::StaticClass()) || bTheGraph))
    {
        return new FTheWireConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
    }
    return nullptr; // other graphs fall back to the engine default policy
}

void FTheWireConnectionDrawingPolicy::DrawStraightWire(int32 LayerId, const FVector2f& A, const FVector2f& B, const FConnectionParams& Params)
{
    if(FVector2f::DistSquared(A, B) < KINDA_SMALL_NUMBER)
    {
        return;
    }
    FSlateDrawElement::MakeDrawSpaceSpline(DrawElementsList, LayerId, A, FVector2f::ZeroVector, B, FVector2f::ZeroVector, Params.WireThickness, ESlateDrawEffect::None, Params.WireColor);
    AccumulateClosestPoint(A, B);
}

void FTheWireConnectionDrawingPolicy::AccumulateClosestPoint(const FVector2f& A, const FVector2f& B)
{
    const FVector2f Mouse = AbsoluteMousePosition;
    const FVector2D Closest = FMath::ClosestPointOnSegment2D(FVector2D(Mouse.X, Mouse.Y), FVector2D(A.X, A.Y), FVector2D(B.X, B.Y));
    const FVector2f ClosestF(static_cast<float>(Closest.X), static_cast<float>(Closest.Y));
    const float DistSquared = FVector2f::DistSquared(Mouse, ClosestF);
    if(DistSquared < ClosestDistanceSquared)
    {
        ClosestDistanceSquared = DistSquared;
        ClosestPoint = ClosestF;
    }
}

void FTheWireConnectionDrawingPolicy::DrawConnection(int32 LayerId, const FVector2f& Start, const FVector2f& End, const FConnectionParams& Params)
{
    const auto& StylerSettings = *GetDefault<UTheStylerSettings>();

    // Fatten every wire a little for readability (data wires included — the base policy draws them).
    FConnectionParams StyledParams = Params;
    StyledParams.WireThickness *= StylerSettings.WireThicknessScale;

    // Focus/Dim: when nodes are selected, fade the wires that don't touch the selection so the selected
    // node's connections stand out. SelectedGraphNodes is filled by the graph panel each paint.
    float DimFactor = 1.0f;
    if(StylerSettings.bFocusDimOnSelection && SelectedGraphNodes.Num() > 0)
    {
        const auto Node1 = StyledParams.AssociatedPin1 ? StyledParams.AssociatedPin1->GetOwningNodeUnchecked() : nullptr;
        const auto Node2 = StyledParams.AssociatedPin2 ? StyledParams.AssociatedPin2->GetOwningNodeUnchecked() : nullptr;
        const bool bTouchesSelection = (Node1 && SelectedGraphNodes.Contains(Node1)) || (Node2 && SelectedGraphNodes.Contains(Node2));
        if(!bTouchesSelection)
        {
            DimFactor = StylerSettings.FocusDimOpacity;
        }
    }
    StyledParams.WireColor.A *= DimFactor;

    const bool bExecWire = IsExecPin(StyledParams.AssociatedPin1) || IsExecPin(StyledParams.AssociatedPin2);

    // Exec wires always get the restyle; data wires (usually many and crossing) only when the user
    // opts in, and even then they keep their pin-type colour. Backward wires fall back to the spline
    // for the elbowed styles — a mid-X bend would route back over the source node; straight lines
    // have no such problem and stay restyled in any direction.
    const bool bRestyled = bExecWire || StylerSettings.bManhattanDataWires;
    const ETheWireStyle WireStyle = StylerSettings.WireStyle;
    if(!bRestyled || (WireStyle != ETheWireStyle::Straight && End.X <= Start.X))
    {
        FKismetConnectionDrawingPolicy::DrawConnection(LayerId, Start, End, StyledParams);
        return;
    }

    // Optional fixed exec-wire colour; otherwise keep the pin-type colour.
    if(bExecWire && StylerSettings.bOverrideWireColor)
    {
        StyledParams.WireColor = StylerSettings.WireColor;
        StyledParams.WireColor.A *= DimFactor; // the override replaced the alpha; re-apply the focus fade
    }

    // Outcome pins (subcategory-marked by the project's graph editors: Success/True, Failed/False)
    // pass their green/red onto the wire — engine exec wires ignore the pin color, so the
    // inheritance has to be explicit here. Keyed by subcategory to stay decoupled; K2 graphs never
    // mark pins, so Blueprints are unaffected.
    if(bExecWire && StyledParams.AssociatedPin1)
    {
        const FName SourceSubCategory = StyledParams.AssociatedPin1->PinType.PinSubCategory;
        if(SourceSubCategory == TEXT("Success") || SourceSubCategory == TEXT("True"))
        {
            StyledParams.WireColor = FLinearColor(0.2f, 0.8f, 0.2f, StyledParams.WireColor.A);
        }
        else if(SourceSubCategory == TEXT("Failed") || SourceSubCategory == TEXT("False"))
        {
            StyledParams.WireColor = FLinearColor(0.8f, 0.2f, 0.2f, StyledParams.WireColor.A);
        }
    }

    ClosestDistanceSquared = FLT_MAX;

    if(WireStyle == ETheWireStyle::Straight || FVector2f::Distance(Start, End) < StylerSettings.MinManhattanDistance * ZoomFactor)
    {
        // Straight style — or a link too short for an elbow to look good in the other styles.
        DrawStraightWire(LayerId, Start, End, StyledParams);
    }
    else
    {
        // Elbowed polyline. Manhattan: horizontal -> vertical at the mid X -> horizontal.
        // Metro 45: equal horizontal leads joined by an exact 45-degree diagonal; a link too steep
        // for the diagonal to fit degrades to the Manhattan bend.
        const float DeltaY = FMath::Abs(End.Y - Start.Y);
        const float MidX = (Start.X + End.X) * 0.5f;

        TArray<FVector2f, TInlineAllocator<4>> Points;
        Points.Add(Start);
        if(WireStyle == ETheWireStyle::Metro45 && End.X - Start.X > DeltaY && DeltaY > KINDA_SMALL_NUMBER)
        {
            const float Lead = (End.X - Start.X - DeltaY) * 0.5f;
            Points.Add(FVector2f(Start.X + Lead, Start.Y));
            Points.Add(FVector2f(Start.X + Lead + DeltaY, End.Y));
        }
        else
        {
            Points.Add(FVector2f(MidX, Start.Y));
            Points.Add(FVector2f(MidX, End.Y));
        }
        Points.Add(End);

        const float Radius = StylerSettings.CornerRadius * ZoomFactor;

        FVector2f Cursor = Points[0];
        for(int32 Ndx = 1; Ndx < Points.Num() - 1; ++Ndx)
        {
            const FVector2f Vertex = Points[Ndx];
            const FVector2f InDir = (Vertex - Points[Ndx - 1]).GetSafeNormal();
            const FVector2f OutDir = (Points[Ndx + 1] - Vertex).GetSafeNormal();

            // Collinear or degenerate vertex: no corner to round, keep going straight.
            if(InDir.IsNearlyZero() || OutDir.IsNearlyZero() || FVector2f::DistSquared(InDir, OutDir) < KINDA_SMALL_NUMBER)
            {
                continue;
            }

            const float SegIn = FVector2f::Distance(Points[Ndx - 1], Vertex);
            const float SegOut = FVector2f::Distance(Vertex, Points[Ndx + 1]);
            const float CornerR = FMath::Min3(Radius, SegIn * 0.5f, SegOut * 0.5f);

            const FVector2f Entry = Vertex - InDir * CornerR;
            const FVector2f Exit = Vertex + OutDir * CornerR;

            DrawStraightWire(LayerId, Cursor, Entry, StyledParams);

            const float Tangent = CornerR * CornerTangentFactor;
            FSlateDrawElement::MakeDrawSpaceSpline(DrawElementsList, LayerId, Entry, InDir * Tangent, Exit, OutDir * Tangent, StyledParams.WireThickness, ESlateDrawEffect::None, StyledParams.WireColor);
            AccumulateClosestPoint(Entry, Exit); // approximate the arc by its chord for hover

            Cursor = Exit;
        }
        DrawStraightWire(LayerId, Cursor, Points.Last(), StyledParams);
    }

    // Preserve wire hover/selection: report the closest point on the drawn path (mirrors the base policy).
    if(Settings->bTreatSplinesLikePins)
    {
        const float ThresholdSquared = FMath::Square(Settings->SplineHoverTolerance + StyledParams.WireThickness * 0.5f);
        if(ClosestDistanceSquared < ThresholdSquared && ClosestDistanceSquared < SplineOverlapResult.GetDistanceSquared())
        {
            const float DistToPin1 = (StyledParams.AssociatedPin1 != nullptr) ? (Start - ClosestPoint).SizeSquared() : FLT_MAX;
            const float DistToPin2 = (StyledParams.AssociatedPin2 != nullptr) ? (End - ClosestPoint).SizeSquared() : FLT_MAX;
            SplineOverlapResult = FGraphSplineOverlapResult(StyledParams.AssociatedPin1, StyledParams.AssociatedPin2, ClosestDistanceSquared, DistToPin1, DistToPin2, false);
        }
    }
}
