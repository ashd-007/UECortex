#include "MCPNiagaraTools.h"
#include "UECortexModule.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraTypes.h"
#include "NiagaraParameterStore.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Async/Async.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Dom/JsonObject.h"

// ─────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────

static UWorld* GetEditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

static AActor* FindActorByLabel(UWorld* World, const FString& Label)
{
	if (!World) return nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == Label || (*It)->GetName() == Label)
			return *It;
	}
	return nullptr;
}

// ─────────────────────────────────────────────
//  RegisterTools
// ─────────────────────────────────────────────

void FMCPNiagaraTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	// niagara_list_systems
	{
		FMCPToolDef T;
		T.Name = TEXT("niagara_list_systems");
		T.Description = TEXT("List all NiagaraSystem assets in the project (optional path filter)");
		T.Params = {
			{ TEXT("filter"), TEXT("string"), TEXT("Optional substring filter on asset path"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return NiagaraListSystems(A); };
		OutTools.Add(T);
	}

	// niagara_create_system
	{
		FMCPToolDef T;
		T.Name = TEXT("niagara_create_system");
		T.Description = TEXT("Create a new empty NiagaraSystem asset at a content path");
		T.Params = {
			{ TEXT("path"), TEXT("string"), TEXT("Content path e.g. '/Game/FX/NS_MyEffect'"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return NiagaraCreateSystem(A); };
		OutTools.Add(T);
	}

	// niagara_list_emitters
	{
		FMCPToolDef T;
		T.Name = TEXT("niagara_list_emitters");
		T.Description = TEXT("List all emitters inside a NiagaraSystem asset");
		T.Params = {
			{ TEXT("system_path"), TEXT("string"), TEXT("Content path to the NiagaraSystem asset"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return NiagaraListEmitters(A); };
		OutTools.Add(T);
	}

	// niagara_get_parameters
	{
		FMCPToolDef T;
		T.Name = TEXT("niagara_get_parameters");
		T.Description = TEXT("Get all user-exposed parameters on a NiagaraSystem asset");
		T.Params = {
			{ TEXT("system_path"), TEXT("string"), TEXT("Content path to the NiagaraSystem asset"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return NiagaraGetParameters(A); };
		OutTools.Add(T);
	}

	// niagara_set_parameter
	{
		FMCPToolDef T;
		T.Name = TEXT("niagara_set_parameter");
		T.Description = TEXT("Set a user-exposed parameter on a NiagaraSystem asset. Supported types: float, int, bool, vector, color");
		T.Params = {
			{ TEXT("system_path"), TEXT("string"), TEXT("Content path to the NiagaraSystem asset"), true },
			{ TEXT("name"),        TEXT("string"), TEXT("Parameter name (without 'User.' prefix)"), true },
			{ TEXT("type"),        TEXT("string"), TEXT("Type: float | int | bool | vector | color"), true },
			{ TEXT("value"),       TEXT("string"), TEXT("Value as string. Vector: 'x,y,z'. Color: 'r,g,b,a'. Bool: 'true'/'false'"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return NiagaraSetParameter(A); };
		OutTools.Add(T);
	}

	// niagara_spawn_component
	{
		FMCPToolDef T;
		T.Name = TEXT("niagara_spawn_component");
		T.Description = TEXT("Add a NiagaraComponent to an actor in the level, linked to a NiagaraSystem asset");
		T.Params = {
			{ TEXT("actor_name"),   TEXT("string"), TEXT("Actor label or name"), true },
			{ TEXT("system_path"),  TEXT("string"), TEXT("Content path to the NiagaraSystem asset"), true },
			{ TEXT("auto_activate"),TEXT("boolean"), TEXT("Activate the effect immediately (default true)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return NiagaraSpawnComponent(A); };
		OutTools.Add(T);
	}

	// niagara_set_component_parameter
	{
		FMCPToolDef T;
		T.Name = TEXT("niagara_set_component_parameter");
		T.Description = TEXT("Override a parameter on a NiagaraComponent attached to an actor. Supported types: float, int, bool, vector, color");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Actor label or name"), true },
			{ TEXT("name"),       TEXT("string"), TEXT("Parameter name"), true },
			{ TEXT("type"),       TEXT("string"), TEXT("Type: float | int | bool | vector | color"), true },
			{ TEXT("value"),      TEXT("string"), TEXT("Value as string"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return NiagaraSetComponentParameter(A); };
		OutTools.Add(T);
	}

	// niagara_activate_component
	{
		FMCPToolDef T;
		T.Name = TEXT("niagara_activate_component");
		T.Description = TEXT("Activate or deactivate a NiagaraComponent on an actor");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"),  TEXT("Actor label or name"), true },
			{ TEXT("activate"),   TEXT("boolean"), TEXT("True to activate, false to deactivate"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return NiagaraActivateComponent(A); };
		OutTools.Add(T);
	}
}

// ─────────────────────────────────────────────
//  Implementations
// ─────────────────────────────────────────────

FMCPToolResult FMCPNiagaraTools::NiagaraListSystems(const TSharedPtr<FJsonObject>& Args)
{
	FString Filter;
	Args->TryGetStringField(TEXT("filter"), Filter);

	FAssetRegistryModule& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Assets;
	FARFilter ARFilter;
	ARFilter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
	AR.Get().GetAssets(ARFilter, Assets);

	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FAssetData& Asset : Assets)
	{
		FString Path = Asset.GetObjectPathString();
		if (!Filter.IsEmpty() && !Path.Contains(Filter, ESearchCase::IgnoreCase)) continue;

		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		Obj->SetStringField(TEXT("path"), Path);
		Arr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("systems"), Arr);
	Data->SetNumberField(TEXT("count"), Arr.Num());
	return FMCPToolResult::Success(FString::Printf(TEXT("Found %d Niagara systems"), Arr.Num()), Data);
}

FMCPToolResult FMCPNiagaraTools::NiagaraCreateSystem(const TSharedPtr<FJsonObject>& Args)
{
	FString Path;
	if (!Args->TryGetStringField(TEXT("path"), Path))
		return FMCPToolResult::Error(TEXT("path required"));

	FString AssetName = FPackageName::GetLongPackageAssetName(Path);
	UPackage* Package = CreatePackage(*Path);
	Package->FullyLoad();

	UNiagaraSystem* System = NewObject<UNiagaraSystem>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!System) return FMCPToolResult::Error(TEXT("Failed to create NiagaraSystem"));

	FAssetRegistryModule::AssetCreated(System);
	Package->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(TEXT("Created NiagaraSystem '%s'"), *Path));
}

FMCPToolResult FMCPNiagaraTools::NiagaraListEmitters(const TSharedPtr<FJsonObject>& Args)
{
	FString SystemPath;
	if (!Args->TryGetStringField(TEXT("system_path"), SystemPath))
		return FMCPToolResult::Error(TEXT("system_path required"));

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System) return FMCPToolResult::Error(FString::Printf(TEXT("NiagaraSystem not found: %s"), *SystemPath));

	const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FNiagaraEmitterHandle& Handle : Handles)
	{
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"),    Handle.GetName().ToString());
		Obj->SetBoolField(TEXT("enabled"),   Handle.GetIsEnabled());
		Obj->SetStringField(TEXT("unique_name"), Handle.GetUniqueInstanceName());
		Arr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("emitters"), Arr);
	Data->SetNumberField(TEXT("count"), Arr.Num());
	return FMCPToolResult::Success(FString::Printf(TEXT("Found %d emitters in '%s'"), Arr.Num(), *SystemPath), Data);
}

FMCPToolResult FMCPNiagaraTools::NiagaraGetParameters(const TSharedPtr<FJsonObject>& Args)
{
	FString SystemPath;
	if (!Args->TryGetStringField(TEXT("system_path"), SystemPath))
		return FMCPToolResult::Error(TEXT("system_path required"));

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System) return FMCPToolResult::Error(FString::Printf(TEXT("NiagaraSystem not found: %s"), *SystemPath));

	TArray<FNiagaraVariable> UserParams;
	System->GetExposedParameters().GetUserParameters(UserParams);

	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FNiagaraVariable& Var : UserParams)
	{
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Var.GetName().ToString());
		Obj->SetStringField(TEXT("type"), Var.GetType().GetName());
		Arr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("parameters"), Arr);
	Data->SetNumberField(TEXT("count"), Arr.Num());
	return FMCPToolResult::Success(FString::Printf(TEXT("Found %d parameters on '%s'"), Arr.Num(), *SystemPath), Data);
}

FMCPToolResult FMCPNiagaraTools::NiagaraSetParameter(const TSharedPtr<FJsonObject>& Args)
{
	FString SystemPath, ParamName, TypeStr, ValueStr;
	if (!Args->TryGetStringField(TEXT("system_path"), SystemPath)) return FMCPToolResult::Error(TEXT("system_path required"));
	if (!Args->TryGetStringField(TEXT("name"),        ParamName))  return FMCPToolResult::Error(TEXT("name required"));
	if (!Args->TryGetStringField(TEXT("type"),        TypeStr))    return FMCPToolResult::Error(TEXT("type required"));
	if (!Args->TryGetStringField(TEXT("value"),       ValueStr))   return FMCPToolResult::Error(TEXT("value required"));

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System) return FMCPToolResult::Error(FString::Printf(TEXT("NiagaraSystem not found: %s"), *SystemPath));

	System->Modify();
	FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();

	TypeStr = TypeStr.ToLower();

	if (TypeStr == TEXT("float"))
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetFloatDef(), FName(*ParamName));
		FNiagaraUserRedirectionParameterStore::MakeUserVariable(Var);
		float Val = FCString::Atof(*ValueStr);
		Store.SetParameterValue(Val, Var, true);
	}
	else if (TypeStr == TEXT("int"))
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetIntDef(), FName(*ParamName));
		FNiagaraUserRedirectionParameterStore::MakeUserVariable(Var);
		int32 Val = FCString::Atoi(*ValueStr);
		Store.SetParameterValue(Val, Var, true);
	}
	else if (TypeStr == TEXT("bool"))
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetBoolDef(), FName(*ParamName));
		FNiagaraUserRedirectionParameterStore::MakeUserVariable(Var);
		bool Val = ValueStr.ToBool();
		FNiagaraBool NiagaraBoolVal;
		NiagaraBoolVal.SetValue(Val);
		Store.SetParameterValue(NiagaraBoolVal, Var, true);
	}
	else if (TypeStr == TEXT("vector"))
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetVec3Def(), FName(*ParamName));
		FNiagaraUserRedirectionParameterStore::MakeUserVariable(Var);
		TArray<FString> Parts;
		ValueStr.ParseIntoArray(Parts, TEXT(","));
		FVector Val(
			Parts.IsValidIndex(0) ? FCString::Atof(*Parts[0]) : 0.f,
			Parts.IsValidIndex(1) ? FCString::Atof(*Parts[1]) : 0.f,
			Parts.IsValidIndex(2) ? FCString::Atof(*Parts[2]) : 0.f);
		Store.SetParameterValue(Val, Var, true);
	}
	else if (TypeStr == TEXT("color"))
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition::GetColorDef(), FName(*ParamName));
		FNiagaraUserRedirectionParameterStore::MakeUserVariable(Var);
		TArray<FString> Parts;
		ValueStr.ParseIntoArray(Parts, TEXT(","));
		FLinearColor Val(
			Parts.IsValidIndex(0) ? FCString::Atof(*Parts[0]) : 0.f,
			Parts.IsValidIndex(1) ? FCString::Atof(*Parts[1]) : 0.f,
			Parts.IsValidIndex(2) ? FCString::Atof(*Parts[2]) : 0.f,
			Parts.IsValidIndex(3) ? FCString::Atof(*Parts[3]) : 1.f);
		Store.SetParameterValue(Val, Var, true);
	}
	else
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Unknown type '%s'. Use: float, int, bool, vector, color"), *TypeStr));
	}

	System->MarkPackageDirty();
	return FMCPToolResult::Success(FString::Printf(TEXT("Set parameter '%s' (%s) = %s on '%s'"), *ParamName, *TypeStr, *ValueStr, *SystemPath));
}

