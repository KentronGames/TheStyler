#include "TheFolderColorSync.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetViewUtils.h"
#include "ContentBrowserItemPath.h"
#include "ContentBrowserModule.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IContentBrowserSingleton.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "TheStylerModule.h"
#include "TheStylerSettings.h"

#define LOCTEXT_NAMESPACE "FTheFolderColorSync"

namespace TheFolderColorSync
{
constexpr int32 FileVersion = 1;
const TCHAR* const PathColorSection = TEXT("PathColor");
const TCHAR* const ColorsField = TEXT("Colors");
const TCHAR* const ColorField = TEXT("Color");
const TCHAR* const PathField = TEXT("Path");
const TCHAR* const VersionField = TEXT("Version");

// Apply saved colors and add the "Save Colors" menu entry once the editor UI is up.
FDelayedAutoRegisterHelper FolderColorRegistration(
    EDelayedRegisterRunPhase::EndOfEngineInit,
    []()
    {
        FTheFolderColorSync::ApplySavedFolderColors();
        FTheFolderColorSync::RegisterMenuEntry();
    },
    true);
}

void FTheFolderColorSync::ApplySavedFolderColors()
{
    if(!GetDefault<UTheStylerSettings>()->bEnableFolderColorSync)
    {
        return;
    }

    TMap<FString, FLinearColor> ProjectFolderColors;
    if(!LoadProjectFolderColors(ProjectFolderColors))
    {
        return;
    }

    const auto CurrentFolderColors = LoadEditorConfigFolderColors();
    const auto ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));

    for(const TPair<FString, FLinearColor>& CurrentFolderColor : CurrentFolderColors)
    {
        if(ProjectFolderColors.Contains(CurrentFolderColor.Key))
        {
            continue;
        }

        AssetViewUtils::SetPathColor(CurrentFolderColor.Key, TOptional<FLinearColor>());
        if(ContentBrowserModule)
        {
            ContentBrowserModule->GetOnSetFolderColor().Broadcast(CurrentFolderColor.Key);
        }
    }

    for(const TPair<FString, FLinearColor>& ProjectFolderColor : ProjectFolderColors)
    {
        AssetViewUtils::SetPathColor(ProjectFolderColor.Key, ProjectFolderColor.Value);
    }

    GConfig->Flush(false, GEditorPerProjectIni);
    UE_LOG(LogTheStyler, Log, TEXT("Applied %d saved editor folder colors."), ProjectFolderColors.Num());
}

void FTheFolderColorSync::SaveCurrentFolderColors()
{
    auto FolderColors = LoadEditorConfigFolderColors();

    // Drop colors remembered for folders that no longer exist on disk so the
    // project file does not accumulate stale entries over time.
    const auto RemovedCount = PruneMissingFolders(FolderColors);

    if(WriteProjectFolderColors(FolderColors))
    {
        const auto Message = RemovedCount > 0 ? FText::Format(LOCTEXT("SaveFolderColorsSuccessPruned", "Saved {0} folder colors (removed {1} for missing folders)."), FText::AsNumber(FolderColors.Num()), FText::AsNumber(RemovedCount))
                                              : FText::Format(LOCTEXT("SaveFolderColorsSuccess", "Saved {0} folder colors."), FText::AsNumber(FolderColors.Num()));
        Notify(Message, true);
        UE_LOG(LogTheStyler, Log, TEXT("Saved %d editor folder colors to %s (removed %d for missing folders)."), FolderColors.Num(), *GetFolderColorsFilePath(), RemovedCount);
        return;
    }

    Notify(LOCTEXT("SaveFolderColorsFailure", "Failed to save folder colors."), false);
    UE_LOG(LogTheStyler, Warning, TEXT("Failed to save editor folder colors to %s."), *GetFolderColorsFilePath());
}

