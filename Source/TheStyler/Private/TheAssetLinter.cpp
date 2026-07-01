#include "TheAssetLinter.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Framework/Commands/UIAction.h"
#include "IAssetTools.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/UObjectToken.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "TheStylerModule.h"

#define LOCTEXT_NAMESPACE "TheStyler"

namespace TheAssetLinter
{
const FName MessageLogName(TEXT("TheStyler"));

// Register the "Check Naming" entry once the tool-menu system is ready.
FDelayedAutoRegisterHelper LinterRegistration(EDelayedRegisterRunPhase::EndOfEngineInit, []() { FTheAssetLinter::RegisterMenuEntry(); }, true);
}

bool FTheAssetLinter::IsExcludedPackagePath(const FString& PackagePath)
{
    // Engine/editor-managed or path-referenced content that the naming scheme does not govern.
    static const TCHAR* const Excluded[] = {TEXT("/Game/Splash"), TEXT("/Game/Developers"), TEXT("/Game/Collections"), TEXT("/Game/Localization"), TEXT("/Game/__ExternalActors__"), TEXT("/Game/__ExternalObjects__")};
    for(const TCHAR* const Prefix : Excluded)
    {
        if(PackagePath.StartsWith(Prefix))
        {
            return true;
        }
    }
    return false;
}

bool FTheAssetLinter::ExpectedPrefixFor(const FAssetData& Asset, FString& OutPrefix)
{
    const FString ClassName = Asset.AssetClassPath.GetAssetName().ToString();

    // Levels carry no prefix; redirectors are not real content.
    if(ClassName == TEXT("World") || ClassName == TEXT("ObjectRedirector"))
    {
        return false;
    }

    // Blueprint family: the prefix depends on the native parent (all are UBlueprint assets).
    if(ClassName == TEXT("WidgetBlueprint"))
    {
        OutPrefix = TEXT("WBP_");
        return true;
    }
    if(ClassName == TEXT("AnimBlueprint"))
    {
        OutPrefix = TEXT("ABP_");
        return true;
    }
    if(ClassName == TEXT("Blueprint"))
    {
        FString NativeParent;
        Asset.GetTagValue(FName(TEXT("NativeParentClass")), NativeParent);
        OutPrefix = NativeParent.Contains(TEXT("CommonInputBaseControllerData")) ? TEXT("CIBCD_") : NativeParent.Contains(TEXT("GameplayAbility")) ? TEXT("GA_") : NativeParent.Contains(TEXT("GameplayEffect")) ? TEXT("GE_") : TEXT("BP_");
        return true;
    }

    // Direct class-name → prefix map for the unambiguous kinds.
    struct FRule
    {
        const TCHAR* Class;
        const TCHAR* Prefix;
    };
    static const FRule Rules[] = {
        {TEXT("Material"), TEXT("M_")},
        {TEXT("MaterialInstanceConstant"), TEXT("MI_")},
        {TEXT("MaterialFunction"), TEXT("MF_")},
        {TEXT("MaterialFunctionInstance"), TEXT("MF_")},
        {TEXT("StaticMesh"), TEXT("SM_")},
        {TEXT("SkeletalMesh"), TEXT("SK_")},
        {TEXT("PhysicsAsset"), TEXT("PHYS_")},
        {TEXT("NiagaraSystem"), TEXT("NS_")},
        {TEXT("NiagaraEmitter"), TEXT("NE_")},
        {TEXT("SoundWave"), TEXT("S_")},
        {TEXT("SoundCue"), TEXT("SC_")},
        {TEXT("DataTable"), TEXT("DT_")},
        {TEXT("CurveTable"), TEXT("CT_")},
        {TEXT("InputAction"), TEXT("IA_")},
        {TEXT("InputMappingContext"), TEXT("MC_")},
    };
    for(const FRule& Rule : Rules)
    {
        if(ClassName == Rule.Class)
        {
            OutPrefix = Rule.Prefix;
            return true;
        }
    }

    // Textures are many concrete classes (Texture2D, TextureCube, ...) — all share the T_ prefix.
    if(ClassName.Contains(TEXT("Texture")) && !ClassName.Contains(TEXT("RenderTarget")))
    {
        OutPrefix = TEXT("T_");
        return true;
    }

    // Project custom asset kinds, matched by class name so no module dependency is needed. Persona is
    // checked first because its class name ("TheDialoguePersona") also contains "Dialogue".
    if(ClassName.Contains(TEXT("Persona")))
    {
        OutPrefix = TEXT("PER_");
        return true;
    }
    if(ClassName.Contains(TEXT("Dialogue")))
    {
        OutPrefix = TEXT("DLG_");
        return true;
    }

    // Unknown / prefix-less kind — do not flag (keeps false positives out).
    return false;
}

void FTheAssetLinter::GatherViolations(TArray<FViolation>& OutViolations, int32& OutChecked)
{
    OutChecked = 0;
    IAssetRegistry* const AssetRegistry = IAssetRegistry::Get();
    if(!AssetRegistry)
    {
        return;
    }

    TArray<FAssetData> Assets;
    AssetRegistry->GetAssetsByPath(FName(TEXT("/Game")), Assets, /*bRecursive*/ true);

    for(const FAssetData& Asset : Assets)
    {
        if(IsExcludedPackagePath(Asset.PackagePath.ToString()))
        {
            continue;
        }
        FString ExpectedPrefix;
        if(!ExpectedPrefixFor(Asset, ExpectedPrefix))
        {
            continue;
        }
        ++OutChecked;
        if(!Asset.AssetName.ToString().StartsWith(ExpectedPrefix))
        {
            OutViolations.Add({Asset, ExpectedPrefix});
        }
    }
}

