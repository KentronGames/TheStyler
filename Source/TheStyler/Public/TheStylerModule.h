#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTheStyler, Log, All);

namespace TheStyler
{
/**
 * UToolMenus name of the "The" dropdown in the Content Browser toolbar — a shared home for The* editor
 * commands. Any module can add entries by calling UToolMenus::ExtendMenu(TheStyler::ContentBrowserMenuName);
 * the menu is decoupled from its extenders, so no code dependency on this plugin is needed.
 */
inline constexpr const TCHAR* ContentBrowserMenuName = TEXT("The.ContentBrowserMenu");
}

struct FTheWireConnectionFactory;
class FTheFormatOnConnect;

class FTheStylerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void RegisterContentBrowserMenu();

    TSharedPtr<FTheWireConnectionFactory> WireFactory;
    TUniquePtr<FTheFormatOnConnect> FormatOnConnect;
};
