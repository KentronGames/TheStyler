#include "TheWireDrawingPolicy.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "GraphEditorSettings.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

namespace
{
// Layout tuning (graph units, pre-zoom). Kept as constants — the styler is intentionally config-free.
constexpr float CornerRadius = 12.0f; // rounded-corner radius at each Manhattan bend
constexpr float MinManhattanDistance = 24.0f; // below this, draw a plain straight line
constexpr float WireThicknessScale = 1.6f; // fatten every wire a touch for readability

// Cubic tangent length factor for an ~circular 90-degree corner.
const float CornerTangentFactor = 4.0f * (FMath::Sqrt(2.0f) - 1.0f);

bool IsExecPin(const UEdGraphPin* Pin)
{
    return Pin != nullptr && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}

// Point at arc-length Distance along a polyline (used to place flow bubbles).
FVector2f PointAlongPath(TArrayView<const FVector2f> Path, float Distance)
{
    for(int32 Ndx = 1; Ndx < Path.Num(); ++Ndx)
    {
        const float SegLength = FVector2f::Distance(Path[Ndx - 1], Path[Ndx]);
        if(Distance <= SegLength)
        {
            const FVector2f Direction = (Path[Ndx] - Path[Ndx - 1]).GetSafeNormal();
            return Path[Ndx - 1] + Direction * Distance;
        }
        Distance -= SegLength;
    }
    return Path.Last();
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
    // Blueprint (K2) graphs, and our dialogue graph — its nodes use PC_Exec pins laid out
    // horizontally, so the same exec-Manhattan path applies. Matched by schema class name to keep
    // this plugin decoupled from the project's editor module.
    const bool bDialogueGraph = Schema && Schema->GetClass()->GetFName() == TEXT("TheDialogueGraphSchema");
    if(Schema && (Schema->IsA(UEdGraphSchema_K2::StaticClass()) || bDialogueGraph))
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

void FTheWireConnectionDrawingPolicy::DrawExecBubbles(int32 LayerId, TArrayView<const FVector2f> Path, float WireThickness, const FLinearColor& Color)
{
    if(BubbleImage == nullptr || Path.Num() < 2)
    {
        return;
    }

    float TotalLength = 0.0f;
    for(int32 Ndx = 1; Ndx < Path.Num(); ++Ndx)
    {
        TotalLength += FVector2f::Distance(Path[Ndx - 1], Path[Ndx]);
    }
    if(TotalLength <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    // Match the engine's exec-bubble cadence so restyled wires animate like native ones.
    const float BubbleSpacing = 64.0f * ZoomFactor;
    const float BubbleSpeed = 192.0f * ZoomFactor;
    const FVector2f BubbleSize = BubbleImage->ImageSize * ZoomFactor * 0.2f * WireThickness;

    const float Time = static_cast<float>(FPlatformTime::Seconds() - GStartTime);
    const float StartOffset = FMath::Fmod(Time * BubbleSpeed, BubbleSpacing);
    const int32 NumBubbles = FMath::CeilToInt(TotalLength / BubbleSpacing);
    for(int32 BubbleNdx = 0; BubbleNdx < NumBubbles; ++BubbleNdx)
    {
        const float Distance = BubbleNdx * BubbleSpacing + StartOffset;
        if(Distance >= TotalLength)
        {
            continue;
        }
        const FVector2f BubblePos = PointAlongPath(Path, Distance) - BubbleSize * 0.5f;
        FSlateDrawElement::MakeBox(DrawElementsList, LayerId, FPaintGeometry(BubblePos, BubbleSize, ZoomFactor), BubbleImage, ESlateDrawEffect::None, Color);
    }
}

void FTheWireConnectionDrawingPolicy::DrawDirectionArrow(int32 LayerId, const FVector2f& End, const FVector2f& EndDirection, const FLinearColor& Color)
{
    const FSlateBrush* Arrow = FAppStyle::GetBrush(TEXT("Graph.Arrow"));
    if(Arrow == nullptr)
    {
        return;
    }
    const FVector2f ArrowSize = Arrow->ImageSize * ZoomFactor;
    const float AngleInRadians = FMath::Atan2(EndDirection.Y, EndDirection.X);
    // Sit the arrowhead just short of the input pin, centred on the wire.
    const FVector2f Centre = End - EndDirection * (ArrowSize.X * 0.5f);
    const FVector2f DrawPos = Centre - ArrowSize * 0.5f;
    FSlateDrawElement::MakeRotatedBox(DrawElementsList, LayerId, FPaintGeometry(DrawPos, ArrowSize, ZoomFactor), Arrow, ESlateDrawEffect::None, AngleInRadians, TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement, Color);
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
    // Fatten every wire a little for readability (data wires included — the base policy draws them).
    FConnectionParams StyledParams = Params;
    StyledParams.WireThickness *= WireThicknessScale;

    const FVector2f EndDirection = (StyledParams.EndDirection == EGPD_Input) ? FVector2f(1.0f, 0.0f) : FVector2f(-1.0f, 0.0f);
    const bool bExecWire = IsExecPin(StyledParams.AssociatedPin1) || IsExecPin(StyledParams.AssociatedPin2);

    // Only exec wires get the Manhattan restyle. Data wires (usually many and crossing) keep the
    // engine's default spline to avoid clutter. Backward exec wires also fall back, since a mid-X
    // elbow would route back over the source node.
    if(!bExecWire || End.X <= Start.X)
    {
        FKismetConnectionDrawingPolicy::DrawConnection(LayerId, Start, End, StyledParams);
        return;
    }

    ClosestDistanceSquared = FLT_MAX;

    if(FVector2f::Distance(Start, End) < MinManhattanDistance * ZoomFactor)
    {
        // Short link: a tiny elbow looks bad — draw it straight.
        DrawStraightWire(LayerId, Start, End, StyledParams);
    }
    else
    {
        // Manhattan path: horizontal out of Start -> vertical -> horizontal into End, bending at the mid X.
        const float MidX = (Start.X + End.X) * 0.5f;
        TArray<FVector2f, TInlineAllocator<4>> Points;
        Points.Add(Start);
        Points.Add(FVector2f(MidX, Start.Y));
        Points.Add(FVector2f(MidX, End.Y));
        Points.Add(End);

        const float Radius = CornerRadius * ZoomFactor;

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

        // Animated flow dots along the exec path.
        DrawExecBubbles(LayerId, Points, StyledParams.WireThickness, StyledParams.WireColor);
    }

    // Direction arrowhead near the input pin.
    DrawDirectionArrow(LayerId, End, EndDirection, StyledParams.WireColor);

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
