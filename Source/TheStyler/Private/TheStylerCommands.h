#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FTheStylerCommands : public TCommands<FTheStylerCommands>
{
public:
    FTheStylerCommands();

    virtual void RegisterCommands() override;

    TSharedPtr<FUICommandInfo> ArrangeNodes;
};
