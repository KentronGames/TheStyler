#include "TheAssetLinter.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Framework/Commands/UIAction.h"
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

void FTheAssetLinter::CheckAssetNaming()
{
    IAssetRegistry* const AssetRegistry = IAssetRegistry::Get();
    if(!AssetRegistry)
    {
        return;
    }

    TArray<FAssetData> Assets;
    AssetRegistry->GetAssetsByPath(FName(TEXT("/Game")), Assets, /*bRecursive*/ true);

    FMessageLog MessageLog(TheAssetLinter::MessageLogName);
    MessageLog.NewPage(LOCTEXT("LinterPage", "Asset naming check"));

    int32 Checked = 0;
    int32 Violations = 0;
    for(const FAssetData& Asset : Assets)
    {
        const FString PackagePath = Asset.PackagePath.ToString();
        if(IsExcludedPackagePath(PackagePath))
        {
            continue;
        }

        FString ExpectedPrefix;
        if(!ExpectedPrefixFor(Asset, ExpectedPrefix))
        {
            continue;
        }
        ++Checked;

        const FString AssetName = Asset.AssetName.ToString();
        if(AssetName.StartsWith(ExpectedPrefix))
        {
            continue;
        }
        ++Violations;

        // Load only the offending asset so the log entry can link straight to it in the Content Browser.
        const TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning);
        if(const auto Object = Asset.GetAsset())
        {
            Message->AddToken(FUObjectToken::Create(Object));
        }
        else
        {
            Message->AddToken(FTextToken::Create(FText::FromString(Asset.GetObjectPathString())));
        }
        Message->AddToken(FTextToken::Create(FText::Format(LOCTEXT("LinterExpectPrefix", "should start with \"{0}\" ({1})"), FText::FromString(ExpectedPrefix), FText::FromString(Asset.AssetClassPath.GetAssetName().ToString()))));
        MessageLog.AddMessage(Message);
    }

    FNotificationInfo Info(FText::GetEmpty());
    Info.ExpireDuration = 4.0f;
    Info.bUseSuccessFailIcons = true;
    if(Violations > 0)
    {
        Info.Text = FText::Format(LOCTEXT("LinterFound", "Asset naming: {0} of {1} named off-convention — see the Message Log."), FText::AsNumber(Violations), FText::AsNumber(Checked));
        MessageLog.Open(EMessageSeverity::Warning);
    }
    else
    {
        Info.Text = FText::Format(LOCTEXT("LinterClean", "Asset naming: all {0} prefixed assets follow the convention."), FText::AsNumber(Checked));
    }
    const auto Notification = FSlateNotificationManager::Get().AddNotification(Info);
    if(Notification)
    {
        Notification->SetCompletionState(Violations > 0 ? SNotificationItem::CS_Fail : SNotificationItem::CS_Success);
    }

    UE_LOG(LogTheStyler, Log, TEXT("Asset naming check: %d checked, %d off-convention."), Checked, Violations);
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
}

#undef LOCTEXT_NAMESPACE
