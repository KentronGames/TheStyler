// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheStylerModule.h"

#include "EdGraphUtilities.h"
#include "Framework/Commands/UICommandList.h"
#include "Interfaces/IMainFrameModule.h"
#include "SActionButton.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"

#include "TheFolderColorSync.h"
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

    FormatOnConnect = MakeShared<FTheFormatOnConnect>();
    FormatOnConnect->Register();
}

void FTheStylerModule::ShutdownModule()
{
    FTheFolderColorSync::Shutdown();

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
                const auto Settings = GetMutableDefault<UTheStylerViewSettings>();
                if(!Settings->bEnableWireStyling)
                {
                    Settings->bEnableWireStyling = true;
                    Settings->WireStyle = ETheStylerWireStyle::Manhattan;
                }
                else if(Settings->WireStyle == ETheStylerWireStyle::Manhattan)
                {
                    Settings->WireStyle = ETheStylerWireStyle::Metro45;
                }
                else if(Settings->WireStyle == ETheStylerWireStyle::Metro45)
                {
                    Settings->WireStyle = ETheStylerWireStyle::Straight;
                }
                else
                {
                    Settings->bEnableWireStyling = false;
                    Settings->WireStyle = ETheStylerWireStyle::Manhattan;
                }
                Settings->SaveConfig();
            });
        CycleStyle.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([](const FToolMenuContext&) { return GetDefault<UTheStylerViewSettings>()->bEnableWireStyling ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; });
        CycleStyle.IsActionVisibleDelegate = GraphVisible;

        const TAttribute<FText> StyleLabel = TAttribute<FText>::CreateLambda(
            []()
            {
                const auto Settings = GetDefault<UTheStylerViewSettings>();
                if(!Settings->bEnableWireStyling)
                {
                    return LOCTEXT("WireStyleOff", "Wires: Off");
                }
                switch(Settings->WireStyle)
                {
                    case ETheStylerWireStyle::Metro45:
                        return LOCTEXT("WireStyleMetro", "Metro 45");
                    case ETheStylerWireStyle::Straight:
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
                const auto Settings = GetMutableDefault<UTheStylerViewSettings>();
                Settings->bFocusDimOnSelection = !Settings->bFocusDimOnSelection;
                Settings->SaveConfig();

                const auto& SharedSettings = *GetDefault<UTheStylerSettings>();
                for(const FString& ClassName : SharedSettings.FocusDimSettingsClasses)
                {
                    UClass* const SettingsClass = FindObject<UClass>(nullptr, *ClassName);
                    if(!SettingsClass || !SettingsClass->IsChildOf(UDeveloperSettings::StaticClass()) || SettingsClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
                    {
                        continue;
                    }
                    if(FBoolProperty* const DimProperty = FindFProperty<FBoolProperty>(SettingsClass, TEXT("bDimUnselectedNodes")))
                    {
                        if(const auto EditorSettings = Cast<UDeveloperSettings>(SettingsClass->GetDefaultObject()))
                        {
                            DimProperty->SetPropertyValue_InContainer(EditorSettings, Settings->bFocusDimOnSelection);
                        }
                    }
                }
            });
        FocusToggle.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([](const FToolMenuContext&) { return GetDefault<UTheStylerViewSettings>()->bFocusDimOnSelection ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; });
        FocusToggle.CanExecuteAction = FToolMenuCanExecuteAction::CreateLambda([](const FToolMenuContext&) { return GetDefault<UTheStylerViewSettings>()->bEnableWireStyling; });
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
                                                    .Text(LOCTEXT("TheMenuLabel", "The Styler"))
                                                    .ToolTipText(LOCTEXT("TheMenuTooltip", "The Styler — Content Browser folder colour commands."))
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
