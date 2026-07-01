#include "TheMaterialRebuild.h"

#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Materials/Material.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "TheAssetToolsLibrary.h"
#include "TheStylerModule.h"

#define LOCTEXT_NAMESPACE "TheStyler"

UMaterial* FTheMaterialRebuild::MaterialFromContext(const FToolMenuContext& Context)
{
    const auto ToolkitContext = Context.FindContext<UAssetEditorToolkitMenuContext>();
    if(!ToolkitContext)
    {
        return nullptr;
    }
    for(UObject* const Object : ToolkitContext->GetEditingObjects())
    {
        if(const auto Material = Cast<UMaterial>(Object))
        {
            return Material;
        }
    }
    return nullptr;
}

bool FTheMaterialRebuild::HasMaterialInContext(const FToolMenuContext& Context)
{
    return MaterialFromContext(Context) != nullptr;
}

void FTheMaterialRebuild::ApplyResult(UMaterial* Material, const FString& Result, const FText& SuccessText)
{
    const bool bOk = !Result.StartsWith(TEXT("ERROR:"));

    FNotificationInfo Info(bOk ? SuccessText : FText::Format(LOCTEXT("MatHlslFail", "HLSL sync failed: {0}"), FText::FromString(Result)));
    Info.ExpireDuration = 5.0f;
    Info.bUseSuccessFailIcons = true;
    const auto Notification = FSlateNotificationManager::Get().AddNotification(Info);
    if(Notification)
    {
        Notification->SetCompletionState(bOk ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
    }

    if(!bOk)
    {
        UE_LOG(LogTheStyler, Warning, TEXT("HLSL sync failed for %s: %s"), *Material->GetPathName(), *Result);
        return;
    }

    // Both Import and Export rebuild the graph, replacing the expression nodes the open editor holds; reopen
    // next tick so the view refreshes instead of going stale. Deferred so we don't tear down this toolkit mid-click.
    const TWeakObjectPtr<UMaterial> WeakMaterial(Material);
    GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateLambda(
        [WeakMaterial]()
        {
            if(UMaterial* const Reopen = WeakMaterial.Get())
            {
                UAssetEditorSubsystem* const Subsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
                Subsystem->CloseAllEditorsForAsset(Reopen);
                Subsystem->OpenEditorForAsset(Reopen);
            }
        }));
}

void FTheMaterialRebuild::ImportFromContext(const FToolMenuContext& Context)
{
    if(UMaterial* const Material = MaterialFromContext(Context))
    {
        const FString Result = UTheAssetToolsLibrary::RebuildMaterialFromSource(Material->GetPathName());
        ApplyResult(Material, Result, LOCTEXT("ImportOk", "Imported from HLSL — reopening the material."));
    }
}

void FTheMaterialRebuild::ExportFromContext(const FToolMenuContext& Context)
{
    if(UMaterial* const Material = MaterialFromContext(Context))
    {
        const FString Result = UTheAssetToolsLibrary::ExportMaterialDefaultsToSource(Material->GetPathName());
        ApplyResult(Material, Result, LOCTEXT("ExportOk", "Exported current parameter defaults to HLSL."));
    }
}

void FTheMaterialRebuild::RegisterMenuEntry()
{
    FToolMenuOwnerScoped OwnerScoped(TEXT("TheStyler"));

    UToolMenu* const Toolbar = UToolMenus::Get()->ExtendMenu(TEXT("AssetEditor.DefaultToolBar"));
    if(!Toolbar)
    {
        return;
    }

    const auto VisibleWhenMaterial = FToolMenuIsActionButtonVisible::CreateLambda([](const FToolMenuContext& Context) { return FTheMaterialRebuild::HasMaterialInContext(Context); });

    FToolUIAction ImportAction;
    ImportAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context) { FTheMaterialRebuild::ImportFromContext(Context); });
    ImportAction.IsActionVisibleDelegate = VisibleWhenMaterial;

    FToolUIAction ExportAction;
    ExportAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context) { FTheMaterialRebuild::ExportFromContext(Context); });
    ExportAction.IsActionVisibleDelegate = VisibleWhenMaterial;

    FToolMenuSection& Section = Toolbar->FindOrAddSection(TEXT("TheStyler"));
    Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("TheImportMaterialHLSL"),
        FToolUIActionChoice(ImportAction),
        LOCTEXT("ImportLabel", "ImportHLSL"),
        LOCTEXT("ImportTooltip", "Rebuild this material from its .hlsl source of truth (found by matching the header's asset path), then reopen it."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Import"))));
    Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("TheExportMaterialHLSL"),
        FToolUIActionChoice(ExportAction),
        LOCTEXT("ExportLabel", "ExportHLSL"),
        LOCTEXT("ExportTooltip", "Write this material's current scalar/vector parameter defaults back into its .hlsl header, so a later ImportHLSL keeps the tuned values."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Export"))));
}

#undef LOCTEXT_NAMESPACE