void FTheAssetLinter::CheckAssetNaming()
{
    TArray<FViolation> Violations;
    int32 Checked = 0;
    GatherViolations(Violations, Checked);

    FMessageLog MessageLog(TheAssetLinter::MessageLogName);
    MessageLog.NewPage(LOCTEXT("LinterPage", "Asset naming check"));
    for(const FViolation& Violation : Violations)
    {
        // Load only the offending asset so the log entry can link straight to it in the Content Browser.
        const TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning);
        if(const auto Object = Violation.Asset.GetAsset())
        {
            Message->AddToken(FUObjectToken::Create(Object));
        }
        else
        {
            Message->AddToken(FTextToken::Create(FText::FromString(Violation.Asset.GetObjectPathString())));
        }
        Message->AddToken(FTextToken::Create(FText::Format(LOCTEXT("LinterExpectPrefix", "should start with \"{0}\" ({1})"), FText::FromString(Violation.ExpectedPrefix), FText::FromString(Violation.Asset.AssetClassPath.GetAssetName().ToString()))));
        MessageLog.AddMessage(Message);
    }

    FNotificationInfo Info(FText::GetEmpty());
    Info.ExpireDuration = 4.0f;
    Info.bUseSuccessFailIcons = true;
    if(Violations.Num() > 0)
    {
        Info.Text = FText::Format(LOCTEXT("LinterFound", "Asset naming: {0} of {1} named off-convention — see the Message Log (The -> Fix Naming to auto-add prefixes)."), FText::AsNumber(Violations.Num()), FText::AsNumber(Checked));
        MessageLog.Open(EMessageSeverity::Warning);
    }
    else
    {
        Info.Text = FText::Format(LOCTEXT("LinterClean", "Asset naming: all {0} prefixed assets follow the convention."), FText::AsNumber(Checked));
    }
    const auto Notification = FSlateNotificationManager::Get().AddNotification(Info);
    if(Notification)
    {
        Notification->SetCompletionState(Violations.Num() > 0 ? SNotificationItem::CS_Fail : SNotificationItem::CS_Success);
    }

    UE_LOG(LogTheStyler, Log, TEXT("Asset naming check: %d checked, %d off-convention."), Checked, Violations.Num());
}

void FTheAssetLinter::FixAssetNaming()
{
    TArray<FViolation> Violations;
    int32 Checked = 0;
    GatherViolations(Violations, Checked);

    if(Violations.Num() == 0)
    {
        FNotificationInfo Info(FText::Format(LOCTEXT("FixNone", "Asset naming: nothing to fix — all {0} assets already follow the convention."), FText::AsNumber(Checked)));
        Info.ExpireDuration = 4.0f;
        FSlateNotificationManager::Get().AddNotification(Info);
        return;
    }

    // Rename each off-convention asset to prepend its expected prefix. Reference fixup + redirector
    // creation is handled by IAssetTools::RenameAssets; a name collision leaves that one asset unchanged.
    TArray<FAssetRenameData> Renames;
    for(const FViolation& Violation : Violations)
    {
        if(const auto Object = Violation.Asset.GetAsset())
        {
            const FString NewName = Violation.ExpectedPrefix + Violation.Asset.AssetName.ToString();
            Renames.Emplace(Object, Violation.Asset.PackagePath.ToString(), NewName);
        }
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    AssetTools.RenameAssets(Renames);

    // Re-scan to count what actually got fixed (collisions or failures stay off-convention).
    TArray<FViolation> Remaining;
    int32 Rechecked = 0;
    GatherViolations(Remaining, Rechecked);
    const int32 Fixed = Violations.Num() - Remaining.Num();

    FNotificationInfo Info(FText::Format(LOCTEXT("FixDone", "Asset naming: renamed {0} of {1} off-convention assets."), FText::AsNumber(Fixed), FText::AsNumber(Violations.Num())));
    Info.ExpireDuration = 5.0f;
    Info.bUseSuccessFailIcons = true;
    const auto Notification = FSlateNotificationManager::Get().AddNotification(Info);
    if(Notification)
    {
        Notification->SetCompletionState(Remaining.Num() == 0 ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
    }
    UE_LOG(LogTheStyler, Log, TEXT("Asset naming fix: renamed %d of %d, %d still off-convention."), Fixed, Violations.Num(), Remaining.Num());

    if(Remaining.Num() > 0)
    {
        CheckAssetNaming(); // surface the ones that couldn't be renamed (e.g. name collisions)
    }
}

void FTheAssetLinter::RegisterMenuEntry()
{
    FToolMenuOwnerScoped OwnerScoped(TEXT("TheStyler"));

    UToolMenu* const Menu = UToolMenus::Get()->ExtendMenu(TheStyler::ContentBrowserMenuName);
    if(!Menu)
    {
        return;
    }

    FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("Lint"));
    Section.AddMenuEntry(TEXT("TheCheckAssetNaming"),
        LOCTEXT("CheckNamingLabel", "Check Naming"),
        LOCTEXT("CheckNamingTooltip", "Check that every /Game asset follows the prefix convention (_Docs/asset_structure.md); off-convention assets are listed (clickable) in the Message Log."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Search")),
        FUIAction(FExecuteAction::CreateStatic(&FTheAssetLinter::CheckAssetNaming)));
    Section.AddMenuEntry(TEXT("TheFixAssetNaming"),
        LOCTEXT("FixNamingLabel", "Fix Naming"),
        LOCTEXT("FixNamingTooltip", "Rename every off-convention /Game asset to prepend its expected prefix (references are fixed up automatically). Run Check Naming first to review."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Edit")),
        FUIAction(FExecuteAction::CreateStatic(&FTheAssetLinter::FixAssetNaming)));
}

#undef LOCTEXT_NAMESPACE
