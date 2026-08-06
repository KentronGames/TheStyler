// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheStylerModule.h"

#include "EdGraphUtilities.h"
#include "Framework/Commands/UICommandList.h"
#include "Interfaces/IMainFrameModule.h"
#include "SActionButton.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "UObject/UObjectIterator.h"

#include "TheFormatOnConnect.h"
#include "TheGraphArranger.h"
#include "TheStylerCommands.h"
#include "TheStylerSettings.h"
#include "TheWireDrawingPolicy.h"

#define LOCTEXT_NAMESPACE "TheStyler"

DEFINE_LOG_CATEGORY(LogTheStyler);

void FTheStylerModule::StartupModule()
{
    FTheStylerCommands::Register();

    IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
    MainFrame.GetMainFrameCommandBindings()->MapAction(FTheStylerCommands::Get().ArrangeNodes, FExecuteAction::CreateStatic(&FTheGraphArranger::ArrangeActiveGraph));
    MainFrame.GetMainFrameCommandBindings()->MapAction(FTheStylerCommands::Get().FormatNode, FExecuteAction::CreateStatic(&FTheGraphArranger::FormatActiveSelection));

    const auto& Cmds = FTheStylerCommands::Get();
    const auto Bindings = MainFrame.GetMainFrameCommandBindings();
    Bindings->MapAction(Cmds.AlignLeft, FExecuteAction::CreateStatic(&FTheGraphArranger::AlignActiveSelection, FTheGraphArranger::ETheAlign::Left));
    Bindings->MapAction(Cmds.AlignRight, FExecuteAction::CreateStatic(&FTheGraphArranger::AlignActiveSelection, FTheGraphArranger::ETheAlign::Right));
    Bindings->MapAction(Cmds.AlignTop, FExecuteAction::CreateStatic(&FTheGraphArranger::AlignActiveSelection, FTheGraphArranger::ETheAlign::Top));
    Bindings->MapAction(Cmds.AlignBottom, FExecuteAction::CreateStatic(&FTheGraphArranger::AlignActiveSelection, FTheGraphArranger::ETheAlign::Bottom));
    Bindings->MapAction(Cmds.AlignCenterX, FExecuteAction::CreateStatic(&FTheGraphArranger::AlignActiveSelection, FTheGraphArranger::ETheAlign::CenterX));
    Bindings->MapAction(Cmds.AlignCenterY, FExecuteAction::CreateStatic(&FTheGraphArranger::AlignActiveSelection, FTheGraphArranger::ETheAlign::CenterY));
    Bindings->MapAction(Cmds.DistributeHorizontally, FExecuteAction::CreateStatic(&FTheGraphArranger::DistributeActiveSelection, FTheGraphArranger::ETheDistribute::Horizontal));
    Bindings->MapAction(Cmds.DistributeVertically, FExecuteAction::CreateStatic(&FTheGraphArranger::DistributeActiveSelection, FTheGraphArranger::ETheDistribute::Vertical));

    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTheStylerModule::RegisterMenus));

    WireFactory = MakeShared<FTheWireConnectionFactory>();
    FEdGraphUtilities::RegisterVisualPinConnectionFactory(WireFactory);

    FormatOnConnect = MakeUnique<FTheFormatOnConnect>();
    FormatOnConnect->Register();
}

void FTheStylerModule::ShutdownModule()
{
    if(FormatOnConnect.IsValid())
    {
        FormatOnConnect->Unregister();
        FormatOnConnect.Reset();
    }

    if(WireFactory.IsValid())
    {
        FEdGraphUtilities::UnregisterVisualPinConnectionFactory(WireFactory);
        WireFactory.Reset();
    }

    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    if(IMainFrameModule* MainFrame = FModuleManager::GetModulePtr<IMainFrameModule>(TEXT("MainFrame")))
    {
        const auto& Cmds = FTheStylerCommands::Get();
        const auto Bindings = MainFrame->GetMainFrameCommandBindings();
        Bindings->UnmapAction(Cmds.ArrangeNodes);
        Bindings->UnmapAction(Cmds.FormatNode);
        Bindings->UnmapAction(Cmds.AlignLeft);
        Bindings->UnmapAction(Cmds.AlignRight);
        Bindings->UnmapAction(Cmds.AlignTop);
        Bindings->UnmapAction(Cmds.AlignBottom);
        Bindings->UnmapAction(Cmds.AlignCenterX);
        Bindings->UnmapAction(Cmds.AlignCenterY);
        Bindings->UnmapAction(Cmds.DistributeHorizontally);
        Bindings->UnmapAction(Cmds.DistributeVertically);
    }

    FTheStylerCommands::Unregister();
}

void FTheStylerModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    const TSharedRef<FUICommandList> CommandList = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame")).GetMainFrameCommandBindings();

    if(UToolMenu* ContextMenu = UToolMenus::Get()->ExtendMenu(TEXT("GraphEditor.GraphContextMenu.Common")))
    {
        FToolMenuSection& Section = ContextMenu->AddSection(TEXT("TheStyler"), LOCTEXT("SectionLabel", "The Styler"));
        Section.AddMenuEntryWithCommandList(FTheStylerCommands::Get().ArrangeNodes, CommandList);
        Section.AddMenuEntryWithCommandList(FTheStylerCommands::Get().FormatNode, CommandList);

        Section.AddSubMenu(TEXT("TheAlignDistribute"),
            LOCTEXT("AlignSubmenuLabel", "Align / Distribute"),
            LOCTEXT("AlignSubmenuTooltip", "Align or evenly space the selected nodes."),
            FNewToolMenuDelegate::CreateLambda(
                [CommandList](UToolMenu* SubMenu)
                {
                    const auto& Cmds = FTheStylerCommands::Get();
                    FToolMenuSection& AlignSection = SubMenu->AddSection(TEXT("Align"), LOCTEXT("AlignSectionLabel", "Align"));
                    AlignSection.AddMenuEntryWithCommandList(Cmds.AlignLeft, CommandList);
                    AlignSection.AddMenuEntryWithCommandList(Cmds.AlignRight, CommandList);
                    AlignSection.AddMenuEntryWithCommandList(Cmds.AlignTop, CommandList);
                    AlignSection.AddMenuEntryWithCommandList(Cmds.AlignBottom, CommandList);
                    AlignSection.AddMenuEntryWithCommandList(Cmds.AlignCenterX, CommandList);
                    AlignSection.AddMenuEntryWithCommandList(Cmds.AlignCenterY, CommandList);

                    FToolMenuSection& DistSection = SubMenu->AddSection(TEXT("Distribute"), LOCTEXT("DistSectionLabel", "Distribute"));
                    DistSection.AddMenuEntryWithCommandList(Cmds.DistributeHorizontally, CommandList);
                    DistSection.AddMenuEntryWithCommandList(Cmds.DistributeVertically, CommandList);
                }));
    }

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

        const auto GraphVisible = FToolMenuIsActionButtonVisible::CreateLambda([](const FToolMenuContext& Context) { return FTheGraphArranger::HasGraphInContext(Context); });

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

        FToolUIAction FocusToggle;
        FocusToggle.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
            [](const FToolMenuContext&)
            {
                const auto Settings = GetMutableDefault<UTheStylerSettings>();
                Settings->bFocusDimOnSelection = !Settings->bFocusDimOnSelection;
                Settings->TryUpdateDefaultConfigFile();

                for(TObjectIterator<UClass> ClassNdx; ClassNdx; ++ClassNdx)
                {
                    if(!ClassNdx->IsChildOf(UDeveloperSettings::StaticClass()) || ClassNdx->HasAnyClassFlags(CLASS_Abstract) || *ClassNdx == UTheStylerSettings::StaticClass())
                    {
                        continue;
                    }
                    if(!Settings->FocusDimSettingsClasses.Contains(ClassNdx->GetName()))
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

    if(!UToolMenus::Get()->IsMenuRegistered(TheStyler::ContentBrowserMenuName))
    {
        UToolMenus::Get()->RegisterMenu(TheStyler::ContentBrowserMenuName);
    }

    UToolMenu* ContentBrowserToolbar = UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.ToolBar"));
    if(!ContentBrowserToolbar)
    {
        return;
    }

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
