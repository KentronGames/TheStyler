// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "EdGraphUtilities.h"

/** Installs the Manhattan wire policy for Blueprint (K2) and dialogue graphs; other graphs keep the engine default. */
struct FTheWireConnectionFactory : public FGraphPanelPinConnectionFactory
{
    virtual ~FTheWireConnectionFactory() override = default;

    virtual FConnectionDrawingPolicy* CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
        const override;
};

/**
 * Restyled exec wires — Manhattan elbows, Metro 45-degree diagonals, or straight lines
 * (ETheWireStyle). Data wires keep the engine default spline. All wires are drawn a little thicker.
 */
class FTheWireConnectionDrawingPolicy : public FKismetConnectionDrawingPolicy
{
public:
    FTheWireConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
        : FKismetConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj)
    {
    }

    virtual void DrawConnection(int32 LayerId, const FVector2f& Start, const FVector2f& End, const FConnectionParams& Params) override;

private:
    void DrawStraightWire(int32 LayerId, const FVector2f& A, const FVector2f& B, const FConnectionParams& Params);

    // Track the drawn path's closest point to the mouse so wire hover/selection keeps working.
    void AccumulateClosestPoint(const FVector2f& A, const FVector2f& B);

    float ClosestDistanceSquared = 0.0f;
    FVector2f ClosestPoint = FVector2f::ZeroVector;

    // Offset-stacking of parallel corridors: wires whose vertical segment (Manhattan) or diagonal
    // (Metro) lands in the same X bucket this paint take successive slots (0, +1, -1, +2, ...) and
    // are nudged apart by WireCorridorSpacing. The policy lives for exactly one panel paint, so the
    // map resets every frame; the panel's stable draw order keeps slot assignment flicker-free.
    TMap<int32, int32> CorridorSlots;
};
