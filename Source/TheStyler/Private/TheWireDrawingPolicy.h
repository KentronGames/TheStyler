#pragma once

#include "CoreMinimal.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "EdGraphUtilities.h"

/** Installs the Manhattan wire policy for Blueprint (K2) graphs; other graphs keep the engine default. */
struct FTheWireConnectionFactory : public FGraphPanelPinConnectionFactory
{
    virtual ~FTheWireConnectionFactory() override = default;

    virtual FConnectionDrawingPolicy* CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
        const override;
};

/**
 * Right-angle (Manhattan) wire style with rounded corners for Blueprint exec wires.
 * Data wires keep the engine default spline. All wires are drawn a little thicker.
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

    // Flow dots animated along the exec wire's path, and a direction arrowhead near the input pin.
    void DrawExecBubbles(int32 LayerId, TArrayView<const FVector2f> Path, float WireThickness, const FLinearColor& Color);
    void DrawDirectionArrow(int32 LayerId, const FVector2f& End, const FVector2f& EndDirection, const FLinearColor& Color);

    // Track the drawn path's closest point to the mouse so wire hover/selection keeps working.
    void AccumulateClosestPoint(const FVector2f& A, const FVector2f& B);

    float ClosestDistanceSquared = 0.0f;
    FVector2f ClosestPoint = FVector2f::ZeroVector;
};