void FTheFolderColorSync::RainbowCurrentFolder()
{
    const FContentBrowserItemPath CurrentPath = IContentBrowserSingleton::Get().GetCurrentPath();
    if(!CurrentPath.HasInternalPath())
    {
        Notify(LOCTEXT("RainbowNoFolder", "Open a project folder in the Content Browser first."), false);
        return;
    }
    const FString BasePath = CurrentPath.GetInternalPathString();

    IAssetRegistry* const AssetRegistry = IAssetRegistry::Get();
    if(!AssetRegistry)
    {
        Notify(LOCTEXT("RainbowNoRegistry", "Asset registry is unavailable."), false);
        return;
    }

    // Immediate subfolders only — do not recurse into their children.
    TArray<FString> SubPaths;
    AssetRegistry->GetSubPaths(BasePath, SubPaths, /*bRecurse*/ false);
    if(SubPaths.Num() == 0)
    {
        Notify(LOCTEXT("RainbowNoSubfolders", "The current folder has no subfolders to color."), false);
        return;
    }

    // Stable ordering so each folder keeps the same hue across runs.
    SubPaths.Sort();

    const auto ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));
    for(int32 FolderNdx = 0; FolderNdx < SubPaths.Num(); ++FolderNdx)
    {
        // Evenly spaced hues give the most visually distinct spread across the set.
        const float Hue = 360.0f * FolderNdx / SubPaths.Num();
        const FLinearColor Color = FLinearColor(Hue, 0.7f, 0.9f).HSVToLinearRGB();

        AssetViewUtils::SetPathColor(SubPaths[FolderNdx], TOptional<FLinearColor>(Color));
        if(ContentBrowserModule)
        {
            ContentBrowserModule->GetOnSetFolderColor().Broadcast(SubPaths[FolderNdx]);
        }
    }

    GConfig->Flush(false, GEditorPerProjectIni);

    // Persist to the project file exactly like "Save Colors" (it reads back the editor config we just wrote).
    SaveCurrentFolderColors();
}

void FTheFolderColorSync::ApplyStandardFolderColors()
{
    IAssetRegistry* const AssetRegistry = IAssetRegistry::Get();
    if(!AssetRegistry)
    {
        Notify(LOCTEXT("StandardNoRegistry", "Asset registry is unavailable."), false);
        return;
    }

    const auto& Rules = GetDefault<UTheStylerSettings>()->StandardFolderColors;
    if(Rules.Num() == 0)
    {
        Notify(LOCTEXT("StandardNoRules", "No standard folder colors are configured (Project Settings -> Plugins -> The Styler)."), false);
        return;
    }

    // Colors are matched by folder leaf name across the whole project — a key starting with "*" matches
    // by suffix (e.g. "*_Data"), exact names win over suffix rules.
    const auto FindColor = [&Rules](const FString& LeafName) -> const FLinearColor*
    {
        for(const TPair<FString, FLinearColor>& Rule : Rules)
        {
            if(!Rule.Key.StartsWith(TEXT("*")) && LeafName.Equals(Rule.Key, ESearchCase::IgnoreCase))
            {
                return &Rule.Value;
            }
        }
        for(const TPair<FString, FLinearColor>& Rule : Rules)
        {
            if(Rule.Key.StartsWith(TEXT("*")) && LeafName.EndsWith(Rule.Key.RightChop(1), ESearchCase::IgnoreCase))
            {
                return &Rule.Value;
            }
        }
        return nullptr;
    };

    // Every folder under /Game, recursively — but each is matched only by its own leaf name.
    TArray<FString> AllPaths;
    AssetRegistry->GetSubPaths(TEXT("/Game"), AllPaths, /*bRecurse*/ true);

    const auto ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));
    int32 ColoredCount = 0;
    for(const FString& Path : AllPaths)
    {
        FString LeafName = Path;
        int32 SlashNdx = INDEX_NONE;
        if(Path.FindLastChar(TEXT('/'), SlashNdx))
        {
            LeafName = Path.RightChop(SlashNdx + 1);
        }

        const FLinearColor* const Color = FindColor(LeafName);
        if(!Color)
        {
            continue;
        }

        AssetViewUtils::SetPathColor(Path, TOptional<FLinearColor>(*Color));
        if(ContentBrowserModule)
        {
            ContentBrowserModule->GetOnSetFolderColor().Broadcast(Path);
        }
        ++ColoredCount;
    }

    if(ColoredCount == 0)
    {
        Notify(LOCTEXT("StandardNoMatches", "No folders matched the standard color rules."), false);
        return;
    }

    GConfig->Flush(false, GEditorPerProjectIni);

    // Persist to the project file exactly like "Save Colors" (it reads back the editor config we just wrote).
    SaveCurrentFolderColors();
}

