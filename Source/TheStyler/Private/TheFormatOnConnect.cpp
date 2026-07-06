#include "TheFormatOnConnect.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "SGraphPanel.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "TimerManager.h"

#include "TheGraphArranger.h"
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
    // The graph panel is not in the widget tree yet on this callback — hook the graph a tick later.
    if(GEditor)
    {
        GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FTheFormatOnConnect::HookActiveGraph));
    }
}

void FTheFormatOnConnect::HookActiveGraph()
{
    // Drop dead entries so the map doesn't grow across a long editor session.
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
}

void FTheFormatOnConnect::OnGraphChanged(const FEdGraphEditAction& Action)
{
    // Ignore changes we cause ourselves, and do nothing unless the opt-in feature is on.
    if(bFormatting || !GetDefault<UTheStylerSettings>()->bFormatOnNodeAdded)
    {
        return;
    }
    if((Action.Action & GRAPHACTION_AddNode) == 0 || Action.Nodes.Num() == 0)
    {
        return;
    }

    // Defer: the editor is still placing/connecting the node; arranging inside the notification fights
    // that. Hold the added nodes weakly and re-check next tick.
    TSet<TWeakObjectPtr<UEdGraphNode>> Added;
    for(const UEdGraphNode* Node : Action.Nodes)
    {
        if(Node)
        {
            Added.Add(const_cast<UEdGraphNode*>(Node));
        }
    }
    if(Added.Num() > 0 && GEditor)
    {
        GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FTheFormatOnConnect::FormatDeferred, Added));
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
    if(Seeds.Num() == 0)
    {
        return;
    }

    // Our arrange calls NotifyGraphChanged; the guard stops that from re-entering this handler.
    TGuardValue<bool> ReentryGuard(bFormatting, true);
    FTheGraphArranger::FormatComponentInActivePanel(Seeds);
}
