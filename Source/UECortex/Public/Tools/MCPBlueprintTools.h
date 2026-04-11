#pragma once

#include "CoreMinimal.h"
#include "MCPToolBase.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

class UECORTEX_API FMCPBlueprintTools : public FMCPToolModuleBase
{
public:
	virtual FString GetModuleName() const override { return TEXT("blueprint"); }
	virtual void RegisterTools(TArray<FMCPToolDef>& OutTools) override;

private:
	static FMCPToolResult BlueprintCreate(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintAddComponent(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintSetComponentProperty(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintAddVariable(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintSetVariable(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintAddFunction(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintAddEventNode(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintAddFunctionCall(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintConnectPins(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintCompile(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintSpawnInLevel(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintGetGraph(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintFindNode(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintSetNodeProperty(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintCreateInputAction(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintAddGameMode(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintSetCDOProperty(const TSharedPtr<FJsonObject>& Args);
	static FMCPToolResult BlueprintSetInheritedComponentProperty(const TSharedPtr<FJsonObject>& Args);

	// Internal helpers
	static UBlueprint*    LoadBP(const FString& Path);
	static UEdGraph*      FindGraph(UBlueprint* BP, const FString& GraphName);
	static UEdGraphNode*  FindNodeByName(UEdGraph* Graph, const FString& Name);
};