void FTheFolderColorSync::RegisterMenuEntry()
{
    FToolMenuOwnerScoped OwnerScoped(TEXT("TheStyler"));

    // Add "Save Colors" to the shared "The" dropdown in the Content Browser toolbar (owned by the
    // TheStyler module). Extending by name keeps this feature decoupled from the button's registration.
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TheStyler::ContentBrowserMenuName);
    if(!Menu)
    {
        return;
    }

    FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("FolderColors"));
    Section.AddMenuEntry(TEXT("TheSaveFolderColors"),
        LOCTEXT("SaveFolderColorsLabel", "Save Colors"),
        LOCTEXT("SaveFolderColorsTooltip", "Save Content Browser folder colors to the project file."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Save")),
        FUIAction(FExecuteAction::CreateStatic(&FTheFolderColorSync::SaveCurrentFolderColors)));
    Section.AddMenuEntry(TEXT("TheRainbowFolderColors"),
        LOCTEXT("RainbowFolderColorsLabel", "Rainbow Colors"),
        LOCTEXT("RainbowFolderColorsTooltip", "Color each subfolder of the current Content Browser folder a distinct hue and save them (this level only, not recursive)."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Adjust")),
        FUIAction(FExecuteAction::CreateStatic(&FTheFolderColorSync::RainbowCurrentFolder)));
    Section.AddMenuEntry(TEXT("TheStandardFolderColors"),
        LOCTEXT("StandardFolderColorsLabel", "Standard Colors"),
        LOCTEXT("StandardFolderColorsTooltip", "Color known structural folders (Meshes, Materials, *_Data, Textures, FX, ...) project-wide using the colors from Project Settings, and save them."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ContentBrowser.AssetTreeFolderClosed")),
        FUIAction(FExecuteAction::CreateStatic(&FTheFolderColorSync::ApplyStandardFolderColors)));
}

FString FTheFolderColorSync::GetFolderColorsFilePath()
{
    return FPaths::ProjectConfigDir() / TEXT("EditorFolderColors.json");
}

TMap<FString, FLinearColor> FTheFolderColorSync::LoadEditorConfigFolderColors()
{
    TMap<FString, FLinearColor> FolderColors;

    TArray<FString> Section;
    if(!GConfig->GetSection(TheFolderColorSync::PathColorSection, Section, GEditorPerProjectIni))
    {
        return FolderColors;
    }

    for(auto Entry : Section)
    {
        Entry.TrimStartAndEndInline();

        FString Path;
        FString ColorString;
        if(!Entry.Split(TEXT("="), &Path, &ColorString))
        {
            continue;
        }

        Path.TrimStartAndEndInline();
        ColorString.TrimStartAndEndInline();

        FLinearColor Color;
        if(Color.InitFromString(ColorString) && !Color.Equals(AssetViewUtils::GetDefaultColor()))
        {
            FolderColors.Add(Path, Color);
        }
    }

    FolderColors.KeySort([](const FString& Left, const FString& Right) { return Left < Right; });
    return FolderColors;
}

int32 FTheFolderColorSync::PruneMissingFolders(TMap<FString, FLinearColor>& FolderColors)
{
    TArray<FString> StalePaths;
    for(const TPair<FString, FLinearColor>& FolderColor : FolderColors)
    {
        if(!DoesFolderExistOnDisk(FolderColor.Key))
        {
            StalePaths.Add(FolderColor.Key);
        }
    }

    for(const auto& StalePath : StalePaths)
    {
        FolderColors.Remove(StalePath);
        UE_LOG(LogTheStyler, Log, TEXT("Dropped remembered folder color for missing folder %s."), *StalePath);
    }

    return StalePaths.Num();
}

bool FTheFolderColorSync::DoesFolderExistOnDisk(const FString& FolderPath)
{
    // Resolve the content-browser folder path (e.g. "/Game/Foo") to an on-disk
    // directory. Paths that cannot be resolved to a filesystem location
    // (virtual roots, unmounted mount points) are treated as existing so we
    // never drop a color we cannot positively verify as stale.
    FString DiskPath;
    if(!FPackageName::TryConvertLongPackageNameToFilename(FolderPath, DiskPath))
    {
        return true;
    }

    return IFileManager::Get().DirectoryExists(*DiskPath);
}

bool FTheFolderColorSync::LoadProjectFolderColors(TMap<FString, FLinearColor>& OutFolderColors)
{
    OutFolderColors.Reset();

    FString JsonText;
    if(!FFileHelper::LoadFileToString(JsonText, *GetFolderColorsFilePath()))
    {
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    const auto Reader = TJsonReaderFactory<>::Create(JsonText);
    if(!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTheStyler, Warning, TEXT("Editor folder colors file is not valid JSON: %s."), *GetFolderColorsFilePath());
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* ColorValues = nullptr;
    if(!RootObject->TryGetArrayField(TheFolderColorSync::ColorsField, ColorValues))
    {
        return true;
    }

    for(const auto& ColorValue : *ColorValues)
    {
        const auto ColorObject = ColorValue.IsValid() ? ColorValue->AsObject() : nullptr;
        if(!ColorObject.IsValid())
        {
            continue;
        }

        FString Path;
        FString ColorString;
        if(!ColorObject->TryGetStringField(TheFolderColorSync::PathField, Path) || !ColorObject->TryGetStringField(TheFolderColorSync::ColorField, ColorString))
        {
            continue;
        }

        FLinearColor Color;
        if(Color.InitFromString(ColorString) && !Color.Equals(AssetViewUtils::GetDefaultColor()))
        {
            OutFolderColors.Add(Path, Color);
        }
    }

    return true;
}

bool FTheFolderColorSync::WriteProjectFolderColors(const TMap<FString, FLinearColor>& FolderColors)
{
    TArray<FString> SortedPaths;
    FolderColors.GetKeys(SortedPaths);
    SortedPaths.Sort();

    TArray<TSharedPtr<FJsonValue>> ColorValues;
    ColorValues.Reserve(SortedPaths.Num());

    for(const auto& Path : SortedPaths)
    {
        const auto Color = FolderColors.Find(Path);
        if(!Color)
        {
            continue;
        }

        const auto ColorObject = MakeShared<FJsonObject>();
        ColorObject->SetStringField(TheFolderColorSync::PathField, Path);
        ColorObject->SetStringField(TheFolderColorSync::ColorField, Color->ToString());
        ColorValues.Add(MakeShared<FJsonValueObject>(ColorObject));
    }

    const auto RootObject = MakeShared<FJsonObject>();
    RootObject->SetNumberField(TheFolderColorSync::VersionField, TheFolderColorSync::FileVersion);
    RootObject->SetArrayField(TheFolderColorSync::ColorsField, ColorValues);

    FString JsonText;
    const auto Writer = TJsonWriterFactory<>::Create(&JsonText);
    if(!FJsonSerializer::Serialize(RootObject, Writer))
    {
        return false;
    }

    return FFileHelper::SaveStringToFile(JsonText, *GetFolderColorsFilePath());
}

void FTheFolderColorSync::Notify(const FText& Message, bool bSuccess)
{
    FNotificationInfo Info(Message);
    Info.ExpireDuration = 3.0f;
    Info.bUseSuccessFailIcons = true;

    const auto Notification = FSlateNotificationManager::Get().AddNotification(Info);
    if(Notification)
    {
        Notification->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
    }
}

#undef LOCTEXT_NAMESPACE
