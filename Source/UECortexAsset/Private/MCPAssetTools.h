#pragma once
#include "MCPToolBase.h"

class FMCPAssetTools : public FMCPToolModuleBase
{
public:
	virtual FString GetModuleName() const override { return TEXT("asset"); }
	virtual void RegisterTools(TArray<FMCPToolDef>& OutTools) override;

private:
	static FMCPToolResult AssetList(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AssetGetInfo(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AssetGetReferences(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AssetGetDependencies(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AssetMove(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AssetCopy(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AssetDelete(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AssetCreateFolder(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AssetListRedirectors(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AssetFixRedirectors(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AssetMigrate(const TSharedPtr<FJsonObject>& Params);
};
