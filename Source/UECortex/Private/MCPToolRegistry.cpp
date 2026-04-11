#include "MCPToolRegistry.h"
#include "UECortexModule.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FMCPToolRegistry& FMCPToolRegistry::Get()
{
	static FMCPToolRegistry Instance;
	return Instance;
}

FMCPToolRegistry::FMCPToolRegistry()
{
	RegisterBuiltins();
}

void FMCPToolRegistry::RegisterBuiltins()
{
	FMCPToolDef ListGroups;
	ListGroups.Name        = TEXT("list_tool_groups");
	ListGroups.Description = TEXT("List all tool categories with their tool names and counts. Use this to discover what's available before diving into tools/list.");
	ListGroups.Category    = TEXT("meta");
	ListGroups.Handler     = [this](const TSharedPtr<FJsonObject>&) -> FMCPToolResult
	{
		TSharedPtr<FJsonObject> Data = BuildGroupsList();
		int32 Total = 0;
		const TArray<TSharedPtr<FJsonValue>>* Groups;
		if (Data->TryGetArrayField(TEXT("groups"), Groups))
			for (auto& G : *Groups)
			{
				double Count = 0;
				G->AsObject()->TryGetNumberField(TEXT("count"), Count);
				Total += (int32)Count;
			}
		return FMCPToolResult::Success(
			FString::Printf(TEXT("%d tools across %d categories"), Total, Groups ? Groups->Num() : 0),
			Data);
	};
	Tools.Add(ListGroups);
}

void FMCPToolRegistry::RegisterModule(TSharedRef<FMCPToolModuleBase> Module)
{
	int32 Before = Tools.Num();
	Module->RegisterTools(Tools);
	// Auto-tag category for any newly added tools that don't have one set
	FString ModuleName = Module->GetModuleName();
	for (int32 i = Before; i < Tools.Num(); i++)
	{
		if (Tools[i].Category.IsEmpty())
			Tools[i].Category = ModuleName;
	}
	Modules.Add(Module);
	UE_LOG(LogUECortex, Log, TEXT("UECortex: Registered module '%s'"), *ModuleName);
}

void FMCPToolRegistry::UnregisterModule(const FString& ModuleName)
{
	Modules.RemoveAll([&](const TSharedRef<FMCPToolModuleBase>& M) {
		return M->GetModuleName() == ModuleName;
	});
	const FString Prefix = ModuleName + TEXT("_");
	Tools.RemoveAll([&](const FMCPToolDef& T) {
		return T.Name.StartsWith(Prefix);
	});
	UE_LOG(LogUECortex, Log, TEXT("UECortex: Unregistered module '%s'"), *ModuleName);
}

void FMCPToolRegistry::RegisterTool(const FMCPToolDef& Tool)
{
	// Replace if already registered
	for (FMCPToolDef& Existing : Tools)
	{
		if (Existing.Name == Tool.Name)
		{
			Existing = Tool;
			UE_LOG(LogUECortex, Log, TEXT("UECortex: Updated dynamic tool '%s'"), *Tool.Name);
			return;
		}
	}
	Tools.Add(Tool);
	UE_LOG(LogUECortex, Log, TEXT("UECortex: Registered dynamic tool '%s'"), *Tool.Name);
}

void FMCPToolRegistry::UnregisterTool(const FString& ToolName)
{
	int32 Removed = Tools.RemoveAll([&](const FMCPToolDef& T) { return T.Name == ToolName; });
	if (Removed > 0)
		UE_LOG(LogUECortex, Log, TEXT("UECortex: Unregistered dynamic tool '%s'"), *ToolName);
}

FMCPToolResult FMCPToolRegistry::CallTool(const FString& ToolName, const TSharedPtr<FJsonObject>& Args)
{
	for (const FMCPToolDef& Tool : Tools)
	{
		if (Tool.Name == ToolName)
		{
			if (!Tool.bEnabled)
			{
				return FMCPToolResult::Error(FString::Printf(
					TEXT("Tool '%s' is currently disabled."), *ToolName));
			}
			return Tool.Handler(Args);
		}
	}
	return FMCPToolResult::Error(FString::Printf(TEXT("Unknown tool: %s"), *ToolName));
}

