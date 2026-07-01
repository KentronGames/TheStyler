#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTheStyler, Log, All);

struct FTheWireConnectionFactory;

class FTheStylerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();

    TSharedPtr<FTheWireConnectionFactory> WireFactory;
};
