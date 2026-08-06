// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTheStyler, Log, All);

namespace TheStyler
{
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
