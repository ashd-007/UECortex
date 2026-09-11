#include "Tools/MCPVolumeTools.h"
#include "UECortexModule.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/TriggerVolume.h"
#include "Engine/BlockingVolume.h"
#include "Sound/AudioVolume.h"
#include "GameFramework/PhysicsVolume.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/BrushComponent.h"
#include "Dom/JsonObject.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static UWorld* GetEditorWorldV()
{
	return FUECortexModule::GetActiveWorld();
}

static AActor* FindActorByLabelV(UWorld* World, const FString& Label)
{
	if (!World) return nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == Label) return *It;
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMCPVolumeTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	// volume_create
	{
		FMCPToolDef Def;
		Def.Name = TEXT("volume_create");
		Def.Description = TEXT("Create a volume actor (trigger/blocking/audio/physics/navmesh) at a location with approximate box extents.");
		Def.Params = {
			{ TEXT("type"),     TEXT("string"), TEXT("Volume type: trigger | blocking | audio | physics | navmesh") },
			{ TEXT("name"),     TEXT("string"), TEXT("Actor label") },
			{ TEXT("location"), TEXT("object"), TEXT("{x,y,z} center location"), false },
			{ TEXT("extent"),   TEXT("object"), TEXT("{x,y,z} half-extents in cm (default 200,200,200)"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return VolumeCreate(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// volume_set_properties
	{
		FMCPToolDef Def;
		Def.Name = TEXT("volume_set_properties");
		Def.Description = TEXT("Configure properties on a volume actor (overlap events, priority, etc).");
		Def.Params = {
			{ TEXT("name"),                 TEXT("string"),  TEXT("Volume actor label") },
			{ TEXT("generate_overlap"),     TEXT("boolean"), TEXT("Enable overlap events on BrushComponent"), false },
			{ TEXT("priority"),             TEXT("number"),  TEXT("Physics/audio volume priority"), false },
			{ TEXT("hidden"),               TEXT("boolean"), TEXT("Hide the volume in editor"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return VolumeSetProperties(A); };
		OutTools.Add(MoveTemp(Def));
	}
}

// ---------------------------------------------------------------------------
// Implementations
// ---------------------------------------------------------------------------

FMCPToolResult FMCPVolumeTools::VolumeCreate(const TSharedPtr<FJsonObject>& Args)
{
	FString Type, Name;
	if (!Args->TryGetStringField(TEXT("type"), Type)) return FMCPToolResult::Error(TEXT("Missing: type"));
	if (!Args->TryGetStringField(TEXT("name"), Name)) return FMCPToolResult::Error(TEXT("Missing: name"));

	UWorld* World = GetEditorWorldV();
	if (!World) return FMCPToolResult::Error(TEXT("No editor world"));

	FVector Location = FVector::ZeroVector;
	FVector Extent(200.f, 200.f, 200.f);

	const TSharedPtr<FJsonObject>* LocObj;
	const TSharedPtr<FJsonObject>* ExtObj;
	if (Args->TryGetObjectField(TEXT("location"), LocObj))
	{
		Location = FVector(
			(*LocObj)->HasField(TEXT("x")) ? (*LocObj)->GetNumberField(TEXT("x")) : 0.0,
			(*LocObj)->HasField(TEXT("y")) ? (*LocObj)->GetNumberField(TEXT("y")) : 0.0,
			(*LocObj)->HasField(TEXT("z")) ? (*LocObj)->GetNumberField(TEXT("z")) : 0.0);
	}
	if (Args->TryGetObjectField(TEXT("extent"), ExtObj))
	{
		Extent = FVector(
			(*ExtObj)->HasField(TEXT("x")) ? (*ExtObj)->GetNumberField(TEXT("x")) : 200.0,
			(*ExtObj)->HasField(TEXT("y")) ? (*ExtObj)->GetNumberField(TEXT("y")) : 200.0,
			(*ExtObj)->HasField(TEXT("z")) ? (*ExtObj)->GetNumberField(TEXT("z")) : 200.0);
	}

	// Resolve class from type string
	UClass* VolumeClass = nullptr;
	FString TypeLower = Type.ToLower();
	if      (TypeLower == TEXT("trigger"))  VolumeClass = ATriggerVolume::StaticClass();
	else if (TypeLower == TEXT("blocking")) VolumeClass = ABlockingVolume::StaticClass();
	else if (TypeLower == TEXT("audio"))    VolumeClass = AAudioVolume::StaticClass();
	else if (TypeLower == TEXT("physics"))  VolumeClass = APhysicsVolume::StaticClass();
	else if (TypeLower == TEXT("navmesh"))  VolumeClass = ANavMeshBoundsVolume::StaticClass();
	else
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Unknown volume type '%s'. Use: trigger, blocking, audio, physics, navmesh"), *Type));
	}

	FActorSpawnParameters SpawnParams;
	AActor* Volume = World->SpawnActor(VolumeClass, &Location, nullptr, SpawnParams);
	if (!Volume)
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to spawn %s volume"), *Type));

	Volume->SetActorLabel(Name);

	// Scale the brush component to approximate the desired extents.
	// Default brush is 200x200x200, so scale = extent / 100 (half-extent).
	FVector Scale = Extent / 100.f;
	Volume->SetActorScale3D(Scale);
	Volume->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created %s volume '%s' at (%.1f,%.1f,%.1f) extent (%.1f,%.1f,%.1f)"),
		*Type, *Name,
		Location.X, Location.Y, Location.Z,
		Extent.X, Extent.Y, Extent.Z));
}

FMCPToolResult FMCPVolumeTools::VolumeSetProperties(const TSharedPtr<FJsonObject>& Args)
{
	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name))
		return FMCPToolResult::Error(TEXT("Missing: name"));

	UWorld* World = GetEditorWorldV();
	AActor* Actor = FindActorByLabelV(World, Name);
	if (!Actor) return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *Name));

	Actor->Modify();
	TArray<FString> Changes;

	bool bOverlap;
	if (Args->TryGetBoolField(TEXT("generate_overlap"), bOverlap))
	{
		UBrushComponent* Brush = Actor->FindComponentByClass<UBrushComponent>();
		if (Brush)
		{
			Brush->SetGenerateOverlapEvents(bOverlap);
			Changes.Add(FString::Printf(TEXT("generate_overlap=%s"), bOverlap ? TEXT("true") : TEXT("false")));
		}
	}

	bool bHidden;
	if (Args->TryGetBoolField(TEXT("hidden"), bHidden))
	{
		Actor->SetIsTemporarilyHiddenInEditor(bHidden);
		Changes.Add(FString::Printf(TEXT("hidden=%s"), bHidden ? TEXT("true") : TEXT("false")));
	}

	// Priority — applies to APhysicsVolume and AAudioVolume
	double Priority;
	if (Args->TryGetNumberField(TEXT("priority"), Priority))
	{
		if (APhysicsVolume* PV = Cast<APhysicsVolume>(Actor))
		{
			PV->Priority = (int32)Priority;
			Changes.Add(FString::Printf(TEXT("priority=%d"), PV->Priority));
		}
		else if (AAudioVolume* AV = Cast<AAudioVolume>(Actor))
		{
			AV->SetPriority((float)Priority);
			Changes.Add(FString::Printf(TEXT("priority=%.1f"), Priority));
		}
	}

	Actor->MarkPackageDirty();

	FString ChangeStr = Changes.Num() > 0 ? FString::Join(Changes, TEXT(", ")) : TEXT("no changes");
	return FMCPToolResult::Success(FString::Printf(
		TEXT("Updated volume '%s': %s"), *Name, *ChangeStr));
}
