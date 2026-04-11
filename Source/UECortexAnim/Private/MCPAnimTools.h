#pragma once
#include "MCPToolBase.h"

class FMCPAnimTools : public FMCPToolModuleBase
{
public:
	virtual FString GetModuleName() const override { return TEXT("anim"); }
	virtual void RegisterTools(TArray<FMCPToolDef>& OutTools) override;

private:
	static FMCPToolResult AnimListSkeletons(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimListAnimBlueprints(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimListSequences(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimListSkeletalMeshes(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimGetSkeletonBones(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimCreateAnimBlueprint(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimGetAnimGraph(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimAddStateMachine(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimAddState(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimCompileAnimBlueprint(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimFixSkeletonReference(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimSetStateAnimation(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimAddTransition(const TSharedPtr<FJsonObject>& Params);
	static FMCPToolResult AnimSetTransitionCondition(const TSharedPtr<FJsonObject>& Params);
};
