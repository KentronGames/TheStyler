// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheFormatOnConnect.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "SGraphPanel.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "TimerManager.h"

#include "TheGraphArranger.h"
#include "TheStylerModule.h"
#include "TheStylerSettings.h"

void FTheFormatOnConnect::Register()
{
    if(!GEditor)
    {
        return;
    }
    if(UAssetEditorSubsystem* const Subsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
    {
        AssetOpenedHandle = Subsystem->OnAssetOpenedInEditor().AddRaw(this, &FTheFormatOnConnect::OnAssetOpened);
    }
}

void FTheFormatOnConnect::Unregister()
{
    if(GEditor)
    {
        if(UAssetEditorSubsystem* const Subsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            Subsystem->OnAssetOpenedInEditor().Remove(AssetOpenedHandle);
        }
    }
    for(const auto& Pair : HookedGraphs)
    {
        if(UEdGraph* const Graph = Pair.Key.Get())
        {
            Graph->RemoveOnGraphChangedHandler(Pair.Value);
        }
    }
    HookedGraphs.Reset();
}

void FTheFormatOnConnect::OnAssetOpened(UObject*, IAssetEditorInstance*)
{
    if(GEditor)
    {
        GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &FTheFormatOnConnect::HookActiveGraph));
    }
}

void FTheFormatOnConnect::HookActiveGraph()
{
    for(auto It = HookedGraphs.CreateIterator(); It; ++It)
    {
        if(!It->Key.IsValid())
        {
            It.RemoveCurrent();
        }
    }

    const auto Panel = FTheGraphArranger::GetActiveGraphPanel();
    if(!Panel.IsValid())
    {
        return;
    }
    UEdGraph* const Graph = Panel->GetGraphObj();
    if(!IsValid(Graph) || HookedGraphs.Contains(Graph))
    {
        return;
    }
    const FDelegateHandle Handle = Graph->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateRaw(this, &FTheFormatOnConnect::OnGraphChanged));
    HookedGraphs.Add(Graph, Handle);
    UE_LOG(LogTheStyler, Verbose, TEXT("FormatOnConnect: hooked graph %s, %d graph(s) hooked"), *Graph->GetPathName(), HookedGraphs.Num());
}

void FTheFormatOnConnect::OnGraphChanged(const FEdGraphEditAction& Action)
{
    if(bFormatting || !GetDefault<UTheStylerViewSettings>()->bFormatOnNodeAdded)
    {
        return;
    }
    if((Action.Action & GRAPHACTION_AddNode) == 0 || Action.Nodes.Num() == 0)
    {
        return;
    }

    TSet<TWeakObjectPtr<UEdGraphNode>> Added;
    for(const UEdGraphNode* Node : Action.Nodes)
    {
        if(Node)
        {
            Added.Add(const_cast<UEdGraphNode*>(Node));
        }
    }
    UE_LOG(LogTheStyler, Verbose, TEXT("FormatOnConnect: graph action 0x%x added %d node(s), %d valid - formatting next tick"), static_cast<uint32>(Action.Action), Action.Nodes.Num(), Added.Num());
    if(Added.Num() > 0 && GEditor)
    {
        GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &FTheFormatOnConnect::FormatDeferred, Added));
    }
}

void FTheFormatOnConnect::FormatDeferred(TSet<TWeakObjectPtr<UEdGraphNode>> AddedNodes)
{
    TSet<UEdGraphNode*> Seeds;
    for(const auto& Weak : AddedNodes)
    {
        if(UEdGraphNode* const Node = Weak.Get())
        {
            Seeds.Add(Node);
        }
    }
    UE_LOG(LogTheStyler, Verbose, TEXT("FormatOnConnect: %d of %d added node(s) still alive"), Seeds.Num(), AddedNodes.Num());
    if(Seeds.Num() == 0)
    {
        return;
    }

    TGuardValue<bool> ReentryGuard(bFormatting, true);
    FTheGraphArranger::FormatComponentInActivePanel(Seeds);
}
