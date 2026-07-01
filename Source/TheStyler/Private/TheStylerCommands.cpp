#include "TheStylerCommands.h"

#include "Framework/Commands/InputChord.h"
#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "TheStyler"

FTheStylerCommands::FTheStylerCommands() : TCommands<FTheStylerCommands>(TEXT("TheStyler"), NSLOCTEXT("Contexts", "TheStyler", "The Styler"), NAME_None, FAppStyle::GetAppStyleSetName())
{
}

void FTheStylerCommands::RegisterCommands()
{
    // (Key, bShift, bCtrl, bAlt, bCmd)
    UI_COMMAND(ArrangeNodes, "Arrange Nodes", "Auto-arrange the current graph's nodes (selected, or all if none selected)", EUserInterfaceActionType::Button, FInputChord(EKeys::Q, true, false, false, false));
}

#undef LOCTEXT_NAMESPACE
