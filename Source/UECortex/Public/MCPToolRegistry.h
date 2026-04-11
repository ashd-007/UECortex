#pragma once

#include "CoreMinimal.h"
#include "MCPToolBase.h"

// Central registry — holds all tools, routes calls, handles enable/disable
class UECORTEX_API FMCPToolRegistry
{
public:
	static FMCPToolRegistry& Get();

	// Register a tool module (call during StartupModule)
	void RegisterModule(TSharedRef<FMCPToolModuleBase> Module);

	// Unregister a tool module by name (call during ShutdownModule of submodules)
	void UnregisterModule(const FString& ModuleName);

	// Register / unregister individual tools (for dynamic Python tools)
	void RegisterTool(const FMCPToolDef& Tool);
	void UnregisterTool(const FString& ToolName);

	// Dispatch a tool call by name
	FMCPToolResult CallTool(const FString& ToolName, const TSharedPtr<FJsonObject>& Args);

	// Build tools/list response array
	TArray<TSharedPtr<FJsonObject>> BuildToolsList() const;

	// Build list_tool_groups response — categories with counts and tool names
	TSharedPtr<FJsonObject> BuildGroupsList() const;

	// Dynamic enable/disable
	bool EnableTool(const FString& ToolName);
	bool DisableTool(const FString& ToolName);
	bool EnableCategory(const FString& CategoryName);
	bool DisableCategory(const FString& CategoryName);
	void ResetAll();

	// Health info
	void GetModuleStatus(TArray<FString>& OutActive, TArray<FString>& OutInactive) const;
	int32 GetEnabledToolCount() const;

private:
	FMCPToolRegistry();
	void RegisterBuiltins();

	TArray<TSharedRef<FMCPToolModuleBase>> Modules;
	TArray<FMCPToolDef> Tools;
};
