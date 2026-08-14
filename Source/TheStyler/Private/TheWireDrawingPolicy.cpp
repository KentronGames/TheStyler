// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheWireDrawingPolicy.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "GraphEditorSettings.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

#include "TheStylerSettings.h"

namespace
{
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
    if(!GetDefault<UTheStylerViewSettings>()->bEnableWireStyling)
    {
        return nullptr;
    }

    const bool bTheGraph = Schema && GetDefault<UTheStylerSettings>()->ExtraWireStylingSchemas.Contains(Schema->GetClass()->GetName());
    if(Schema && (Schema->IsA(UEdGraphSchema_K2::StaticClass()) || bTheGraph))
    {
        return new FTheWireConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
    }
    return nullptr;
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
    const auto& StylerSettings = *GetDefault<UTheStylerViewSettings>();

    FConnectionParams StyledParams = Params;
    StyledParams.WireThickness *= StylerSettings.WireThicknessScale;

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

    const bool bRestyled = bExecWire || StylerSettings.bManhattanDataWires;
    const ETheWireStyle WireStyle = StylerSettings.WireStyle;
    if(!bRestyled || (WireStyle != ETheWireStyle::Straight && End.X <= Start.X))
    {
        FKismetConnectionDrawingPolicy::DrawConnection(LayerId, Start, End, StyledParams);
        return;
    }

    if(bExecWire && StylerSettings.bOverrideWireColor)
    {
        StyledParams.WireColor = StylerSettings.WireColor;
        StyledParams.WireColor.A *= DimFactor;
    }

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
        DrawStraightWire(LayerId, Start, End, StyledParams);
    }
    else
    {
        const float DeltaY = FMath::Abs(End.Y - Start.Y);
        float MidX = (Start.X + End.X) * 0.5f;

        const float CorridorStep = StylerSettings.WireCorridorSpacing * ZoomFactor;
        float CorridorShift = 0.0f;
        if(CorridorStep > KINDA_SMALL_NUMBER)
        {
            const int32 Bucket = FMath::RoundToInt(MidX / CorridorStep);
            int32& SlotCounter = CorridorSlots.FindOrAdd(Bucket);
            const int32 Slot = SlotCounter++;
            if(Slot > 0)
            {
                const int32 Ring = (Slot + 1) / 2;
                CorridorShift = (Slot % 2 == 1 ? 1.0f : -1.0f) * Ring * CorridorStep;
            }
        }

        TArray<FVector2f, TInlineAllocator<4>> Points;
        Points.Add(Start);
        if(WireStyle == ETheWireStyle::Metro45 && End.X - Start.X > DeltaY && DeltaY > KINDA_SMALL_NUMBER)
        {
            const float BaseLead = (End.X - Start.X - DeltaY) * 0.5f;
            const float Lead = FMath::Clamp(BaseLead + CorridorShift, 0.0f, End.X - Start.X - DeltaY);
            Points.Add(FVector2f(Start.X + Lead, Start.Y));
            Points.Add(FVector2f(Start.X + Lead + DeltaY, End.Y));
        }
        else
        {
            MidX = FMath::Clamp(MidX + CorridorShift, FMath::Min(Start.X, End.X), FMath::Max(Start.X, End.X));
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
            AccumulateClosestPoint(Entry, Exit);

            Cursor = Exit;
        }
        DrawStraightWire(LayerId, Cursor, Points.Last(), StyledParams);
    }

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
