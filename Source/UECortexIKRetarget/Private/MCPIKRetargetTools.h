#pragma once
#include "MCPToolBase.h"

class FMCPIKRetargetTools : public FMCPToolModuleBase
{
public:
	virtual FString GetModuleName() const override { return TEXT("ikretarget"); }
	virtual void RegisterTools(TArray<FMCPToolDef>& OutTools) override;

private:
	static FMCPToolResult IKRetargetCreate(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult IKRigAssignMesh(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult IKRigSetRetargetRoot(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult IKRigAddRetargetChain(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult IKRigRemoveRetargetChain(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimRetargetBatch(const TSharedPtr<FJsonObject>& Params);
};
