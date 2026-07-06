#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FTheStylerCommands : public TCommands<FTheStylerCommands>
{
public:
    FTheStylerCommands();

    virtual void RegisterCommands() override;

    TSharedPtr<FUICommandInfo> ArrangeNodes;
    TSharedPtr<FUICommandInfo> FormatNode;

    // Align the selected nodes to a shared edge/center.
    TSharedPtr<FUICommandInfo> AlignLeft;
    TSharedPtr<FUICommandInfo> AlignRight;
    TSharedPtr<FUICommandInfo> AlignTop;
    TSharedPtr<FUICommandInfo> AlignBottom;
    TSharedPtr<FUICommandInfo> AlignCenterX;
    TSharedPtr<FUICommandInfo> AlignCenterY;

    // Even out the gaps between the selected nodes along an axis.
    TSharedPtr<FUICommandInfo> DistributeHorizontally;
    TSharedPtr<FUICommandInfo> DistributeVertically;
};
