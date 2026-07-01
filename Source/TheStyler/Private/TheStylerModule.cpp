#include "TheStylerModule.h"

#include "EdGraphUtilities.h"
#include "Framework/Commands/UICommandList.h"
#include "Interfaces/IMainFrameModule.h"
#include "ToolMenus.h"

#include "TheGraphArranger.h"
#include "TheStylerCommands.h"
#include "TheWireDrawingPolicy.h"

#define LOCTEXT_NAMESPACE "TheStyler"

DEFINE_LOG_CATEGORY(LogTheStyler);

void FTheStylerModule::StartupModule()
{
    FTheStylerCommands::Register();

    // Global hotkey via the main-frame command bindings (fires while a graph editor is focused).
    IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
    MainFrame.GetMainFrameCommandBindings()->MapAction(FTheStylerCommands::Get().ArrangeNodes, FExecuteAction::CreateStatic(&FTheGraphArranger::ArrangeActiveGraph));

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
    }

    FTheStylerCommands::Unregister();
}

void FTheStylerModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    // "GraphEditor.GraphContextMenu.Common" is the shared ancestor of every graph context menu
    // (node right-click and empty-graph right-click), so one entry here appears everywhere.
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("GraphEditor.GraphContextMenu.Common"));
    if(!Menu)
    {
        return;
    }

    // Bind the entry to the command via the main-frame command list (where its action is mapped)
    // so the menu item shows its keyboard shortcut next to the label.
    const TSharedRef<FUICommandList> CommandList = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame")).GetMainFrameCommandBindings();

    FToolMenuSection& Section = Menu->AddSection(TEXT("TheStyler"), LOCTEXT("SectionLabel", "The Styler"));
    Section.AddMenuEntryWithCommandList(FTheStylerCommands::Get().ArrangeNodes, CommandList);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTheStylerModule, TheStyler)
