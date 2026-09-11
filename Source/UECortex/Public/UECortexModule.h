#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

// Custom log category — visible in the UE Output Log by default
DECLARE_LOG_CATEGORY_EXTERN(LogUECortex, Log, All);

// Version shorthand macros
#define UE_MCP_VERSION_5_7_PLUS (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
#define UE_MCP_VERSION_5_6_ONLY (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 6)

class FMCPHttpServer;

class FUECortexModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FUECortexModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUECortexModule>("UECortex");
	}

	/// The world every MCP tool should operate against: the editor world normally, but the live
	/// PIE world while a Play session is active. Every tool previously re-resolved
	/// GEditor->GetEditorWorldContext().World() independently, which is always the editor world
	/// and never updates when Play starts -- this single tracked pointer (kept current via
	/// OnWorldPostInitialization/OnWorldCleanup, mirroring the GameDriver plugin's own
	/// ChangeWorld pattern) is the fix. Falls back to the editor world context if nothing has
	/// been tracked yet (e.g. tool called before any world init delegate has fired).
	static UWorld* GetActiveWorld()
	{
		FUECortexModule& Module = Get();
		if (Module.TrackedWorld)
		{
			return Module.TrackedWorld;
		}
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

private:
	void OnWorldPostInitialization(UWorld* World, const UWorld::InitializationValues IVS);
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	TUniquePtr<FMCPHttpServer> HttpServer;
	UWorld* TrackedWorld = nullptr;
	FDelegateHandle OnWorldPostInitHandle;
	FDelegateHandle OnWorldCleanupHandle;
};
