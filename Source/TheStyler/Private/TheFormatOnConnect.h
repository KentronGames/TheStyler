// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class IAssetEditorInstance;
class UEdGraph;
class UEdGraphNode;
struct FEdGraphEditAction;

class FTheFormatOnConnect
{
public:
    void Register();
    void Unregister();

private:
    void OnAssetOpened(UObject* Asset, IAssetEditorInstance* Editor);
    void HookActiveGraph();
    void OnGraphChanged(const FEdGraphEditAction& Action);
    void FormatDeferred(TSet<TWeakObjectPtr<UEdGraphNode>> AddedNodes);

    FDelegateHandle AssetOpenedHandle;
    TMap<TWeakObjectPtr<UEdGraph>, FDelegateHandle> HookedGraphs;
    bool bFormatting = false;
};
