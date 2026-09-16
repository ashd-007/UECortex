#include "MCPPythonTools.h"
#include "MCPToolRegistry.h"
#include "IPythonScriptPlugin.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// ---------------------------------------------------------------------------
// NOTE: HTTP handlers are called from the game thread tick (FHttpServerModule::ProcessRequests).
// Do NOT use AsyncTask(GameThread) + FEvent — deadlocks. Call UE APIs directly.
// ---------------------------------------------------------------------------

// --------------- Dynamic tool registry ------------------------------------

struct FPythonToolEntry
{
	FString Name;
	FString Description;
	FString Code; // Python code; receives params via _mcp_params JSON string, sets _mcp_result
};

static TMap<FString, FPythonToolEntry> GPythonTools;

// --------------- Helpers --------------------------------------------------

static IPythonScriptPlugin* GetPython()
{
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("PythonScriptPlugin")))
		return nullptr;
	IPythonScriptPlugin* P = &FModuleManager::GetModuleChecked<IPythonScriptPlugin>(TEXT("PythonScriptPlugin"));
	return (P && P->IsPythonAvailable()) ? P : nullptr;
}

static FString CollectOutput(FPythonCommandEx& Cmd)
{
	TArray<FString> Lines;
	for (const FPythonLogOutputEntry& L : Cmd.LogOutput)
		Lines.Add(L.Output);
	FString Output = FString::Join(Lines, TEXT("\n")).TrimEnd();
	if (!Cmd.CommandResult.IsEmpty())
	{
		if (!Output.IsEmpty()) Output += TEXT("\n");
		Output += Cmd.CommandResult;
	}
	return Output.IsEmpty() ? TEXT("(no output)") : Output;
}

// Run a single-line expression via EvaluateStatement (returns its value)
static FString EvalPython(const FString& Expr)
{
	IPythonScriptPlugin* P = GetPython();
	if (!P) return TEXT("[Python not available]");
	FPythonCommandEx Cmd;
	Cmd.Command = Expr;
	Cmd.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
	P->ExecPythonCommandEx(Cmd);
	return CollectOutput(Cmd);
}

// Run arbitrary (possibly multi-line) code via a temp file
static FString RunPython(const FString& Code)
{
	IPythonScriptPlugin* P = GetPython();
	if (!P) return TEXT("[Python not available]");

	// Write to a temp file so multi-line code works
	FString TempPath = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("UECortexPythonTemp.py"));
	FFileHelper::SaveStringToFile(Code, *TempPath);

	FPythonCommandEx Cmd;
	Cmd.Command = TempPath;
	Cmd.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Cmd.FileExecutionScope = EPythonFileExecutionScope::Public; // variables persist in global scope
	P->ExecPythonCommandEx(Cmd);

	return CollectOutput(Cmd);
}

static FMCPToolResult ExecutePythonTool(const FPythonToolEntry& Entry, const TSharedPtr<FJsonObject>& Params)
{
	IPythonScriptPlugin* P = GetPython();
	if (!P) return FMCPToolResult::Error(TEXT("Python not available"));

	// Write params to a separate temp file to avoid any string escaping issues
	FString ParamsJson;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ParamsJson);
	FJsonSerializer::Serialize(Params.ToSharedRef(), Writer);

	FString ParamsPath = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("UECortexPythonParams.json"));
	FFileHelper::SaveStringToFile(ParamsJson, *ParamsPath);
	// Normalize slashes for Python
	ParamsPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	FString BootCode = FString::Printf(TEXT(
		"import json\n"
		"with open(r'%s') as _f:\n"
		"    _mcp_params = json.load(_f)\n"
		"_mcp_result = ''\n"
		"%s\n"
		"if _mcp_result:\n"
		"    print(_mcp_result)\n"
	), *ParamsPath, *Entry.Code);

	FString Output = RunPython(BootCode);
	return FMCPToolResult::Success(Output);
}

// --------------- RegisterTools --------------------------------------------

void FMCPPythonTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	{
		FMCPToolDef T;
		T.Name = TEXT("python_exec");
		T.Description = TEXT("Execute Python code in the UE Python environment. Returns stdout + result. Use 'import unreal' to access UE APIs.");
		T.Params.Add(FMCPParamSchema(TEXT("code"), TEXT("string"), TEXT("Python source code to execute"), true));
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return PythonExec(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("python_exec_file");
		T.Description = TEXT("Execute a Python file in the UE Python environment.");
		T.Params.Add(FMCPParamSchema(TEXT("file_path"), TEXT("string"), TEXT("Absolute or project-relative path to the Python file to execute"), true));
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return PythonExecFile(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("python_check");
		T.Description = TEXT("Check Python availability and return version info.");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return PythonCheck(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("python_set_var");
		T.Description = TEXT("Set a global Python variable.");
		T.Params.Add(FMCPParamSchema(TEXT("name"),  TEXT("string"), TEXT("Variable name"), true));
		T.Params.Add(FMCPParamSchema(TEXT("value"), TEXT("string"), TEXT("Any Python literal or expression"), true));
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return PythonSetVar(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("python_get_var");
		T.Description = TEXT("Get the string representation of a global Python variable.");
		T.Params.Add(FMCPParamSchema(TEXT("name"), TEXT("string"), TEXT("Variable name"), true));
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return PythonGetVar(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("python_list_scripts");
		T.Description = TEXT("List .py files in a directory.");
		T.Params.Add(FMCPParamSchema(TEXT("path"), TEXT("string"), TEXT("Directory to list (default: project dir)"), false));
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return PythonListScripts(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("python_register_tool");
		T.Description = TEXT(
			"Register a new MCP tool backed by Python code. The code runs with 'import json' and '_mcp_params' dict already set. "
			"Set '_mcp_result' string to return a value.");
		T.Params.Add(FMCPParamSchema(TEXT("name"),        TEXT("string"), TEXT("Tool name to register"), true));
		T.Params.Add(FMCPParamSchema(TEXT("description"), TEXT("string"), TEXT("Tool description"), true));
		T.Params.Add(FMCPParamSchema(TEXT("code"),        TEXT("string"), TEXT("Python source code for the tool body"), true));
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return PythonRegisterTool(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("python_unregister_tool");
		T.Description = TEXT("Remove a dynamically registered Python tool.");
		T.Params.Add(FMCPParamSchema(TEXT("name"), TEXT("string"), TEXT("Name of the tool to remove"), true));
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return PythonUnregisterTool(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("python_list_dynamic_tools");
		T.Description = TEXT("List all currently registered dynamic Python tools.");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return PythonListDynamicTools(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("python_reload_tools_from_file");
		T.Description = TEXT(
			"Load tool definitions from a JSON file and register them all. "
			"File format: array of {name, description, code} objects.");
		T.Params.Add(FMCPParamSchema(TEXT("file_path"), TEXT("string"), TEXT("Path to the JSON file containing tool definitions"), true));
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return PythonReloadToolsFromFile(P); };
		OutTools.Add(T);
	}

	// Re-register any tools that survived a module reload
	for (auto& Pair : GPythonTools)
	{
		FMCPToolDef DynT;
		const FPythonToolEntry& Entry = Pair.Value;
		DynT.Name    = Entry.Name;
		DynT.Description = Entry.Description;
		DynT.Handler = [Name = Entry.Name](const TSharedPtr<FJsonObject>& P) -> FMCPToolResult
		{
			const FPythonToolEntry* E = GPythonTools.Find(Name);
			if (!E) return FMCPToolResult::Error(FString::Printf(TEXT("Python tool '%s' not found"), *Name));
			return ExecutePythonTool(*E, P);
		};
		OutTools.Add(DynT);
	}
}

// --------------- Tool implementations ------------------------------------

FMCPToolResult FMCPPythonTools::PythonExec(const TSharedPtr<FJsonObject>& Params)
{
	FString Code;
	if (!Params->TryGetStringField(TEXT("code"), Code))
		return FMCPToolResult::Error(TEXT("Missing 'code'"));

	IPythonScriptPlugin* P = GetPython();
	if (!P) return FMCPToolResult::Error(TEXT("Python is not available in this project. Enable the PythonScriptPlugin."));

	FString Output = RunPython(Code);
	return FMCPToolResult::Success(Output);
}

FMCPToolResult FMCPPythonTools::PythonExecFile(const TSharedPtr<FJsonObject>& Params)
{
	FString FilePath;
	if (!Params->TryGetStringField(TEXT("file_path"), FilePath))
		return FMCPToolResult::Error(TEXT("Missing 'file_path'"));

	IPythonScriptPlugin* P = GetPython();
	if (!P) return FMCPToolResult::Error(TEXT("Python not available"));

	// Resolve relative paths against project dir
	if (FPaths::IsRelative(FilePath))
		FilePath = FPaths::Combine(FPaths::ProjectDir(), FilePath);

	if (!FPaths::FileExists(FilePath))
		return FMCPToolResult::Error(FString::Printf(TEXT("File not found: %s"), *FilePath));

	FPythonCommandEx Cmd;
	Cmd.Command  = FilePath;
	Cmd.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	P->ExecPythonCommandEx(Cmd);

	TArray<FString> Lines;
	for (const FPythonLogOutputEntry& L : Cmd.LogOutput)
		Lines.Add(L.Output);

	FString Output = FString::Join(Lines, TEXT("\n")).TrimEnd();
	return FMCPToolResult::Success(Output.IsEmpty() ? TEXT("(no output)") : Output);
}

FMCPToolResult FMCPPythonTools::PythonCheck(const TSharedPtr<FJsonObject>& Params)
{
	IPythonScriptPlugin* P = GetPython();
	if (!P) return FMCPToolResult::Error(TEXT("Python not available — enable PythonScriptPlugin in your project"));

	FString Version = RunPython(TEXT("import sys\nprint(sys.version)"));
	FString UnrealTest = RunPython(TEXT("import unreal\nprint('unreal module OK:', unreal.SystemLibrary.get_engine_version())"));

	return FMCPToolResult::Success(FString::Printf(TEXT("Python available.\n%s\n%s"), *Version, *UnrealTest));
}

FMCPToolResult FMCPPythonTools::PythonSetVar(const TSharedPtr<FJsonObject>& Params)
{
	FString Name, Value;
	if (!Params->TryGetStringField(TEXT("name"), Name))
		return FMCPToolResult::Error(TEXT("Missing 'name'"));
	if (!Params->TryGetStringField(TEXT("value"), Value))
		return FMCPToolResult::Error(TEXT("Missing 'value'"));

	IPythonScriptPlugin* P = GetPython();
	if (!P) return FMCPToolResult::Error(TEXT("Python not available"));

	FString Code = FString::Printf(TEXT("%s = %s"), *Name, *Value);
	RunPython(Code);
	return FMCPToolResult::Success(FString::Printf(TEXT("Set %s = %s"), *Name, *Value));
}

FMCPToolResult FMCPPythonTools::PythonGetVar(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
		return FMCPToolResult::Error(TEXT("Missing 'name'"));

	IPythonScriptPlugin* P = GetPython();
	if (!P) return FMCPToolResult::Error(TEXT("Python not available"));

	FString Result = RunPython(FString::Printf(TEXT("print(str(%s))"), *Name));
	return FMCPToolResult::Success(FString::Printf(TEXT("%s = %s"), *Name, *Result));
}

FMCPToolResult FMCPPythonTools::PythonListScripts(const TSharedPtr<FJsonObject>& Params)
{
	FString SearchPath = FPaths::ProjectDir();
	Params->TryGetStringField(TEXT("path"), SearchPath);

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *SearchPath, TEXT("*.py"), true, false);

	TArray<TSharedPtr<FJsonValue>> List;
	for (const FString& F : Files)
	{
		FString Rel = F;
		FPaths::MakePathRelativeTo(Rel, *FPaths::ProjectDir());
		List.Add(MakeShared<FJsonValueString>(Rel));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("scripts"), List);
	Result->SetNumberField(TEXT("count"), Files.Num());

	FString Json;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

	return FMCPToolResult::Success(FString::Printf(TEXT("Found %d Python scripts\n%s"), Files.Num(), *Json));
}

FMCPToolResult FMCPPythonTools::PythonRegisterTool(const TSharedPtr<FJsonObject>& Params)
{
	FString Name, Description, Code;
	if (!Params->TryGetStringField(TEXT("name"), Name))
		return FMCPToolResult::Error(TEXT("Missing 'name'"));
	if (!Params->TryGetStringField(TEXT("description"), Description))
		return FMCPToolResult::Error(TEXT("Missing 'description'"));
	if (!Params->TryGetStringField(TEXT("code"), Code))
		return FMCPToolResult::Error(TEXT("Missing 'code'"));

	// Validate name — no spaces, must be snake_case
	if (Name.Contains(TEXT(" ")) || Name.IsEmpty())
		return FMCPToolResult::Error(TEXT("Tool name must be snake_case with no spaces"));

	// Store in dynamic registry
	FPythonToolEntry Entry;
	Entry.Name        = Name;
	Entry.Description = Description;
	Entry.Code        = Code;
	GPythonTools.Add(Name, Entry);

	// Register as a real MCP tool so it appears in tools/list immediately
	FMCPToolDef DynT;
	DynT.Name        = Name;
	DynT.Description = Description;
	DynT.Handler = [Name](const TSharedPtr<FJsonObject>& P) -> FMCPToolResult
	{
		const FPythonToolEntry* E = GPythonTools.Find(Name);
		if (!E) return FMCPToolResult::Error(FString::Printf(TEXT("Python tool '%s' not found"), *Name));
		return ExecutePythonTool(*E, P);
	};
	FMCPToolRegistry::Get().RegisterTool(DynT);

	return FMCPToolResult::Success(FString::Printf(TEXT("Registered Python tool '%s'"), *Name));
}

FMCPToolResult FMCPPythonTools::PythonUnregisterTool(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params->TryGetStringField(TEXT("name"), Name))
		return FMCPToolResult::Error(TEXT("Missing 'name'"));

	if (!GPythonTools.Contains(Name))
		return FMCPToolResult::Error(FString::Printf(TEXT("No dynamic tool named '%s'"), *Name));

	GPythonTools.Remove(Name);
	FMCPToolRegistry::Get().UnregisterTool(Name);

	return FMCPToolResult::Success(FString::Printf(TEXT("Unregistered Python tool '%s'"), *Name));
}

FMCPToolResult FMCPPythonTools::PythonListDynamicTools(const TSharedPtr<FJsonObject>& Params)
{
	TArray<TSharedPtr<FJsonValue>> List;
	for (const auto& Pair : GPythonTools)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Pair.Value.Name);
		Obj->SetStringField(TEXT("description"), Pair.Value.Description);
		List.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tools"), List);
	Result->SetNumberField(TEXT("count"), GPythonTools.Num());

	FString Json;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

	return FMCPToolResult::Success(FString::Printf(TEXT("%d dynamic Python tools\n%s"), GPythonTools.Num(), *Json));
}

FMCPToolResult FMCPPythonTools::PythonReloadToolsFromFile(const TSharedPtr<FJsonObject>& Params)
{
	FString FilePath;
	if (!Params->TryGetStringField(TEXT("file_path"), FilePath))
		return FMCPToolResult::Error(TEXT("Missing 'file_path'"));

	if (FPaths::IsRelative(FilePath))
		FilePath = FPaths::Combine(FPaths::ProjectDir(), FilePath);

	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *FilePath))
		return FMCPToolResult::Error(FString::Printf(TEXT("Could not read file: %s"), *FilePath));

	TSharedPtr<FJsonValue> ParsedJson;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);
	if (!FJsonSerializer::Deserialize(Reader, ParsedJson) || !ParsedJson.IsValid())
		return FMCPToolResult::Error(TEXT("Invalid JSON in file"));

	const TArray<TSharedPtr<FJsonValue>>* ToolArray;
	if (!ParsedJson->TryGetArray(ToolArray))
		return FMCPToolResult::Error(TEXT("JSON root must be an array of tool objects"));

	int32 Registered = 0;
	for (const TSharedPtr<FJsonValue>& Val : *ToolArray)
	{
		const TSharedPtr<FJsonObject>* ObjPtr;
		if (!Val->TryGetObject(ObjPtr)) continue;

		FString Name, Description, Code;
		if (!(*ObjPtr)->TryGetStringField(TEXT("name"), Name))        continue;
		if (!(*ObjPtr)->TryGetStringField(TEXT("description"), Description)) continue;
		if (!(*ObjPtr)->TryGetStringField(TEXT("code"), Code))        continue;

		// Register (same logic as python_register_tool)
		FPythonToolEntry Entry{ Name, Description, Code };
		GPythonTools.Add(Name, Entry);

		FMCPToolDef DynT;
		DynT.Name        = Name;
		DynT.Description = Description;
		DynT.Handler = [Name](const TSharedPtr<FJsonObject>& P) -> FMCPToolResult
		{
			const FPythonToolEntry* E = GPythonTools.Find(Name);
			if (!E) return FMCPToolResult::Error(FString::Printf(TEXT("Python tool '%s' not found"), *Name));
			return ExecutePythonTool(*E, P);
		};
		FMCPToolRegistry::Get().RegisterTool(DynT);
		Registered++;
	}

	return FMCPToolResult::Success(FString::Printf(TEXT("Loaded %d Python tools from '%s'"),
		Registered, *FPaths::GetCleanFilename(FilePath)));
}
