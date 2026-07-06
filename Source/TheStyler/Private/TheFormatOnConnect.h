#pragma once

#include "CoreMinimal.h"

class IAssetEditorInstance;
class UEdGraph;
class UEdGraphNode;
struct FEdGraphEditAction;

/**
 * Optional "format on connect": while UTheStylerSettings::bFormatOnNodeAdded is on, adding a node to a
 * graph (e.g. dragging off a pin) auto-arranges the wire-connected cluster it joins. Hooks each graph's
 * change delegate as its editor opens; all work is deferred a tick and guarded against self-triggering.
 */
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
