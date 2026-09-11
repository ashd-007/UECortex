#include "Tools/MCPSplineTools.h"
#include "UECortexModule.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static UWorld* GetEditorWorldS()
{
	return FUECortexModule::GetActiveWorld();
}

static AActor* FindActorByLabelS(UWorld* World, const FString& Label)
{
	if (!World) return nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == Label) return *It;
	}
	return nullptr;
}

static USplineComponent* GetSplineComp(AActor* Actor)
{
	if (!Actor) return nullptr;
	return Actor->FindComponentByClass<USplineComponent>();
}

static TSharedPtr<FJsonObject> VecToJson(const FVector& V)
{
	auto O = MakeShared<FJsonObject>();
	O->SetNumberField(TEXT("x"), V.X);
	O->SetNumberField(TEXT("y"), V.Y);
	O->SetNumberField(TEXT("z"), V.Z);
	return O;
}

static FVector JsonToVec(const TSharedPtr<FJsonObject>& O, const FVector& Def = FVector::ZeroVector)
{
	if (!O.IsValid()) return Def;
	return FVector(
		O->HasField(TEXT("x")) ? O->GetNumberField(TEXT("x")) : Def.X,
		O->HasField(TEXT("y")) ? O->GetNumberField(TEXT("y")) : Def.Y,
		O->HasField(TEXT("z")) ? O->GetNumberField(TEXT("z")) : Def.Z);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMCPSplineTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	// spline_create
	{
		FMCPToolDef Def;
		Def.Name = TEXT("spline_create");
		Def.Description = TEXT("Spawn a new actor with a SplineComponent at the given location.");
		Def.Params = {
			{ TEXT("name"),     TEXT("string"), TEXT("Actor label for the spline actor") },
			{ TEXT("location"), TEXT("object"), TEXT("{x,y,z} world location"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return SplineCreate(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// spline_add_point
	{
		FMCPToolDef Def;
		Def.Name = TEXT("spline_add_point");
		Def.Description = TEXT("Add a world-space point to a spline actor's SplineComponent.");
		Def.Params = {
			{ TEXT("name"),     TEXT("string"), TEXT("Spline actor label") },
			{ TEXT("location"), TEXT("object"), TEXT("{x,y,z} world location for the new point") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return SplineAddPoint(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// spline_modify_point
	{
		FMCPToolDef Def;
		Def.Name = TEXT("spline_modify_point");
		Def.Description = TEXT("Move or set tangents on a spline point by index.");
		Def.Params = {
			{ TEXT("name"),            TEXT("string"), TEXT("Spline actor label") },
			{ TEXT("index"),           TEXT("number"), TEXT("Zero-based spline point index") },
			{ TEXT("location"),        TEXT("object"), TEXT("{x,y,z} new world location"), false },
			{ TEXT("arrive_tangent"),  TEXT("object"), TEXT("{x,y,z} arrive tangent"), false },
			{ TEXT("leave_tangent"),   TEXT("object"), TEXT("{x,y,z} leave tangent"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return SplineModifyPoint(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// spline_attach_mesh
	{
		FMCPToolDef Def;
		Def.Name = TEXT("spline_attach_mesh");
		Def.Description = TEXT("Attach SplineMeshComponents along each segment of a spline actor.");
		Def.Params = {
			{ TEXT("name"),       TEXT("string"), TEXT("Spline actor label") },
			{ TEXT("mesh_path"),  TEXT("string"), TEXT("Static mesh asset path") },
			{ TEXT("axis"),       TEXT("string"), TEXT("Forward axis: X, Y, or Z (default Z)"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return SplineAttachMesh(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// spline_query
	{
		FMCPToolDef Def;
		Def.Name = TEXT("spline_query");
		Def.Description = TEXT("Return all spline point positions, tangents, and total length.");
		Def.Params = {
			{ TEXT("name"), TEXT("string"), TEXT("Spline actor label") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return SplineQuery(A); };
		OutTools.Add(MoveTemp(Def));
	}
}

// ---------------------------------------------------------------------------
// Implementations
// ---------------------------------------------------------------------------

FMCPToolResult FMCPSplineTools::SplineCreate(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
		return FMCPToolResult::Error(TEXT("Missing required param: name"));

	UWorld* World = GetEditorWorldS();
	if (!World) return FMCPToolResult::Error(TEXT("No editor world"));

	FVector Location = FVector::ZeroVector;
	const TSharedPtr<FJsonObject>* LocObj;
	if (Args->TryGetObjectField(TEXT("location"), LocObj))
		Location = JsonToVec(*LocObj);

	FActorSpawnParameters Params;
	AActor* SplineActor = World->SpawnActor<AActor>(
		AActor::StaticClass(), FTransform(Location), Params);

	if (!SplineActor)
		return FMCPToolResult::Error(TEXT("Failed to spawn spline actor"));

	USplineComponent* SplineComp = NewObject<USplineComponent>(
		SplineActor, TEXT("SplineComponent"));
	SplineComp->RegisterComponent();
	SplineActor->AddInstanceComponent(SplineComp);
	SplineComp->SetWorldLocation(Location);

	SplineActor->SetActorLabel(Name);
	SplineActor->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created spline actor '%s' at (%.1f,%.1f,%.1f)"),
		*Name, Location.X, Location.Y, Location.Z));
}

FMCPToolResult FMCPSplineTools::SplineAddPoint(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
		return FMCPToolResult::Error(TEXT("Missing: name"));

	const TSharedPtr<FJsonObject>* LocObj;
	if (!Args->TryGetObjectField(TEXT("location"), LocObj))
		return FMCPToolResult::Error(TEXT("Missing: location"));

	UWorld* World = GetEditorWorldS();
	AActor* Actor = FindActorByLabelS(World, Name);
	if (!Actor) return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *Name));

	USplineComponent* Spline = GetSplineComp(Actor);
	if (!Spline) return FMCPToolResult::Error(TEXT("Actor has no SplineComponent"));

	FVector Loc = JsonToVec(*LocObj);
	Spline->AddSplineWorldPoint(Loc);
	Spline->UpdateSpline();
	Actor->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added point to '%s' at (%.1f,%.1f,%.1f). Total points: %d"),
		*Name, Loc.X, Loc.Y, Loc.Z, Spline->GetNumberOfSplinePoints()));
}

FMCPToolResult FMCPSplineTools::SplineModifyPoint(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
		return FMCPToolResult::Error(TEXT("Missing: name"));

	int32 Index = 0;
	if (!Args->HasField(TEXT("index")))
		return FMCPToolResult::Error(TEXT("Missing: index"));
	Index = (int32)Args->GetNumberField(TEXT("index"));

	UWorld* World = GetEditorWorldS();
	AActor* Actor = FindActorByLabelS(World, Name);
	if (!Actor) return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *Name));

	USplineComponent* Spline = GetSplineComp(Actor);
	if (!Spline) return FMCPToolResult::Error(TEXT("No SplineComponent"));

	if (Index < 0 || Index >= Spline->GetNumberOfSplinePoints())
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Index %d out of range (0-%d)"), Index, Spline->GetNumberOfSplinePoints()-1));

	const TSharedPtr<FJsonObject>* LocObj;
	if (Args->TryGetObjectField(TEXT("location"), LocObj))
		Spline->SetWorldLocationAtSplinePoint(Index, JsonToVec(*LocObj));

	const TSharedPtr<FJsonObject>* ArrObj;
	const TSharedPtr<FJsonObject>* LeavObj;
	if (Args->TryGetObjectField(TEXT("arrive_tangent"), ArrObj) ||
		Args->TryGetObjectField(TEXT("leave_tangent"),  LeavObj))
	{
		FVector Arrive = Spline->GetArriveTangentAtSplinePoint(Index, ESplineCoordinateSpace::World);
		FVector Leave  = Spline->GetLeaveTangentAtSplinePoint(Index,  ESplineCoordinateSpace::World);
		if (Args->TryGetObjectField(TEXT("arrive_tangent"), ArrObj))  Arrive = JsonToVec(*ArrObj);
		if (Args->TryGetObjectField(TEXT("leave_tangent"),  LeavObj)) Leave  = JsonToVec(*LeavObj);
		Spline->SetTangentsAtSplinePoint(Index, Arrive, Leave, ESplineCoordinateSpace::World);
	}

	Spline->UpdateSpline();
	Actor->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Modified spline point %d on '%s'"), Index, *Name));
}

FMCPToolResult FMCPSplineTools::SplineAttachMesh(const TSharedPtr<FJsonObject>& Args)
{
	FString Name, MeshPath;
	if (!Args->TryGetStringField(TEXT("name"),      Name))     return FMCPToolResult::Error(TEXT("Missing: name"));
	if (!Args->TryGetStringField(TEXT("mesh_path"), MeshPath)) return FMCPToolResult::Error(TEXT("Missing: mesh_path"));

	UWorld* World = GetEditorWorldS();
	AActor* Actor = FindActorByLabelS(World, Name);
	if (!Actor) return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *Name));

	USplineComponent* Spline = GetSplineComp(Actor);
	if (!Spline) return FMCPToolResult::Error(TEXT("No SplineComponent"));

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh) return FMCPToolResult::Error(FString::Printf(TEXT("Mesh not found: %s"), *MeshPath));

	// Determine forward axis
	FString AxisStr;
	Args->TryGetStringField(TEXT("axis"), AxisStr);
	ESplineMeshAxis::Type Axis = ESplineMeshAxis::Z;
	if (AxisStr == TEXT("X")) Axis = ESplineMeshAxis::X;
	else if (AxisStr == TEXT("Y")) Axis = ESplineMeshAxis::Y;

	// Remove old SplineMeshComponents first
	TArray<USplineMeshComponent*> Old;
	Actor->GetComponents<USplineMeshComponent>(Old);
	for (USplineMeshComponent* C : Old)
	{
		C->DestroyComponent();
	}

	int32 NumSegments = Spline->GetNumberOfSplinePoints() - 1;
	for (int32 i = 0; i < NumSegments; ++i)
	{
		USplineMeshComponent* SMC = NewObject<USplineMeshComponent>(Actor);
		SMC->SetStaticMesh(Mesh);
		SMC->SetMobility(EComponentMobility::Movable);
		SMC->SetForwardAxis(Axis);
		SMC->RegisterComponent();
		Actor->AddInstanceComponent(SMC);

		FVector StartPos = Spline->GetLocationAtSplinePoint(i,   ESplineCoordinateSpace::Local);
		FVector StartTan = Spline->GetTangentAtSplinePoint(i,   ESplineCoordinateSpace::Local);
		FVector EndPos   = Spline->GetLocationAtSplinePoint(i+1, ESplineCoordinateSpace::Local);
		FVector EndTan   = Spline->GetTangentAtSplinePoint(i+1, ESplineCoordinateSpace::Local);

		SMC->SetStartAndEnd(StartPos, StartTan, EndPos, EndTan, true);
	}

	Actor->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Attached mesh to %d segment(s) on '%s'"), NumSegments, *Name));
}

FMCPToolResult FMCPSplineTools::SplineQuery(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
		return FMCPToolResult::Error(TEXT("Missing: name"));

	UWorld* World = GetEditorWorldS();
	AActor* Actor = FindActorByLabelS(World, Name);
	if (!Actor) return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *Name));

	USplineComponent* Spline = GetSplineComp(Actor);
	if (!Spline) return FMCPToolResult::Error(TEXT("No SplineComponent"));

	TArray<TSharedPtr<FJsonValue>> Points;
	int32 NumPoints = Spline->GetNumberOfSplinePoints();
	for (int32 i = 0; i < NumPoints; ++i)
	{
		auto P = MakeShared<FJsonObject>();
		P->SetNumberField(TEXT("index"), i);
		P->SetObjectField(TEXT("location"),
			VecToJson(Spline->GetWorldLocationAtSplinePoint(i)));
		P->SetObjectField(TEXT("arrive_tangent"),
			VecToJson(Spline->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::World)));
		P->SetObjectField(TEXT("leave_tangent"),
			VecToJson(Spline->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::World)));
		Points.Add(MakeShared<FJsonValueObject>(P));
	}

	auto Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("points"), Points);
	Data->SetNumberField(TEXT("point_count"),   NumPoints);
	Data->SetNumberField(TEXT("length"),        Spline->GetSplineLength());
	Data->SetBoolField  (TEXT("closed"),        Spline->IsClosedLoop());

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Spline '%s': %d points, length=%.1f"), *Name, NumPoints, Spline->GetSplineLength()), Data);
}
