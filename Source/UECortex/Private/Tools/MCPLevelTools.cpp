#include "Tools/MCPLevelTools.h"
#include "UECortexModule.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Selection.h"
#include "CollisionQueryParams.h"

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMCPLevelTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	// actor_spawn
	{
		FMCPToolDef Def;
		Def.Name = TEXT("actor_spawn");
		Def.Description = TEXT("Spawn an actor in the current level by class path or Blueprint asset path.");
		Def.Params = {
			{ TEXT("class_path"), TEXT("string"),
			  TEXT("UE class path, e.g. '/Engine/BasicShapes/Cube.Cube' or '/Game/BP_MyActor.BP_MyActor_C'") },
			{ TEXT("name"),       TEXT("string"),  TEXT("Label for the spawned actor"), false },
			{ TEXT("location"),   TEXT("object"),  TEXT("{x,y,z} world location in cm"), false },
			{ TEXT("rotation"),   TEXT("object"),  TEXT("{pitch,yaw,roll} in degrees"), false },
			{ TEXT("scale"),      TEXT("object"),  TEXT("{x,y,z} scale"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ActorSpawn(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// actor_delete
	{
		FMCPToolDef Def;
		Def.Name = TEXT("actor_delete");
		Def.Description = TEXT("Delete a named actor from the current level.");
		Def.Params = {
			{ TEXT("name"), TEXT("string"), TEXT("Exact actor label to delete") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ActorDelete(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// actor_set_transform
	{
		FMCPToolDef Def;
		Def.Name = TEXT("actor_set_transform");
		Def.Description = TEXT("Set location, rotation, and/or scale on a named actor.");
		Def.Params = {
			{ TEXT("name"),     TEXT("string"), TEXT("Actor label") },
			{ TEXT("location"), TEXT("object"), TEXT("{x,y,z} in cm"), false },
			{ TEXT("rotation"), TEXT("object"), TEXT("{pitch,yaw,roll} in degrees"), false },
			{ TEXT("scale"),    TEXT("object"), TEXT("{x,y,z} scale"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ActorSetTransform(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// actor_get_properties
	{
		FMCPToolDef Def;
		Def.Name = TEXT("actor_get_properties");
		Def.Description = TEXT("Return transform and basic properties of a named actor as JSON.");
		Def.Params = {
			{ TEXT("name"), TEXT("string"), TEXT("Actor label") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ActorGetProperties(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// actor_list
	{
		FMCPToolDef Def;
		Def.Name = TEXT("actor_list");
		Def.Description = TEXT("List all actors in the current level with class, label, and transform.");
		Def.Params = {
			{ TEXT("filter_class"), TEXT("string"), TEXT("Optional class name substring filter"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ActorList(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// level_get_info
	{
		FMCPToolDef Def;
		Def.Name = TEXT("level_get_info");
		Def.Description = TEXT("Return current level name, world type, and actor count.");
		Def.Params = {};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return LevelGetInfo(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// viewport_focus
	{
		FMCPToolDef Def;
		Def.Name = TEXT("viewport_focus");
		Def.Description = TEXT("Focus the editor viewport on a named actor or world location.");
		Def.Params = {
			{ TEXT("name"),     TEXT("string"), TEXT("Actor label to focus on"), false },
			{ TEXT("location"), TEXT("object"), TEXT("{x,y,z} world location to focus on"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ViewportFocus(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// console_command
	{
		FMCPToolDef Def;
		Def.Name = TEXT("console_command");
		Def.Description = TEXT("Execute any Unreal Engine console command.");
		Def.Params = {
			{ TEXT("command"), TEXT("string"), TEXT("Console command string to execute") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ConsoleCommand(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// actor_find_by_name
	{
		FMCPToolDef Def;
		Def.Name = TEXT("actor_find_by_name");
		Def.Description = TEXT("Search actors by substring match on their label. Returns all matches.");
		Def.Params = {
			{ TEXT("query"), TEXT("string"), TEXT("Substring to search for in actor labels") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ActorFindByName(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// actor_spawn_on_surface
	{
		FMCPToolDef Def;
		Def.Name = TEXT("actor_spawn_on_surface");
		Def.Description = TEXT("Raycast downward from (x,y,z_start) and spawn actor on first hit surface.");
		Def.Params = {
			{ TEXT("class_path"), TEXT("string"), TEXT("UE class path or mesh path") },
			{ TEXT("x"),          TEXT("number"), TEXT("World X position") },
			{ TEXT("y"),          TEXT("number"), TEXT("World Y position") },
			{ TEXT("z_start"),    TEXT("number"), TEXT("Raycast start Z (above expected surface)"), false },
			{ TEXT("name"),       TEXT("string"), TEXT("Label for spawned actor"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ActorSpawnOnSurface(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// actor_duplicate
	{
		FMCPToolDef Def;
		Def.Name = TEXT("actor_duplicate");
		Def.Description = TEXT("Duplicate an existing actor with an optional position offset.");
		Def.Params = {
			{ TEXT("name"),       TEXT("string"), TEXT("Source actor label") },
			{ TEXT("new_name"),   TEXT("string"), TEXT("Label for the duplicate"), false },
			{ TEXT("offset"),     TEXT("object"), TEXT("{x,y,z} offset from source location"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ActorDuplicate(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// actor_get_selected
	{
		FMCPToolDef Def;
		Def.Name = TEXT("actor_get_selected");
		Def.Description = TEXT("Return all currently editor-selected actors with their properties.");
		Def.Params = {};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ActorGetSelected(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// actor_set_property
	{
		FMCPToolDef Def;
		Def.Name = TEXT("actor_set_property");
		Def.Description = TEXT("Set any UPROPERTY on a named actor by property name and string value.");
		Def.Params = {
			{ TEXT("name"),     TEXT("string"), TEXT("Actor label") },
			{ TEXT("property"), TEXT("string"), TEXT("UPROPERTY name (e.g. 'bHidden', 'InitialLifeSpan')") },
			{ TEXT("value"),    TEXT("string"), TEXT("Value as string (e.g. 'true', '5.0', 'MyTag')") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ActorSetProperty(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// viewport_set_camera
	{
		FMCPToolDef Def;
		Def.Name = TEXT("viewport_set_camera");
		Def.Description = TEXT("Set the editor viewport camera to a specific location and orientation.");
		Def.Params = {
			{ TEXT("location"), TEXT("object"), TEXT("{x,y,z} camera position in world space") },
			{ TEXT("rotation"), TEXT("object"), TEXT("{pitch,yaw,roll} camera orientation in degrees"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return ViewportSetCamera(Args); };
		OutTools.Add(MoveTemp(Def));
	}

	// line_trace
	{
		FMCPToolDef Def;
		Def.Name = TEXT("line_trace");
		Def.Description = TEXT("Raycast from start to end, return first blocking hit actor, location, and normal.");
		Def.Params = {
			{ TEXT("start"), TEXT("object"), TEXT("{x,y,z} trace start in world space") },
			{ TEXT("end"),   TEXT("object"), TEXT("{x,y,z} trace end in world space") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& Args) { return LineTrace(Args); };
		OutTools.Add(MoveTemp(Def));
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static UWorld* GetEditorWorld()
{
	// Despite the name (kept to avoid touching every call site), this returns the tracked
	// active world -- the editor world normally, the live PIE world while Play is running. See
	// FUECortexModule::GetActiveWorld().
	return FUECortexModule::GetActiveWorld();
}

static AActor* FindActorByLabel(const FString& Label)
{
	UWorld* World = GetEditorWorld();
	if (!World) return nullptr;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == Label)
		{
			return *It;
		}
	}
	return nullptr;
}

static FVector ParseVector(const TSharedPtr<FJsonObject>& Obj, const FVector& Default = FVector::ZeroVector)
{
	if (!Obj.IsValid()) return Default;
	double X = Obj->HasField(TEXT("x")) ? Obj->GetNumberField(TEXT("x")) : Default.X;
	double Y = Obj->HasField(TEXT("y")) ? Obj->GetNumberField(TEXT("y")) : Default.Y;
	double Z = Obj->HasField(TEXT("z")) ? Obj->GetNumberField(TEXT("z")) : Default.Z;
	return FVector(X, Y, Z);
}

static FRotator ParseRotator(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return FRotator::ZeroRotator;
	double P = Obj->HasField(TEXT("pitch")) ? Obj->GetNumberField(TEXT("pitch")) : 0.0;
	double Y = Obj->HasField(TEXT("yaw"))   ? Obj->GetNumberField(TEXT("yaw"))   : 0.0;
	double R = Obj->HasField(TEXT("roll"))  ? Obj->GetNumberField(TEXT("roll"))  : 0.0;
	return FRotator(P, Y, R);
}

static TSharedPtr<FJsonObject> VectorToJson(const FVector& V)
{
	auto Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("x"), V.X);
	Obj->SetNumberField(TEXT("y"), V.Y);
	Obj->SetNumberField(TEXT("z"), V.Z);
	return Obj;
}

static TSharedPtr<FJsonObject> RotatorToJson(const FRotator& R)
{
	auto Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("pitch"), R.Pitch);
	Obj->SetNumberField(TEXT("yaw"),   R.Yaw);
	Obj->SetNumberField(TEXT("roll"),  R.Roll);
	return Obj;
}

// ---------------------------------------------------------------------------
// Implementations
// ---------------------------------------------------------------------------

FMCPToolResult FMCPLevelTools::ActorSpawn(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassPath;
	if (!Args->TryGetStringField(TEXT("class_path"), ClassPath))
	{
		return FMCPToolResult::Error(TEXT("Missing required param: class_path"));
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FMCPToolResult::Error(TEXT("No editor world available"));
	}

	// Resolve class — try UClass load first, then Blueprint _C
	UClass* SpawnClass = nullptr;

	// Try loading as a Blueprint asset (_C suffix = generated class)
	FString GenClassPath = ClassPath;
	if (!GenClassPath.EndsWith(TEXT("_C")))
	{
		GenClassPath += TEXT("_C");
	}
	SpawnClass = LoadObject<UClass>(nullptr, *GenClassPath);

	// Fallback: try as native class path (e.g. /Script/Engine.StaticMeshActor)
	if (!SpawnClass)
	{
		SpawnClass = LoadObject<UClass>(nullptr, *ClassPath);
	}

	// Fallback: try UObject load (for basic shape assets treated as StaticMeshActor)
	if (!SpawnClass)
	{
		// For paths like /Engine/BasicShapes/Cube.Cube — spawn as StaticMeshActor
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ClassPath);
		if (Mesh)
		{
			FVector Location = FVector::ZeroVector;
			FRotator Rotation = FRotator::ZeroRotator;
			FVector Scale(1.0f);

			const TSharedPtr<FJsonObject>* LocObj;
			const TSharedPtr<FJsonObject>* RotObj;
			const TSharedPtr<FJsonObject>* ScaleObj;
			if (Args->TryGetObjectField(TEXT("location"), LocObj)) Location = ParseVector(*LocObj);
			if (Args->TryGetObjectField(TEXT("rotation"), RotObj)) Rotation = ParseRotator(*RotObj);
			if (Args->TryGetObjectField(TEXT("scale"),    ScaleObj)) Scale = ParseVector(*ScaleObj, FVector(1));

			FActorSpawnParameters Params;
			AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(
				AStaticMeshActor::StaticClass(),
				FTransform(Rotation, Location, Scale),
				Params);

			if (!MeshActor)
			{
				return FMCPToolResult::Error(TEXT("Failed to spawn StaticMeshActor"));
			}

			MeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
			MeshActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);

			FString Label;
			if (Args->TryGetStringField(TEXT("name"), Label))
			{
				MeshActor->SetActorLabel(Label);
			}

			return FMCPToolResult::Success(FString::Printf(
				TEXT("Spawned StaticMeshActor '%s' with mesh '%s' at (%.1f, %.1f, %.1f)"),
				*MeshActor->GetActorLabel(), *ClassPath,
				Location.X, Location.Y, Location.Z));
		}

		return FMCPToolResult::Error(FString::Printf(
			TEXT("Could not resolve class or mesh from path: %s"), *ClassPath));
	}

	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale(1.0f);

	const TSharedPtr<FJsonObject>* LocObj;
	const TSharedPtr<FJsonObject>* RotObj;
	const TSharedPtr<FJsonObject>* ScaleObj;
	if (Args->TryGetObjectField(TEXT("location"), LocObj)) Location = ParseVector(*LocObj);
	if (Args->TryGetObjectField(TEXT("rotation"), RotObj)) Rotation = ParseRotator(*RotObj);
	if (Args->TryGetObjectField(TEXT("scale"),    ScaleObj)) Scale = ParseVector(*ScaleObj, FVector(1));

	FActorSpawnParameters SpawnParams;
	AActor* Actor = World->SpawnActor<AActor>(SpawnClass, FTransform(Rotation, Location, Scale), SpawnParams);

	if (!Actor)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("SpawnActor failed for class: %s"), *ClassPath));
	}

	FString Label;
	if (Args->TryGetStringField(TEXT("name"), Label))
	{
		Actor->SetActorLabel(Label);
	}

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Spawned '%s' (%s) at (%.1f, %.1f, %.1f)"),
		*Actor->GetActorLabel(), *SpawnClass->GetName(),
		Location.X, Location.Y, Location.Z));
}

FMCPToolResult FMCPLevelTools::ActorDelete(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FMCPToolResult::Error(TEXT("Missing required param: name"));
	}

	AActor* Actor = FindActorByLabel(Name);
	if (!Actor)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *Name));
	}

	UWorld* World = GetEditorWorld();
	World->DestroyActor(Actor);

	return FMCPToolResult::Success(FString::Printf(TEXT("Deleted actor '%s'"), *Name));
}

FMCPToolResult FMCPLevelTools::ActorSetTransform(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FMCPToolResult::Error(TEXT("Missing required param: name"));
	}

	AActor* Actor = FindActorByLabel(Name);
	if (!Actor)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *Name));
	}

	const TSharedPtr<FJsonObject>* LocObj;
	const TSharedPtr<FJsonObject>* RotObj;
	const TSharedPtr<FJsonObject>* ScaleObj;

	if (Args->TryGetObjectField(TEXT("location"), LocObj))
	{
		Actor->SetActorLocation(ParseVector(*LocObj));
	}
	if (Args->TryGetObjectField(TEXT("rotation"), RotObj))
	{
		Actor->SetActorRotation(ParseRotator(*RotObj));
	}
	if (Args->TryGetObjectField(TEXT("scale"), ScaleObj))
	{
		Actor->SetActorScale3D(ParseVector(*ScaleObj, FVector(1)));
	}

	const FVector Loc = Actor->GetActorLocation();
	return FMCPToolResult::Success(FString::Printf(
		TEXT("Updated transform of '%s'. Location: (%.1f, %.1f, %.1f)"),
		*Name, Loc.X, Loc.Y, Loc.Z));
}

FMCPToolResult FMCPLevelTools::ActorGetProperties(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
	{
		return FMCPToolResult::Error(TEXT("Missing required param: name"));
	}

	AActor* Actor = FindActorByLabel(Name);
	if (!Actor)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *Name));
	}

	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("label"),  Actor->GetActorLabel());
	Data->SetStringField(TEXT("class"),  Actor->GetClass()->GetName());
	Data->SetStringField(TEXT("path"),   Actor->GetPathName());
	Data->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));
	Data->SetObjectField(TEXT("rotation"), RotatorToJson(Actor->GetActorRotation()));
	Data->SetObjectField(TEXT("scale"),    VectorToJson(Actor->GetActorScale3D()));
	Data->SetBoolField  (TEXT("hidden"),   Actor->IsHidden());
	Data->SetBoolField  (TEXT("selected"), Actor->IsSelected());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Properties for '%s'"), *Name), Data);
}

FMCPToolResult FMCPLevelTools::ActorList(const TSharedPtr<FJsonObject>& Args)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FMCPToolResult::Error(TEXT("No editor world available"));
	}

	FString ClassFilter;
	Args->TryGetStringField(TEXT("filter_class"), ClassFilter);

	auto ResultObj = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A->IsEditorOnly() && A->GetClass()->GetName().Contains(TEXT("Default"))) continue;

		const FString ClassName = A->GetClass()->GetName();
		if (!ClassFilter.IsEmpty() && !ClassName.Contains(ClassFilter)) continue;

		auto ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("label"), A->GetActorLabel());
		ActorObj->SetStringField(TEXT("class"), ClassName);
		ActorObj->SetObjectField(TEXT("location"), VectorToJson(A->GetActorLocation()));

		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
	}

	ResultObj->SetArrayField(TEXT("actors"), ActorsArray);
	ResultObj->SetNumberField(TEXT("count"), ActorsArray.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d actors"), ActorsArray.Num()), ResultObj);
}

FMCPToolResult FMCPLevelTools::LevelGetInfo(const TSharedPtr<FJsonObject>& Args)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FMCPToolResult::Error(TEXT("No editor world available"));
	}

	int32 ActorCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It) ActorCount++;

	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("level_name"), World->GetMapName());
	auto WorldTypeStr = [](EWorldType::Type T) -> FString {
		switch (T)
		{
			case EWorldType::Editor:        return TEXT("Editor");
			case EWorldType::EditorPreview: return TEXT("EditorPreview");
			case EWorldType::Game:          return TEXT("Game");
			case EWorldType::PIE:           return TEXT("PIE");
			case EWorldType::GamePreview:   return TEXT("GamePreview");
			case EWorldType::Inactive:      return TEXT("Inactive");
			default:                        return TEXT("None");
		}
	}(World->WorldType);
	Data->SetStringField(TEXT("world_type"), WorldTypeStr);
	Data->SetNumberField(TEXT("actor_count"), ActorCount);
	Data->SetBoolField  (TEXT("is_playing"),  World->IsPlayInEditor());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Level: %s, %d actors"), *World->GetMapName(), ActorCount), Data);
}

FMCPToolResult FMCPLevelTools::ViewportFocus(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	bool bByName = Args->TryGetStringField(TEXT("name"), Name);

	if (bByName)
	{
		AActor* Actor = FindActorByLabel(Name);
		if (!Actor)
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *Name));
		}

		if (GEditor)
		{
			GEditor->SelectNone(true, true);
			GEditor->SelectActor(Actor, true, true);
			GEditor->MoveViewportCamerasToActor(*Actor, false);
		}

		return FMCPToolResult::Success(
			FString::Printf(TEXT("Viewport focused on '%s'"), *Name));
	}

	const TSharedPtr<FJsonObject>* LocObj;
	if (Args->TryGetObjectField(TEXT("location"), LocObj))
	{
		FVector Loc = ParseVector(*LocObj);

		for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
		{
			if (Client && Client->IsPerspective())
			{
				Client->SetViewLocation(Loc);
				Client->Invalidate();
				break;
			}
		}

		return FMCPToolResult::Success(FString::Printf(
			TEXT("Viewport moved to (%.1f, %.1f, %.1f)"), Loc.X, Loc.Y, Loc.Z));
	}

	return FMCPToolResult::Error(TEXT("Provide either 'name' or 'location'"));
}

FMCPToolResult FMCPLevelTools::ConsoleCommand(const TSharedPtr<FJsonObject>& Args)
{
	FString Command;
	if (!Args->TryGetStringField(TEXT("command"), Command))
	{
		return FMCPToolResult::Error(TEXT("Missing required param: command"));
	}

	if (GEditor)
	{
		GEditor->Exec(FUECortexModule::GetActiveWorld(), *Command);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Executed: %s"), *Command));
}

FMCPToolResult FMCPLevelTools::ActorFindByName(const TSharedPtr<FJsonObject>& Args)
{
	FString Query;
	if (!Args->TryGetStringField(TEXT("query"), Query))
	{
		return FMCPToolResult::Error(TEXT("Missing required param: query"));
	}

	UWorld* World = GetEditorWorld();
	if (!World) return FMCPToolResult::Error(TEXT("No editor world available"));

	TArray<TSharedPtr<FJsonValue>> Results;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A->GetActorLabel().Contains(Query, ESearchCase::IgnoreCase))
		{
			auto Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("label"),    A->GetActorLabel());
			Obj->SetStringField(TEXT("class"),    A->GetClass()->GetName());
			Obj->SetObjectField(TEXT("location"), VectorToJson(A->GetActorLocation()));
			Results.Add(MakeShared<FJsonValueObject>(Obj));
		}
	}

	auto Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("actors"), Results);
	Data->SetNumberField(TEXT("count"), Results.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d actors matching '%s'"), Results.Num(), *Query), Data);
}

FMCPToolResult FMCPLevelTools::ActorSpawnOnSurface(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassPath;
	if (!Args->TryGetStringField(TEXT("class_path"), ClassPath))
		return FMCPToolResult::Error(TEXT("Missing required param: class_path"));

	UWorld* World = GetEditorWorld();
	if (!World) return FMCPToolResult::Error(TEXT("No editor world available"));

	double X = Args->HasField(TEXT("x")) ? Args->GetNumberField(TEXT("x")) : 0.0;
	double Y = Args->HasField(TEXT("y")) ? Args->GetNumberField(TEXT("y")) : 0.0;
	double ZStart = Args->HasField(TEXT("z_start")) ? Args->GetNumberField(TEXT("z_start")) : 10000.0;

	FVector RayStart(X, Y, ZStart);
	FVector RayEnd(X, Y, -10000.0);

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;

	if (!World->LineTraceSingleByChannel(Hit, RayStart, RayEnd, ECC_WorldStatic, QueryParams))
	{
		return FMCPToolResult::Error(TEXT("No surface found at given X,Y position"));
	}

	FVector SpawnLoc = Hit.ImpactPoint;

	// Reuse ActorSpawn logic via a synthetic args object
	auto SpawnArgs = MakeShared<FJsonObject>();
	SpawnArgs->SetStringField(TEXT("class_path"), ClassPath);
	auto LocObj = MakeShared<FJsonObject>();
	LocObj->SetNumberField(TEXT("x"), SpawnLoc.X);
	LocObj->SetNumberField(TEXT("y"), SpawnLoc.Y);
	LocObj->SetNumberField(TEXT("z"), SpawnLoc.Z);
	SpawnArgs->SetObjectField(TEXT("location"), LocObj);
	FString Label;
	if (Args->TryGetStringField(TEXT("name"), Label))
		SpawnArgs->SetStringField(TEXT("name"), Label);

	FMCPToolResult Result = ActorSpawn(SpawnArgs);
	if (Result.bSuccess)
	{
		Result.Message += FString::Printf(TEXT(" (surface hit at %.1f,%.1f,%.1f)"),
			SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z);
	}
	return Result;
}

FMCPToolResult FMCPLevelTools::ActorDuplicate(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
		return FMCPToolResult::Error(TEXT("Missing required param: name"));

	AActor* Source = FindActorByLabel(Name);
	if (!Source)
		return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *Name));

	UWorld* World = GetEditorWorld();

	const TSharedPtr<FJsonObject>* OffsetObj;
	FVector Offset = FVector::ZeroVector;
	if (Args->TryGetObjectField(TEXT("offset"), OffsetObj))
		Offset = ParseVector(*OffsetObj);

	FTransform NewTransform = Source->GetActorTransform();
	NewTransform.SetLocation(NewTransform.GetLocation() + Offset);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Template = Source;
	AActor* Dup = World->SpawnActor(Source->GetClass(), &NewTransform, SpawnParams);

	if (!Dup)
		return FMCPToolResult::Error(TEXT("Failed to duplicate actor"));

	FString NewName;
	if (Args->TryGetStringField(TEXT("new_name"), NewName))
		Dup->SetActorLabel(NewName);
	else
		Dup->SetActorLabel(Name + TEXT("_Copy"));

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Duplicated '%s' → '%s'"), *Name, *Dup->GetActorLabel()));
}

FMCPToolResult FMCPLevelTools::ActorGetSelected(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor) return FMCPToolResult::Error(TEXT("GEditor not available"));

	TArray<TSharedPtr<FJsonValue>> Selected;
	USelection* Selection = GEditor->GetSelectedActors();
	for (int32 i = 0; i < Selection->Num(); ++i)
	{
		AActor* A = Cast<AActor>(Selection->GetSelectedObject(i));
		if (!A) continue;

		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("label"),    A->GetActorLabel());
		Obj->SetStringField(TEXT("class"),    A->GetClass()->GetName());
		Obj->SetObjectField(TEXT("location"), VectorToJson(A->GetActorLocation()));
		Obj->SetObjectField(TEXT("rotation"), RotatorToJson(A->GetActorRotation()));
		Obj->SetObjectField(TEXT("scale"),    VectorToJson(A->GetActorScale3D()));
		Selected.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("actors"), Selected);
	Data->SetNumberField(TEXT("count"), Selected.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("%d actor(s) selected"), Selected.Num()), Data);
}

FMCPToolResult FMCPLevelTools::ActorSetProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString Name, PropName, Value;
	if (!Args->TryGetStringField(TEXT("name"),     Name))     return FMCPToolResult::Error(TEXT("Missing: name"));
	if (!Args->TryGetStringField(TEXT("property"), PropName)) return FMCPToolResult::Error(TEXT("Missing: property"));
	if (!Args->TryGetStringField(TEXT("value"),    Value))    return FMCPToolResult::Error(TEXT("Missing: value"));

	AActor* Actor = FindActorByLabel(Name);
	if (!Actor)
		return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *Name));

	FProperty* Prop = FindFProperty<FProperty>(Actor->GetClass(), FName(*PropName));
	if (!Prop)
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Property '%s' not found on %s"), *PropName, *Actor->GetClass()->GetName()));

	Actor->Modify();
	const TCHAR* ImportResult = Prop->ImportText_InContainer(*Value, Actor, Actor, PPF_None);
	if (!ImportResult)
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Failed to set '%s' = '%s' (type mismatch?)"), *PropName, *Value));

	Actor->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Set %s.%s = %s"), *Name, *PropName, *Value));
}

FMCPToolResult FMCPLevelTools::ViewportSetCamera(const TSharedPtr<FJsonObject>& Args)
{
	const TSharedPtr<FJsonObject>* LocObj;
	if (!Args->TryGetObjectField(TEXT("location"), LocObj))
		return FMCPToolResult::Error(TEXT("Missing required param: location"));

	FVector Location = ParseVector(*LocObj);

	FRotator Rotation = FRotator::ZeroRotator;
	const TSharedPtr<FJsonObject>* RotObj;
	if (Args->TryGetObjectField(TEXT("rotation"), RotObj))
		Rotation = ParseRotator(*RotObj);

	if (!GEditor) return FMCPToolResult::Error(TEXT("GEditor not available"));

	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (Client && Client->IsPerspective())
		{
			Client->SetViewLocation(Location);
			Client->SetViewRotation(Rotation);
			Client->Invalidate();
			break;
		}
	}

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Camera set to (%.1f,%.1f,%.1f) rot (%.1f,%.1f,%.1f)"),
		Location.X, Location.Y, Location.Z,
		Rotation.Pitch, Rotation.Yaw, Rotation.Roll));
}

FMCPToolResult FMCPLevelTools::LineTrace(const TSharedPtr<FJsonObject>& Args)
{
	const TSharedPtr<FJsonObject>* StartObj;
	const TSharedPtr<FJsonObject>* EndObj;
	if (!Args->TryGetObjectField(TEXT("start"), StartObj)) return FMCPToolResult::Error(TEXT("Missing: start"));
	if (!Args->TryGetObjectField(TEXT("end"),   EndObj))   return FMCPToolResult::Error(TEXT("Missing: end"));

	FVector Start = ParseVector(*StartObj);
	FVector End   = ParseVector(*EndObj);

	UWorld* World = GetEditorWorld();
	if (!World) return FMCPToolResult::Error(TEXT("No editor world available"));

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);

	auto Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("hit"), bHit);

	if (bHit)
	{
		Data->SetStringField(TEXT("actor"),    Hit.GetActor() ? Hit.GetActor()->GetActorLabel() : TEXT("None"));
		Data->SetStringField(TEXT("component"),Hit.GetComponent() ? Hit.GetComponent()->GetName() : TEXT("None"));
		Data->SetObjectField(TEXT("location"), VectorToJson(Hit.ImpactPoint));
		Data->SetObjectField(TEXT("normal"),   VectorToJson(Hit.ImpactNormal));
		Data->SetNumberField(TEXT("distance"), Hit.Distance);
	}

	FString Msg = bHit
		? FString::Printf(TEXT("Hit '%s' at (%.1f,%.1f,%.1f)"),
			Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("None"),
			Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z)
		: TEXT("No hit");

	return FMCPToolResult::Success(Msg, Data);
}
