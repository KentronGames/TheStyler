// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "EdGraphUtilities.h"

struct FTheWireConnectionFactory : public FGraphPanelPinConnectionFactory
{
    virtual ~FTheWireConnectionFactory() override = default;

    virtual FConnectionDrawingPolicy* CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
        const override;
};

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

    void AccumulateClosestPoint(const FVector2f& A, const FVector2f& B);

    float ClosestDistanceSquared = 0.0f;
    FVector2f ClosestPoint = FVector2f::ZeroVector;

    TMap<int32, int32> CorridorSlots;
};