TArray<TSharedPtr<FJsonObject>> FMCPToolRegistry::BuildToolsList() const
{
	TArray<TSharedPtr<FJsonObject>> Result;

	for (const FMCPToolDef& Tool : Tools)
	{
		if (!Tool.bEnabled) continue;

		auto ToolObj = MakeShared<FJsonObject>();
		ToolObj->SetStringField(TEXT("name"), Tool.Name);
		ToolObj->SetStringField(TEXT("description"), Tool.Description);

		// Build inputSchema
		auto Schema = MakeShared<FJsonObject>();
		Schema->SetStringField(TEXT("type"), TEXT("object"));

		auto Properties = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Required;

		for (const FMCPParamSchema& Param : Tool.Params)
		{
			auto PropObj = MakeShared<FJsonObject>();
			PropObj->SetStringField(TEXT("type"), Param.Type);
			PropObj->SetStringField(TEXT("description"), Param.Description);
			Properties->SetObjectField(Param.Name, PropObj);

			if (Param.bRequired)
			{
				Required.Add(MakeShared<FJsonValueString>(Param.Name));
			}
		}

		Schema->SetObjectField(TEXT("properties"), Properties);
		Schema->SetArrayField(TEXT("required"), Required);
		ToolObj->SetObjectField(TEXT("inputSchema"), Schema);

		Result.Add(ToolObj);
	}

	return Result;
}

bool FMCPToolRegistry::EnableTool(const FString& ToolName)
{
	for (FMCPToolDef& Tool : Tools)
	{
		if (Tool.Name == ToolName)
		{
			Tool.bEnabled = true;
			return true;
		}
	}
	return false;
}

bool FMCPToolRegistry::DisableTool(const FString& ToolName)
{
	for (FMCPToolDef& Tool : Tools)
	{
		if (Tool.Name == ToolName)
		{
			Tool.bEnabled = false;
			return true;
		}
	}
	return false;
}

bool FMCPToolRegistry::EnableCategory(const FString& CategoryName)
{
	bool bAny = false;
	for (FMCPToolDef& Tool : Tools)
	{
		if (Tool.Category.Equals(CategoryName, ESearchCase::IgnoreCase))
		{
			Tool.bEnabled = true;
			bAny = true;
		}
	}
	return bAny;
}

bool FMCPToolRegistry::DisableCategory(const FString& CategoryName)
{
	bool bAny = false;
	for (FMCPToolDef& Tool : Tools)
	{
		if (Tool.Category.Equals(CategoryName, ESearchCase::IgnoreCase))
		{
			Tool.bEnabled = false;
			bAny = true;
		}
	}
	return bAny;
}

void FMCPToolRegistry::ResetAll()
{
	for (FMCPToolDef& Tool : Tools)
	{
		Tool.bEnabled = true;
	}
}

int32 FMCPToolRegistry::GetEnabledToolCount() const
{
	int32 Count = 0;
	for (const FMCPToolDef& Tool : Tools)
	{
		if (Tool.bEnabled) Count++;
	}
	return Count;
}

TSharedPtr<FJsonObject> FMCPToolRegistry::BuildGroupsList() const
{
	// Collect tools by category (skip meta / built-ins from the groups list itself)
	TMap<FString, TArray<FString>> CategoryTools;
	for (const FMCPToolDef& Tool : Tools)
	{
		if (!Tool.bEnabled || Tool.Category == TEXT("meta")) continue;
		CategoryTools.FindOrAdd(Tool.Category).Add(Tool.Name);
	}

	// Sort categories alphabetically
	TArray<FString> SortedCategories;
	CategoryTools.GetKeys(SortedCategories);
	SortedCategories.Sort();

	TArray<TSharedPtr<FJsonValue>> GroupsArray;
	for (const FString& Cat : SortedCategories)
	{
		TArray<FString>& ToolNames = CategoryTools[Cat];
		ToolNames.Sort();

		TArray<TSharedPtr<FJsonValue>> NamesArray;
		for (const FString& N : ToolNames)
			NamesArray.Add(MakeShared<FJsonValueString>(N));

		auto GroupObj = MakeShared<FJsonObject>();
		GroupObj->SetStringField(TEXT("category"), Cat);
		GroupObj->SetNumberField(TEXT("count"), ToolNames.Num());
		GroupObj->SetArrayField(TEXT("tools"), NamesArray);
		GroupsArray.Add(MakeShared<FJsonValueObject>(GroupObj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("groups"), GroupsArray);
	return Result;
}

void FMCPToolRegistry::GetModuleStatus(TArray<FString>& OutActive, TArray<FString>& OutInactive) const
{
	for (const TSharedRef<FMCPToolModuleBase>& Module : Modules)
	{
		OutActive.Add(Module->GetModuleName());
	}
}