FMCPToolResult FMCPNiagaraTools::NiagaraSpawnComponent(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName, SystemPath;
	if (!Args->TryGetStringField(TEXT("actor_name"),  ActorName))  return FMCPToolResult::Error(TEXT("actor_name required"));
	if (!Args->TryGetStringField(TEXT("system_path"), SystemPath)) return FMCPToolResult::Error(TEXT("system_path required"));

	bool bAutoActivate = true;
	Args->TryGetBoolField(TEXT("auto_activate"), bAutoActivate);

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, SystemPath, bAutoActivate, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByLabel(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
		if (!System) { Result = FMCPToolResult::Error(FString::Printf(TEXT("NiagaraSystem not found: %s"), *SystemPath)); return; }

		UNiagaraComponent* Comp = NewObject<UNiagaraComponent>(Actor, TEXT("NiagaraFX"));
		Comp->SetAsset(System);
		Comp->bAutoActivate = bAutoActivate;
		Comp->RegisterComponent();
		Actor->AddInstanceComponent(Comp);
		Actor->Modify();

		if (bAutoActivate)
			Comp->Activate(true);

		Result = FMCPToolResult::Success(FString::Printf(
			TEXT("Added NiagaraComponent to '%s' with system '%s'"), *ActorName, *SystemPath));
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPNiagaraTools::NiagaraSetComponentParameter(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName, ParamName, TypeStr, ValueStr;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName)) return FMCPToolResult::Error(TEXT("actor_name required"));
	if (!Args->TryGetStringField(TEXT("name"),       ParamName)) return FMCPToolResult::Error(TEXT("name required"));
	if (!Args->TryGetStringField(TEXT("type"),       TypeStr))   return FMCPToolResult::Error(TEXT("type required"));
	if (!Args->TryGetStringField(TEXT("value"),      ValueStr))  return FMCPToolResult::Error(TEXT("value required"));

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, ParamName, TypeStr, ValueStr, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByLabel(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UNiagaraComponent* Comp = Actor->FindComponentByClass<UNiagaraComponent>();
		if (!Comp) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no NiagaraComponent"), *ActorName)); return; }

		FString Type = TypeStr.ToLower();
		if (Type == TEXT("float"))
		{
			Comp->SetVariableFloat(FName(*ParamName), FCString::Atof(*ValueStr));
		}
		else if (Type == TEXT("int"))
		{
			Comp->SetVariableInt(FName(*ParamName), FCString::Atoi(*ValueStr));
		}
		else if (Type == TEXT("bool"))
		{
			Comp->SetVariableBool(FName(*ParamName), ValueStr.ToBool());
		}
		else if (Type == TEXT("vector"))
		{
			TArray<FString> Parts;
			ValueStr.ParseIntoArray(Parts, TEXT(","));
			FVector Val(
				Parts.IsValidIndex(0) ? FCString::Atof(*Parts[0]) : 0.f,
				Parts.IsValidIndex(1) ? FCString::Atof(*Parts[1]) : 0.f,
				Parts.IsValidIndex(2) ? FCString::Atof(*Parts[2]) : 0.f);
			Comp->SetVariableVec3(FName(*ParamName), Val);
		}
		else if (Type == TEXT("color"))
		{
			TArray<FString> Parts;
			ValueStr.ParseIntoArray(Parts, TEXT(","));
			FLinearColor Val(
				Parts.IsValidIndex(0) ? FCString::Atof(*Parts[0]) : 0.f,
				Parts.IsValidIndex(1) ? FCString::Atof(*Parts[1]) : 0.f,
				Parts.IsValidIndex(2) ? FCString::Atof(*Parts[2]) : 0.f,
				Parts.IsValidIndex(3) ? FCString::Atof(*Parts[3]) : 1.f);
			Comp->SetVariableLinearColor(FName(*ParamName), Val);
		}
		else
		{
			Result = FMCPToolResult::Error(FString::Printf(TEXT("Unknown type '%s'"), *TypeStr));
			return;
		}

		Result = FMCPToolResult::Success(FString::Printf(
			TEXT("Set parameter '%s' (%s) = %s on '%s'"), *ParamName, *TypeStr, *ValueStr, *ActorName));
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPNiagaraTools::NiagaraActivateComponent(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName;
	bool bActivate = true;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName)) return FMCPToolResult::Error(TEXT("actor_name required"));
	if (!Args->TryGetBoolField(TEXT("activate"), bActivate))     return FMCPToolResult::Error(TEXT("activate required"));

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, bActivate, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByLabel(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UNiagaraComponent* Comp = Actor->FindComponentByClass<UNiagaraComponent>();
		if (!Comp) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no NiagaraComponent"), *ActorName)); return; }

		if (bActivate)
			Comp->Activate(true);
		else
			Comp->Deactivate();

		Result = FMCPToolResult::Success(FString::Printf(
			TEXT("%s NiagaraComponent on '%s'"), bActivate ? TEXT("Activated") : TEXT("Deactivated"), *ActorName));
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}
