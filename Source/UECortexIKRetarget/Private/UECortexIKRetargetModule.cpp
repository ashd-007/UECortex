#include "UECortexIKRetargetModule.h"
#include "MCPIKRetargetTools.h"
#include "MCPToolRegistry.h"
#include "Modules/ModuleManager.h"

void FUECortexIKRetargetModule::StartupModule()
{
	FMCPToolRegistry::Get().RegisterModule(MakeShared<FMCPIKRetargetTools>());
}

void FUECortexIKRetargetModule::ShutdownModule()
{
	FMCPToolRegistry::Get().UnregisterModule(TEXT("ikretarget"));
}

IMPLEMENT_MODULE(FUECortexIKRetargetModule, UECortexIKRetarget)
