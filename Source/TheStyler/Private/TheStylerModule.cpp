#include "TheStylerModule.h"

#include "EdGraphUtilities.h"
#include "Framework/Commands/UICommandList.h"
#include "Interfaces/IMainFrameModule.h"
#include "SActionButton.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "UObject/UObjectIterator.h"

#include "TheGraphArranger.h"
#include "TheStylerCommands.h"
#include "TheStylerSettings.h"
#include "TheWireDrawingPolicy.h"

#define LOCTEXT_NAMESPACE "TheStyler"

DEFINE_LOG_CATEGORY(LogTheStyler);

void FTheStylerModule::StartupModule()
{
    FTheStylerCommands::Register();

    // Global hotkey via the main-frame command bindings (fires while a graph editor is focused).
    IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
    MainFrame.GetMainFrameCommandBindings()->MapAction(FTheStylerCommands::Get().ArrangeNodes, FExecuteAction::CreateStatic(&FTheGraphArranger::ArrangeActiveGraph));
    MainFrame.GetMainFrameCommandBindings()->MapAction(FTheStylerCommands::Get().FormatNode, FExecuteAction::CreateStatic(&FTheGraphArranger::FormatActiveSelection));

    // Context-menu entry — registered once the tool-menu system is ready.
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTheStylerModule::RegisterMenus));

    // Manhattan wire styling for Blueprint graphs.
    WireFactory = MakeShared<FTheWireConnectionFactory>();
    FEdGraphUtilities::RegisterVisualPinConnectionFactory(WireFactory);
}

void FTheStylerModule::ShutdownModule()
{
    if(WireFactory.IsValid())
    {
        FEdGraphUtilities::UnregisterVisualPinConnectionFactory(WireFactory);
        WireFactory.Reset();
    }

    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    if(IMainFrameModule* MainFrame = FModuleManager::GetModulePtr<IMainFrameModule>(TEXT("MainFrame")))
    {
        MainFrame->GetMainFrameCommandBindings()->UnmapAction(FTheStylerCommands::Get().ArrangeNodes);
        MainFrame->GetMainFrameCommandBindings()->UnmapAction(FTheStylerCommands::Get().FormatNode);
    }

    FTheStylerCommands::Unregister();
}

void FTheStylerModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    // The main-frame command list is where ArrangeNodes' action is mapped, so binding menu entries to
    // it shows the keyboard shortcut next to the label.
    const TSharedRef<FUICommandList> CommandList = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame")).GetMainFrameCommandBindings();

    // Context-menu entry — "GraphEditor.GraphContextMenu.Common" is the shared ancestor of every graph
    // context menu (node right-click and empty-graph right-click), so one entry here appears everywhere.
    if(UToolMenu* ContextMenu = UToolMenus::Get()->ExtendMenu(TEXT("GraphEditor.GraphContextMenu.Common")))
    {
        FToolMenuSection& Section = ContextMenu->AddSection(TEXT("TheStyler"), LOCTEXT("SectionLabel", "The Styler"));
        Section.AddMenuEntryWithCommandList(FTheStylerCommands::Get().ArrangeNodes, CommandList);
        Section.AddMenuEntryWithCommandList(FTheStylerCommands::Get().FormatNode, CommandList);
    }

    // Toolbar button — every asset editor's toolbar inherits from this shared parent, so one entry
    // reaches all of them; a visibility gate then shows it only in editors that host a graph panel
    // (Blueprint, dialogue, material, ...). Both delegates resolve the graph from the toolbar's own
    // menu context (its owning toolkit), so the button targets the editor it lives in regardless of
    // which tab role hosts the graph — the Material editor's graph is a fixed Document tab the global
    // active-tab lookup misses. Bound to a direct action rather than the command, since the toolbar
    // context has no access to the main-frame command list.
    if(UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu(TEXT("AssetEditor.DefaultToolBar")))
    {
        FToolUIAction Action;
        Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context) { FTheGraphArranger::ArrangeGraphFromContext(Context); });
        Action.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateLambda([](const FToolMenuContext& Context) { return FTheGraphArranger::HasGraphInContext(Context); });

        FToolMenuSection& Section = Toolbar->FindOrAddSection(TEXT("TheStyler"));
        Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("TheArrangeNodes"),
            FToolUIActionChoice(Action),
            LOCTEXT("ArrangeToolbarLabel", "Arrange"),
            LOCTEXT("ArrangeToolbarTooltip", "Auto-arrange the current graph's nodes — selected, or all if none selected (Shift+Q)."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GraphEditor.StraightenConnections"))));

        // Wire-styling quick controls. Both mutate the plugin settings and persist to the project
        // config, so the state is shared with Project Settings -> Plugins -> The Styler; wires
        // re-read the settings every paint, so the effect is immediate.
        const auto GraphVisible = FToolMenuIsActionButtonVisible::CreateLambda([](const FToolMenuContext& Context) { return FTheGraphArranger::HasGraphInContext(Context); });

        // "Wires" cycles the routing style, with Off as part of the loop:
        // Off -> Manhattan -> Metro 45 -> Straight -> Off. The label shows the current state.
        FToolUIAction CycleStyle;
        CycleStyle.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
            [](const FToolMenuContext&)
            {
                const auto Settings = GetMutableDefault<UTheStylerSettings>();
                if(!Settings->bEnableWireStyling)
                {
                    Settings->bEnableWireStyling = true;
                    Settings->WireStyle = ETheWireStyle::Manhattan;
                }
                else if(Settings->WireStyle == ETheWireStyle::Manhattan)
                {
                    Settings->WireStyle = ETheWireStyle::Metro45;
                }
                else if(Settings->WireStyle == ETheWireStyle::Metro45)
                {
                    Settings->WireStyle = ETheWireStyle::Straight;
                }
                else
                {
                    Settings->bEnableWireStyling = false;
                    Settings->WireStyle = ETheWireStyle::Manhattan;
                }
                Settings->TryUpdateDefaultConfigFile();
            });
        CycleStyle.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([](const FToolMenuContext&) { return GetDefault<UTheStylerSettings>()->bEnableWireStyling ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; });
        CycleStyle.IsActionVisibleDelegate = GraphVisible;

        const TAttribute<FText> StyleLabel = TAttribute<FText>::CreateLambda(
            []()
            {
                const auto Settings = GetDefault<UTheStylerSettings>();
                if(!Settings->bEnableWireStyling)
                {
                    return LOCTEXT("WireStyleOff", "Wires: Off");
                }
                switch(Settings->WireStyle)
                {
                    case ETheWireStyle::Metro45:
                        return LOCTEXT("WireStyleMetro", "Metro 45");
                    case ETheWireStyle::Straight:
                        return LOCTEXT("WireStyleStraight", "Straight");
                    default:
                        return LOCTEXT("WireStyleManhattan", "Manhattan");
                }
            });
        Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("TheCycleWireStyle"),
            FToolUIActionChoice(CycleStyle),
            StyleLabel,
            LOCTEXT("WireStyleTooltip", "Cycle the exec-wire style: Off -> Manhattan -> Metro 45 -> Straight."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GraphEditor.StraightenConnections")),
            EUserInterfaceActionType::ToggleButton));

        // Focus-dim toggle; greyed out while the wire restyle itself is off.
        FToolUIAction FocusToggle;
        FocusToggle.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
            [](const FToolMenuContext&)
            {
                const auto Settings = GetMutableDefault<UTheStylerSettings>();
                Settings->bFocusDimOnSelection = !Settings->bFocusDimOnSelection;
                Settings->TryUpdateDefaultConfigFile();

                // Focus is one concept to the user, but node dimming lives in per-editor settings
                // (the dialogue/quest editors' bDimUnselectedNodes). Flip every such flag in sync,
                // matched by property name so the plugin stays decoupled from those modules.
                for(TObjectIterator<UClass> ClassNdx; ClassNdx; ++ClassNdx)
                {
                    if(!ClassNdx->IsChildOf(UDeveloperSettings::StaticClass()) || ClassNdx->HasAnyClassFlags(CLASS_Abstract) || *ClassNdx == UTheStylerSettings::StaticClass())
                    {
                        continue;
                    }
                    if(FBoolProperty* const DimProperty = FindFProperty<FBoolProperty>(*ClassNdx, TEXT("bDimUnselectedNodes")))
                    {
                        const auto EditorSettings = CastChecked<UDeveloperSettings>(ClassNdx->GetDefaultObject());
                        DimProperty->SetPropertyValue_InContainer(EditorSettings, Settings->bFocusDimOnSelection);
                        EditorSettings->TryUpdateDefaultConfigFile();
                    }
                }
            });
        FocusToggle.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([](const FToolMenuContext&) { return GetDefault<UTheStylerSettings>()->bFocusDimOnSelection ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; });
        FocusToggle.CanExecuteAction = FToolMenuCanExecuteAction::CreateLambda([](const FToolMenuContext&) { return GetDefault<UTheStylerSettings>()->bEnableWireStyling; });
        FocusToggle.IsActionVisibleDelegate = GraphVisible;
        Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("TheToggleFocus"),
            FToolUIActionChoice(FocusToggle),
            LOCTEXT("FocusToolbarLabel", "Focus"),
            LOCTEXT("FocusToolbarTooltip", "Dim wires not touching the selected nodes (focus mode)."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GraphEditor.ToggleHideUnrelatedNodes")),
            EUserInterfaceActionType::ToggleButton));
    }

    RegisterContentBrowserMenu();
}

void FTheStylerModule::RegisterContentBrowserMenu()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    // The "The" dropdown in the Content Browser toolbar: a shared home for The* editor commands. The menu
    // it opens is registered empty here; features add their own entries by extending it (see
    // TheStyler::ContentBrowserMenuName). It is generated fresh on each open, so late extensions still show.
    if(!UToolMenus::Get()->IsMenuRegistered(TheStyler::ContentBrowserMenuName))
    {
        UToolMenus::Get()->RegisterMenu(TheStyler::ContentBrowserMenuName);
    }

    UToolMenu* ContentBrowserToolbar = UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.ToolBar"));
    if(!ContentBrowserToolbar)
    {
        return;
    }

    // SActionButton in combo mode (no OnClicked, opens OnGetMenuContent) so it matches the native
    // Content Browser toolbar buttons rather than the duller generic toolbar block.
    const TSharedRef<SActionButton> TheButton = SNew(SActionButton)
                                                    .Text(LOCTEXT("TheMenuLabel", "The"))
                                                    .ToolTipText(LOCTEXT("TheMenuTooltip", "Commands from the The* plugins."))
                                                    .Icon(FAppStyle::Get().GetBrush("Icons.Toolbar.Settings"))
                                                    .OnGetMenuContent_Lambda([]() { return UToolMenus::Get()->GenerateWidget(TheStyler::ContentBrowserMenuName, FToolMenuContext()); });

    FToolMenuSection& Section = ContentBrowserToolbar->FindOrAddSection(TEXT("Save"));
    Section.AddEntry(FToolMenuEntry::InitWidget(TEXT("TheCommandsMenu"),
        TheButton,
        FText::GetEmpty(),
        /*bNoIndent*/ true,
        /*bSearchable*/ false));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTheStylerModule, TheStyler)
