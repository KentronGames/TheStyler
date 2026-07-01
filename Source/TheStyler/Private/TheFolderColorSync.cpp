#include "TheFolderColorSync.h"

#include "AssetViewUtils.h"
#include "ContentBrowserModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "SActionButton.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "TheStylerModule.h"

#define LOCTEXT_NAMESPACE "FTheFolderColorSync"

namespace TheFolderColorSync
{
constexpr int32 FileVersion = 1;
const TCHAR* const PathColorSection = TEXT("PathColor");
const TCHAR* const ColorsField = TEXT("Colors");
const TCHAR* const ColorField = TEXT("Color");
const TCHAR* const PathField = TEXT("Path");
const TCHAR* const VersionField = TEXT("Version");

// Apply saved colors and add the toolbar button once the editor UI is up.
FDelayedAutoRegisterHelper FolderColorRegistration(
    EDelayedRegisterRunPhase::EndOfEngineInit,
    []()
    {
        FTheFolderColorSync::ApplySavedFolderColors();
        FTheFolderColorSync::RegisterToolbarButton();
    },
    true);
}

void FTheFolderColorSync::ApplySavedFolderColors()
{
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

void FTheFolderColorSync::RegisterToolbarButton()
{
    FToolMenuOwnerScoped OwnerScoped(TEXT("TheStyler"));

    auto AddSaveFolderColorsButton = [](UToolMenu* ToolMenu, const FName SectionName, const FName EntryName)
    {
        if(!ToolMenu)
        {
            return;
        }

        // Build the entry as an SActionButton so it matches the native Content Browser toolbar
        // buttons ("Save All", "Add", ...). A plain InitToolBarButton uses the generic toolbar
        // button block, which renders with different padding/colour and looks out of place here.
        const TSharedRef<SActionButton> SaveButton = SNew(SActionButton)
                                                         .ToolTipText(LOCTEXT("SaveFolderColorsTooltip", "Save Content Browser folder colors to the project file."))
                                                         .OnClicked_Lambda(
                                                             []()
                                                             {
                                                                 FTheFolderColorSync::SaveCurrentFolderColors();
                                                                 return FReply::Handled();
                                                             })
                                                         .Icon(FAppStyle::Get().GetBrush("Icons.Save"))
                                                         .Text(LOCTEXT("SaveFolderColorsLabel", "Save Colors"));

        auto& Section = ToolMenu->FindOrAddSection(SectionName);
        Section.AddEntry(FToolMenuEntry::InitWidget(EntryName,
            SaveButton,
            FText::GetEmpty(),
            /*bNoIndent*/ true,
            /*bSearchable*/ false));
    };

    // Content Browser toolbar only — the level-editor top toolbar keeps just the Run button.
    AddSaveFolderColorsButton(UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.ToolBar")), TEXT("Save"), TEXT("TheSaveFolderColors"));
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
