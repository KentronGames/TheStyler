// (c) 2026 Kentron Cowboys. All rights reserved.

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

    TSharedPtr<FUICommandInfo> AlignLeft;
    TSharedPtr<FUICommandInfo> AlignRight;
    TSharedPtr<FUICommandInfo> AlignTop;
    TSharedPtr<FUICommandInfo> AlignBottom;
    TSharedPtr<FUICommandInfo> AlignCenterX;
    TSharedPtr<FUICommandInfo> AlignCenterY;

    TSharedPtr<FUICommandInfo> DistributeHorizontally;
    TSharedPtr<FUICommandInfo> DistributeVertically;
};
