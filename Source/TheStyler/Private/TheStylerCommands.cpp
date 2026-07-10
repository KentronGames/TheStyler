// (c) 2026 Kentron Cowboys. All rights reserved.

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
    UI_COMMAND(FormatNode, "Format Node", "Auto-arrange only the wire-connected component of the selected node(s)", EUserInterfaceActionType::Button, FInputChord(EKeys::F, true, false, false, false));

    // Align / Distribute — menu-only (no default chords to avoid clashing with editor shortcuts).
    UI_COMMAND(AlignLeft, "Align Left", "Align the selected nodes' left edges", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(AlignRight, "Align Right", "Align the selected nodes' right edges", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(AlignTop, "Align Top", "Align the selected nodes' top edges", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(AlignBottom, "Align Bottom", "Align the selected nodes' bottom edges", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(AlignCenterX, "Align Center X", "Align the selected nodes on a shared vertical center line", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(AlignCenterY, "Align Center Y", "Align the selected nodes on a shared horizontal center line", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(DistributeHorizontally, "Distribute Horizontally", "Even out the horizontal gaps between the selected nodes", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(DistributeVertically, "Distribute Vertically", "Even out the vertical gaps between the selected nodes", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
