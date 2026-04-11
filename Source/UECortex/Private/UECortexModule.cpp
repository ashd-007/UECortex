#include "UECortexModule.h"
#include "MCPHttpServer.h"
#include "MCPToolRegistry.h"
#include "Tools/MCPLevelTools.h"
#include "Tools/MCPSplineTools.h"
#include "Tools/MCPVolumeTools.h"
#include "Tools/MCPBlueprintTools.h"
#include "Tools/MCPCppCodegenTools.h"
#include "Tools/MCPRenderingTools.h"
#include "Tools/MCPViewportCaptureTools.h"
#include "Tools/MCPEditorInputTools.h"
#include "Tools/MCPFabTools.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogUECortex);

#define LOCTEXT_NAMESPACE "FUECortexModule"

void FUECortexModule::StartupModule()
{
	UE_LOG(LogUECortex, Log, TEXT("UECortex: Starting up"));

	auto& Registry = FMCPToolRegistry::Get();

	// --- Core tool modules — always registered ---
	Registry.RegisterModule(MakeShared<FMCPLevelTools>());
	Registry.RegisterModule(MakeShared<FMCPViewportCaptureTools>());
	Registry.RegisterModule(MakeShared<FMCPEditorInputTools>());
	Registry.RegisterModule(MakeShared<FMCPFabTools>());
	Registry.RegisterModule(MakeShared<FMCPSplineTools>());
	Registry.RegisterModule(MakeShared<FMCPVolumeTools>());
	Registry.RegisterModule(MakeShared<FMCPBlueprintTools>());

	// PCG tools — loaded only if PCG plugin is already present in this project
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PCG")))
	{
		FModuleManager::Get().LoadModule(TEXT("UECortexPCG"));
	}

	Registry.RegisterModule(MakeShared<FMCPCppCodegenTools>());

	// GAS tools — loaded only if GameplayAbilities is already present in this project
	if (FModuleManager::Get().IsModuleLoaded(TEXT("GameplayAbilities")))
	{
		FModuleManager::Get().LoadModule(TEXT("UECortexGAS"));
	}

	// Niagara tools — loaded only if Niagara is present
	if (FModuleManager::Get().IsModuleLoaded(TEXT("Niagara")))
	{
		FModuleManager::Get().LoadModule(TEXT("UECortexNiagara"));
	}

	// UMG tools — loaded only if UMG is present
	if (FModuleManager::Get().IsModuleLoaded(TEXT("UMG")))
	{
		FModuleManager::Get().LoadModule(TEXT("UECortexUMG"));
	}

	// Animation tools — AnimGraph is always present in editor builds
	if (FModuleManager::Get().IsModuleLoaded(TEXT("AnimGraph")))
	{
		FModuleManager::Get().LoadModule(TEXT("UECortexAnim"));
	}

	// Asset management tools — always available
	FModuleManager::Get().LoadModule(TEXT("UECortexAsset"));

	Registry.RegisterModule(MakeShared<FMCPRenderingTools>());

	// Sequencer tools — LevelSequence is a standard enabled plugin in all UE projects
	if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelSequence")))
	{
		FModuleManager::Get().LoadModule(TEXT("UECortexSequencer"));
	}

	// Python executor + dynamic tool registry
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PythonScriptPlugin")))
	{
		FModuleManager::Get().LoadModule(TEXT("UECortexPython"));
	}

	// IK Retarget tools — loaded only if IKRig plugin is present
	if (FModuleManager::Get().IsModuleLoaded(TEXT("IKRig")))
	{
		FModuleManager::Get().LoadModule(TEXT("UECortexIKRetarget"));
	}

	// ── Startup summary ──────────────────────────────────────────────────────
	{
		TArray<FString> ActiveModules, InactiveModules;
		Registry.GetModuleStatus(ActiveModules, InactiveModules);

		UE_LOG(LogUECortex, Log, TEXT("┌─────────────────────────────────────────┐"));
		UE_LOG(LogUECortex, Log, TEXT("│              UECortex Ready              │"));
		UE_LOG(LogUECortex, Log, TEXT("├─────────────────────────────────────────┤"));
		UE_LOG(LogUECortex, Log, TEXT("│  Tools registered : %3d                  │"), Registry.GetEnabledToolCount());
		UE_LOG(LogUECortex, Log, TEXT("│  Active modules   : %3d                  │"), ActiveModules.Num());
		UE_LOG(LogUECortex, Log, TEXT("│  Inactive modules : %3d                  │"), InactiveModules.Num());
		UE_LOG(LogUECortex, Log, TEXT("├─────────────────────────────────────────┤"));
		for (const FString& M : ActiveModules)
			UE_LOG(LogUECortex, Log, TEXT("│  [ON]  %-33s │"), *M);
		for (const FString& M : InactiveModules)
			UE_LOG(LogUECortex, Log, TEXT("│  [OFF] %-33s │"), *M);
		UE_LOG(LogUECortex, Log, TEXT("├─────────────────────────────────────────┤"));
		UE_LOG(LogUECortex, Log, TEXT("│  http://localhost:7777/mcp               │"));
		UE_LOG(LogUECortex, Log, TEXT("│  GET  /health  — status + tool count     │"));
		UE_LOG(LogUECortex, Log, TEXT("│  POST /mcp     — JSON-RPC 2.0            │"));
		UE_LOG(LogUECortex, Log, TEXT("└─────────────────────────────────────────┘"));
	}

	// Start HTTP server — AFTER all tools are registered
	HttpServer = MakeUnique<FMCPHttpServer>(7777);
	HttpServer->Start();
}

void FUECortexModule::ShutdownModule()
{
	if (HttpServer)
	{
		HttpServer->Stop();
		HttpServer.Reset();
	}
	UE_LOG(LogUECortex, Log, TEXT("UECortex: Shutdown complete"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUECortexModule, UECortex)
