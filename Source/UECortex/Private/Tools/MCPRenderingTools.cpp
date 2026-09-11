#include "Tools/MCPRenderingTools.h"
#include "MCPToolBase.h"
#include "UECortexModule.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Async/Async.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"

// ─────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────

static UWorld* GetEditorWorld()
{
	return FUECortexModule::GetActiveWorld();
}

static AActor* FindActorByName(UWorld* World, const FString& Name)
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetName() == Name || It->GetActorLabel() == Name)
			return *It;
	}
	return nullptr;
}

template<class T>
static T* FindActorOfClass(UWorld* World)
{
	for (TActorIterator<T> It(World); It; ++It)
		return *It;
	return nullptr;
}

// ─────────────────────────────────────────────
//  RegisterTools
// ─────────────────────────────────────────────

void FMCPRenderingTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	// --- render_set_nanite ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_set_nanite");
		T.Description = TEXT("Enable or disable Nanite on a static mesh component or actor");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Actor label or name"), true },
			{ TEXT("enabled"),    TEXT("boolean"), TEXT("true to enable Nanite, false to disable"), true },
			{ TEXT("fallback_relative_error"), TEXT("number"), TEXT("Nanite fallback relative error (default 1.0)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderSetNanite(A); };
		OutTools.Add(T);
	}

	// --- render_get_nanite_settings ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_get_nanite_settings");
		T.Description = TEXT("Get Nanite settings for a static mesh actor");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Actor label or name"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderGetNaniteSettings(A); };
		OutTools.Add(T);
	}

	// --- render_set_lumen ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_set_lumen");
		T.Description = TEXT("Configure Lumen global illumination and reflections on a PostProcessVolume");
		T.Params = {
			{ TEXT("gi_enabled"),          TEXT("boolean"), TEXT("Enable Lumen GI"), false },
			{ TEXT("reflections_enabled"), TEXT("boolean"), TEXT("Enable Lumen reflections"), false },
			{ TEXT("gi_quality"),          TEXT("number"),  TEXT("Lumen GI quality 0-4 (default 1)"), false },
			{ TEXT("reflections_quality"), TEXT("number"),  TEXT("Lumen reflections quality 0-4 (default 1)"), false },
			{ TEXT("volume_name"),         TEXT("string"),  TEXT("PostProcessVolume actor name (uses world settings volume if omitted)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderSetLumen(A); };
		OutTools.Add(T);
	}

	// --- render_get_lumen_settings ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_get_lumen_settings");
		T.Description = TEXT("Get current Lumen settings from a PostProcessVolume");
		T.Params = {
			{ TEXT("volume_name"), TEXT("string"), TEXT("PostProcessVolume actor name (optional)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderGetLumenSettings(A); };
		OutTools.Add(T);
	}

	// --- render_set_material ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_set_material");
		T.Description = TEXT("Assign a material to a mesh component on an actor");
		T.Params = {
			{ TEXT("actor_name"),    TEXT("string"), TEXT("Actor label or name"), true },
			{ TEXT("material_path"), TEXT("string"), TEXT("Asset path of the material e.g. '/Game/Materials/M_Rock'"), true },
			{ TEXT("slot_index"),    TEXT("number"), TEXT("Material slot index (default 0)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderSetMaterial(A); };
		OutTools.Add(T);
	}

	// --- render_get_materials ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_get_materials");
		T.Description = TEXT("Get all material slots on an actor's mesh component");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Actor label or name"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderGetMaterials(A); };
		OutTools.Add(T);
	}

	// --- render_create_material_instance ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_create_material_instance");
		T.Description = TEXT("Create a MaterialInstanceConstant from a parent material and save it as an asset");
		T.Params = {
			{ TEXT("parent_material_path"), TEXT("string"), TEXT("Asset path of the parent material"), true },
			{ TEXT("instance_name"),        TEXT("string"), TEXT("Name for the new material instance asset"), true },
			{ TEXT("save_path"),            TEXT("string"), TEXT("Content folder path e.g. '/Game/Materials' (default '/Game/Materials')"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderCreateMaterialInstance(A); };
		OutTools.Add(T);
	}

	// --- render_set_material_param ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_set_material_param");
		T.Description = TEXT("Set a scalar, vector, or texture parameter on a MaterialInstanceConstant");
		T.Params = {
			{ TEXT("material_path"), TEXT("string"), TEXT("Asset path of the MaterialInstanceConstant"), true },
			{ TEXT("param_name"),    TEXT("string"), TEXT("Parameter name"), true },
			{ TEXT("param_type"),    TEXT("string"), TEXT("'scalar', 'vector', or 'texture'"), true },
			{ TEXT("value"),         TEXT("string"), TEXT("For scalar: number. For vector: 'R,G,B,A'. For texture: asset path"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderSetMaterialParam(A); };
		OutTools.Add(T);
	}

	// --- render_set_post_process ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_set_post_process");
		T.Description = TEXT("Set post-process settings (bloom, exposure, color grading, vignette, etc.) on a PostProcessVolume");
		T.Params = {
			{ TEXT("volume_name"),          TEXT("string"),  TEXT("PostProcessVolume actor name (creates one if not found)"), false },
			{ TEXT("infinite_extent"),      TEXT("boolean"), TEXT("Make this an infinite/unbound volume"), false },
			{ TEXT("bloom_intensity"),      TEXT("number"),  TEXT("Bloom intensity (0 = disabled)"), false },
			{ TEXT("exposure_compensation"),TEXT("number"),  TEXT("Manual exposure EV100 offset"), false },
			{ TEXT("vignette_intensity"),   TEXT("number"),  TEXT("Vignette 0-1"), false },
			{ TEXT("saturation"),           TEXT("number"),  TEXT("Global saturation 0-2 (1 = neutral)"), false },
			{ TEXT("contrast"),             TEXT("number"),  TEXT("Global contrast 0-2 (1 = neutral)"), false },
			{ TEXT("gamma"),                TEXT("number"),  TEXT("Global gamma 0-2 (1 = neutral)"), false },
			{ TEXT("depth_of_field_fstop"), TEXT("number"),  TEXT("DoF f-stop (0 disables)"), false },
			{ TEXT("focal_distance"),       TEXT("number"),  TEXT("DoF focal distance in cm"), false },
			{ TEXT("motion_blur_amount"),   TEXT("number"),  TEXT("Motion blur amount 0-1"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderSetPostProcess(A); };
		OutTools.Add(T);
	}

	// --- render_get_post_process ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_get_post_process");
		T.Description = TEXT("Read current post-process settings from a PostProcessVolume");
		T.Params = {
			{ TEXT("volume_name"), TEXT("string"), TEXT("PostProcessVolume actor name (optional, reads first found)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderGetPostProcess(A); };
		OutTools.Add(T);
	}

	// --- render_set_sky_light ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_set_sky_light");
		T.Description = TEXT("Configure the SkyLight in the level");
		T.Params = {
			{ TEXT("intensity"),        TEXT("number"),  TEXT("Sky light intensity multiplier"), false },
			{ TEXT("cast_shadows"),     TEXT("boolean"), TEXT("Enable sky light shadows"), false },
			{ TEXT("recapture"),        TEXT("boolean"), TEXT("Force sky light recapture"), false },
			{ TEXT("real_time_capture"),TEXT("boolean"), TEXT("Enable real-time sky light capture"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderSetSkyLight(A); };
		OutTools.Add(T);
	}

	// --- render_set_fog ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_set_fog");
		T.Description = TEXT("Configure ExponentialHeightFog in the level");
		T.Params = {
			{ TEXT("density"),           TEXT("number"), TEXT("Fog density (default 0.02)"), false },
			{ TEXT("height_falloff"),    TEXT("number"), TEXT("Height falloff (default 0.2)"), false },
			{ TEXT("start_distance"),    TEXT("number"), TEXT("Fog start distance in cm"), false },
			{ TEXT("inscattering_color"),TEXT("string"), TEXT("Fog color 'R,G,B' (0-1)"), false },
			{ TEXT("enabled"),           TEXT("boolean"),TEXT("Show/hide the fog actor"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderSetFog(A); };
		OutTools.Add(T);
	}

	// --- render_set_sky_atmosphere ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_set_sky_atmosphere");
		T.Description = TEXT("Configure SkyAtmosphere component settings");
		T.Params = {
			{ TEXT("rayleigh_scattering_scale"), TEXT("number"), TEXT("Rayleigh scattering scale (default 0.0331)"), false },
			{ TEXT("mie_scattering_scale"),      TEXT("number"), TEXT("Mie scattering scale (default 0.003996)"), false },
			{ TEXT("atmosphere_height"),         TEXT("number"), TEXT("Atmosphere height in km (default 60)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderSetSkyAtmosphere(A); };
		OutTools.Add(T);
	}

	// --- render_set_shadows ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_set_shadows");
		T.Description = TEXT("Configure shadow casting on an actor's mesh component");
		T.Params = {
			{ TEXT("actor_name"),           TEXT("string"),  TEXT("Actor label or name"), true },
			{ TEXT("cast_shadows"),         TEXT("boolean"), TEXT("Enable shadow casting"), false },
			{ TEXT("cast_dynamic_shadows"), TEXT("boolean"), TEXT("Enable dynamic shadow casting"), false },
			{ TEXT("shadow_bias"),          TEXT("number"),  TEXT("Shadow bias"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderSetShadows(A); };
		OutTools.Add(T);
	}

	// --- render_set_ambient_occlusion ---
	{
		FMCPToolDef T;
		T.Name = TEXT("render_set_ambient_occlusion");
		T.Description = TEXT("Set ambient occlusion settings on a PostProcessVolume");
		T.Params = {
			{ TEXT("volume_name"), TEXT("string"),  TEXT("PostProcessVolume name (optional)"), false },
			{ TEXT("intensity"),   TEXT("number"),  TEXT("AO intensity 0-1"), false },
			{ TEXT("radius"),      TEXT("number"),  TEXT("AO radius in world units"), false },
			{ TEXT("enabled"),     TEXT("boolean"), TEXT("Enable/disable AO"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return RenderSetAmbientOcclusion(A); };
		OutTools.Add(T);
	}
}

// ─────────────────────────────────────────────
//  Nanite
// ─────────────────────────────────────────────

FMCPToolResult FMCPRenderingTools::RenderSetNanite(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName;
	bool bEnabled = true;
	double FallbackError = 1.0;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("actor_name required"));
	Args->TryGetBoolField(TEXT("enabled"), bEnabled);
	Args->TryGetNumberField(TEXT("fallback_relative_error"), FallbackError);

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, bEnabled, FallbackError, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByName(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UStaticMeshComponent* SMC = Actor->FindComponentByClass<UStaticMeshComponent>();
		if (!SMC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponent"), *ActorName)); return; }

		UStaticMesh* Mesh = SMC->GetStaticMesh();
		if (!Mesh) { Result = FMCPToolResult::Error(TEXT("No static mesh assigned")); return; }

		Mesh->Modify();
		Mesh->NaniteSettings.bEnabled = bEnabled;
		Mesh->NaniteSettings.FallbackRelativeError = (float)FallbackError;
		Mesh->PostEditChange();

		auto Data = MakeShared<FJsonObject>();
		Data->SetBoolField(TEXT("nanite_enabled"), bEnabled);
		Data->SetNumberField(TEXT("fallback_relative_error"), FallbackError);
		Data->SetStringField(TEXT("mesh"), Mesh->GetPathName());
		Result = FMCPToolResult::Success(
			FString::Printf(TEXT("Nanite %s on '%s'"), bEnabled ? TEXT("enabled") : TEXT("disabled"), *ActorName), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPRenderingTools::RenderGetNaniteSettings(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("actor_name required"));

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByName(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UStaticMeshComponent* SMC = Actor->FindComponentByClass<UStaticMeshComponent>();
		if (!SMC) { Result = FMCPToolResult::Error(TEXT("No StaticMeshComponent")); return; }

		UStaticMesh* Mesh = SMC->GetStaticMesh();
		if (!Mesh) { Result = FMCPToolResult::Error(TEXT("No static mesh")); return; }

		auto Data = MakeShared<FJsonObject>();
		Data->SetBoolField(TEXT("nanite_enabled"), Mesh->NaniteSettings.bEnabled);
		Data->SetNumberField(TEXT("fallback_relative_error"), Mesh->NaniteSettings.FallbackRelativeError);
		Data->SetStringField(TEXT("mesh"), Mesh->GetPathName());
		Result = FMCPToolResult::Success(TEXT("Nanite settings retrieved"), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

// ─────────────────────────────────────────────
//  Lumen
// ─────────────────────────────────────────────

static APostProcessVolume* FindOrCreatePPV(UWorld* World, const FString& VolumeName)
{
	if (!VolumeName.IsEmpty())
	{
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (It->GetName() == VolumeName || It->GetActorLabel() == VolumeName)
				return *It;
		}
	}
	// Return first found, or null
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		return *It;
	return nullptr;
}

FMCPToolResult FMCPRenderingTools::RenderSetLumen(const TSharedPtr<FJsonObject>& Args)
{
	FString VolumeName;
	Args->TryGetStringField(TEXT("volume_name"), VolumeName);

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [Args, VolumeName, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		APostProcessVolume* PPV = FindOrCreatePPV(World, VolumeName);
		if (!PPV)
		{
			// Spawn one
			PPV = World->SpawnActor<APostProcessVolume>();
			if (!PPV) { Result = FMCPToolResult::Error(TEXT("Failed to create PostProcessVolume")); return; }
			PPV->bUnbound = true;
			PPV->SetActorLabel(TEXT("MCP_PostProcessVolume"));
		}

		PPV->Modify();

		bool bGIEnabled;
		if (Args->TryGetBoolField(TEXT("gi_enabled"), bGIEnabled))
		{
			PPV->Settings.bOverride_DynamicGlobalIlluminationMethod = true;
			PPV->Settings.DynamicGlobalIlluminationMethod = bGIEnabled
				? EDynamicGlobalIlluminationMethod::Lumen
				: EDynamicGlobalIlluminationMethod::None;
		}

		bool bReflEnabled;
		if (Args->TryGetBoolField(TEXT("reflections_enabled"), bReflEnabled))
		{
			PPV->Settings.bOverride_ReflectionMethod = true;
			PPV->Settings.ReflectionMethod = bReflEnabled
				? EReflectionMethod::Lumen
				: EReflectionMethod::None;
		}

		double GIQuality;
		if (Args->TryGetNumberField(TEXT("gi_quality"), GIQuality))
		{
			PPV->Settings.bOverride_LumenSceneLightingQuality = true;
			PPV->Settings.LumenSceneLightingQuality = (float)GIQuality;
		}

		double ReflQuality;
		if (Args->TryGetNumberField(TEXT("reflections_quality"), ReflQuality))
		{
			PPV->Settings.bOverride_LumenReflectionQuality = true;
			PPV->Settings.LumenReflectionQuality = (float)ReflQuality;
		}

		PPV->PostEditChange();

		auto Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("volume"), PPV->GetActorLabel());
		Result = FMCPToolResult::Success(TEXT("Lumen settings updated"), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPRenderingTools::RenderGetLumenSettings(const TSharedPtr<FJsonObject>& Args)
{
	FString VolumeName;
	Args->TryGetStringField(TEXT("volume_name"), VolumeName);

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [VolumeName, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		APostProcessVolume* PPV = FindOrCreatePPV(World, VolumeName);
		if (!PPV) { Result = FMCPToolResult::Error(TEXT("No PostProcessVolume found")); return; }

		auto Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("volume"), PPV->GetActorLabel());
		Data->SetStringField(TEXT("gi_method"), PPV->Settings.bOverride_DynamicGlobalIlluminationMethod
			? (PPV->Settings.DynamicGlobalIlluminationMethod == EDynamicGlobalIlluminationMethod::Lumen ? TEXT("Lumen") : TEXT("None"))
			: TEXT("(not overridden)"));
		Data->SetStringField(TEXT("reflection_method"), PPV->Settings.bOverride_ReflectionMethod
			? (PPV->Settings.ReflectionMethod == EReflectionMethod::Lumen ? TEXT("Lumen") : TEXT("None"))
			: TEXT("(not overridden)"));
		Data->SetNumberField(TEXT("gi_quality"), PPV->Settings.LumenSceneLightingQuality);
		Data->SetNumberField(TEXT("reflections_quality"), PPV->Settings.LumenReflectionQuality);
		Result = FMCPToolResult::Success(TEXT("Lumen settings retrieved"), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

// ─────────────────────────────────────────────
//  Materials
// ─────────────────────────────────────────────

FMCPToolResult FMCPRenderingTools::RenderSetMaterial(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName, MaterialPath;
	double SlotIndex = 0;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("actor_name required"));
	if (!Args->TryGetStringField(TEXT("material_path"), MaterialPath))
		return FMCPToolResult::Error(TEXT("material_path required"));
	Args->TryGetNumberField(TEXT("slot_index"), SlotIndex);

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, MaterialPath, SlotIndex, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByName(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UMeshComponent* Mesh = Actor->FindComponentByClass<UMeshComponent>();
		if (!Mesh) { Result = FMCPToolResult::Error(TEXT("No mesh component found")); return; }

		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!Mat) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Material '%s' not found"), *MaterialPath)); return; }

		Mesh->SetMaterial((int32)SlotIndex, Mat);
		Mesh->Modify();

		auto Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("material"), MaterialPath);
		Data->SetNumberField(TEXT("slot"), SlotIndex);
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Set material on '%s' slot %d"), *ActorName, (int32)SlotIndex), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPRenderingTools::RenderGetMaterials(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("actor_name required"));

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByName(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UMeshComponent* Mesh = Actor->FindComponentByClass<UMeshComponent>();
		if (!Mesh) { Result = FMCPToolResult::Error(TEXT("No mesh component found")); return; }

		TArray<TSharedPtr<FJsonValue>> MatArr;
		for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
		{
			UMaterialInterface* Mat = Mesh->GetMaterial(i);
			auto Obj = MakeShared<FJsonObject>();
			Obj->SetNumberField(TEXT("slot"), i);
			Obj->SetStringField(TEXT("material"), Mat ? Mat->GetPathName() : TEXT("None"));
			Obj->SetStringField(TEXT("slot_name"), Mesh->GetMaterialSlotNames().IsValidIndex(i)
				? Mesh->GetMaterialSlotNames()[i].ToString() : TEXT(""));
			MatArr.Add(MakeShared<FJsonValueObject>(Obj));
		}

		auto Data = MakeShared<FJsonObject>();
		Data->SetArrayField(TEXT("materials"), MatArr);
		Data->SetNumberField(TEXT("count"), MatArr.Num());
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Found %d material slots"), MatArr.Num()), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPRenderingTools::RenderCreateMaterialInstance(const TSharedPtr<FJsonObject>& Args)
{
	FString ParentPath, InstanceName, SavePath;
	if (!Args->TryGetStringField(TEXT("parent_material_path"), ParentPath))
		return FMCPToolResult::Error(TEXT("parent_material_path required"));
	if (!Args->TryGetStringField(TEXT("instance_name"), InstanceName))
		return FMCPToolResult::Error(TEXT("instance_name required"));
	if (!Args->TryGetStringField(TEXT("save_path"), SavePath))
		SavePath = TEXT("/Game/Materials");

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ParentPath, InstanceName, SavePath, &Result]()
	{
		UMaterial* ParentMat = LoadObject<UMaterial>(nullptr, *ParentPath);
		if (!ParentMat)
		{
			// Also try as MaterialInterface (could be an MI itself)
			UMaterialInterface* MatIface = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
			if (!MatIface) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Parent material '%s' not found"), *ParentPath)); return; }
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = LoadObject<UMaterialInterface>(nullptr, *ParentPath);

		FString PackagePath = SavePath / InstanceName;
		UObject* NewAsset = AssetTools.CreateAsset(InstanceName, SavePath, UMaterialInstanceConstant::StaticClass(), Factory);
		if (!NewAsset) { Result = FMCPToolResult::Error(TEXT("Failed to create material instance asset")); return; }

		auto Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("asset_path"), NewAsset->GetPathName());
		Data->SetStringField(TEXT("parent"), ParentPath);
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Created material instance '%s'"), *InstanceName), Data);
	});
	FPlatformProcess::Sleep(0.1f);
	return Result;
}

FMCPToolResult FMCPRenderingTools::RenderSetMaterialParam(const TSharedPtr<FJsonObject>& Args)
{
	FString MatPath, ParamName, ParamType, ValueStr;
	if (!Args->TryGetStringField(TEXT("material_path"), MatPath))   return FMCPToolResult::Error(TEXT("material_path required"));
	if (!Args->TryGetStringField(TEXT("param_name"), ParamName))    return FMCPToolResult::Error(TEXT("param_name required"));
	if (!Args->TryGetStringField(TEXT("param_type"), ParamType))    return FMCPToolResult::Error(TEXT("param_type required"));
	if (!Args->TryGetStringField(TEXT("value"), ValueStr))          return FMCPToolResult::Error(TEXT("value required"));

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [MatPath, ParamName, ParamType, ValueStr, &Result]()
	{
		UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *MatPath);
		if (!MIC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("MaterialInstanceConstant '%s' not found"), *MatPath)); return; }

		MIC->Modify();

		if (ParamType.Equals(TEXT("scalar"), ESearchCase::IgnoreCase))
		{
			float Val = FCString::Atof(*ValueStr);
			MIC->SetScalarParameterValueEditorOnly(FName(*ParamName), Val);
			Result = FMCPToolResult::Success(FString::Printf(TEXT("Set scalar '%s' = %f"), *ParamName, Val));
		}
		else if (ParamType.Equals(TEXT("vector"), ESearchCase::IgnoreCase))
		{
			TArray<FString> Parts;
			ValueStr.ParseIntoArray(Parts, TEXT(","));
			FLinearColor Color(
				Parts.IsValidIndex(0) ? FCString::Atof(*Parts[0]) : 0.f,
				Parts.IsValidIndex(1) ? FCString::Atof(*Parts[1]) : 0.f,
				Parts.IsValidIndex(2) ? FCString::Atof(*Parts[2]) : 0.f,
				Parts.IsValidIndex(3) ? FCString::Atof(*Parts[3]) : 1.f
			);
			MIC->SetVectorParameterValueEditorOnly(FName(*ParamName), Color);
			Result = FMCPToolResult::Success(FString::Printf(TEXT("Set vector '%s' = (%s)"), *ParamName, *ValueStr));
		}
		else if (ParamType.Equals(TEXT("texture"), ESearchCase::IgnoreCase))
		{
			UTexture* Tex = LoadObject<UTexture>(nullptr, *ValueStr);
			if (!Tex) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Texture '%s' not found"), *ValueStr)); return; }
			MIC->SetTextureParameterValueEditorOnly(FName(*ParamName), Tex);
			Result = FMCPToolResult::Success(FString::Printf(TEXT("Set texture '%s' = '%s'"), *ParamName, *ValueStr));
		}
		else
		{
			Result = FMCPToolResult::Error(FString::Printf(TEXT("Unknown param_type '%s' (use scalar/vector/texture)"), *ParamType));
		}

		MIC->PostEditChange();
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

// ─────────────────────────────────────────────
//  Post Process
// ─────────────────────────────────────────────

FMCPToolResult FMCPRenderingTools::RenderSetPostProcess(const TSharedPtr<FJsonObject>& Args)
{
	FString VolumeName;
	Args->TryGetStringField(TEXT("volume_name"), VolumeName);

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [Args, VolumeName, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		APostProcessVolume* PPV = FindOrCreatePPV(World, VolumeName);
		if (!PPV)
		{
			PPV = World->SpawnActor<APostProcessVolume>();
			if (!PPV) { Result = FMCPToolResult::Error(TEXT("Failed to spawn PostProcessVolume")); return; }
			PPV->SetActorLabel(VolumeName.IsEmpty() ? TEXT("MCP_PostProcessVolume") : *VolumeName);
		}

		PPV->Modify();

		bool bInfinite;
		if (Args->TryGetBoolField(TEXT("infinite_extent"), bInfinite))
			PPV->bUnbound = bInfinite;

		double BloomIntensity;
		if (Args->TryGetNumberField(TEXT("bloom_intensity"), BloomIntensity))
		{
			PPV->Settings.bOverride_BloomIntensity = true;
			PPV->Settings.BloomIntensity = (float)BloomIntensity;
		}

		double ExposureComp;
		if (Args->TryGetNumberField(TEXT("exposure_compensation"), ExposureComp))
		{
			PPV->Settings.bOverride_AutoExposureBias = true;
			PPV->Settings.AutoExposureBias = (float)ExposureComp;
		}

		double Vignette;
		if (Args->TryGetNumberField(TEXT("vignette_intensity"), Vignette))
		{
			PPV->Settings.bOverride_VignetteIntensity = true;
			PPV->Settings.VignetteIntensity = (float)Vignette;
		}

		double Saturation;
		if (Args->TryGetNumberField(TEXT("saturation"), Saturation))
		{
			PPV->Settings.bOverride_ColorSaturation = true;
			PPV->Settings.ColorSaturation = FVector4((float)Saturation, (float)Saturation, (float)Saturation, 1.f);
		}

		double Contrast;
		if (Args->TryGetNumberField(TEXT("contrast"), Contrast))
		{
			PPV->Settings.bOverride_ColorContrast = true;
			PPV->Settings.ColorContrast = FVector4((float)Contrast, (float)Contrast, (float)Contrast, 1.f);
		}

		double Gamma;
		if (Args->TryGetNumberField(TEXT("gamma"), Gamma))
		{
			PPV->Settings.bOverride_ColorGamma = true;
			PPV->Settings.ColorGamma = FVector4((float)Gamma, (float)Gamma, (float)Gamma, 1.f);
		}

		double FStop;
		if (Args->TryGetNumberField(TEXT("depth_of_field_fstop"), FStop))
		{
			PPV->Settings.bOverride_DepthOfFieldFstop = true;
			PPV->Settings.DepthOfFieldFstop = (float)FStop;
		}

		double FocalDist;
		if (Args->TryGetNumberField(TEXT("focal_distance"), FocalDist))
		{
			PPV->Settings.bOverride_DepthOfFieldFocalDistance = true;
			PPV->Settings.DepthOfFieldFocalDistance = (float)FocalDist;
		}

		double MotionBlur;
		if (Args->TryGetNumberField(TEXT("motion_blur_amount"), MotionBlur))
		{
			PPV->Settings.bOverride_MotionBlurAmount = true;
			PPV->Settings.MotionBlurAmount = (float)MotionBlur;
		}

		PPV->PostEditChange();

		auto Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("volume"), PPV->GetActorLabel());
		Result = FMCPToolResult::Success(TEXT("Post process settings updated"), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPRenderingTools::RenderGetPostProcess(const TSharedPtr<FJsonObject>& Args)
{
	FString VolumeName;
	Args->TryGetStringField(TEXT("volume_name"), VolumeName);

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [VolumeName, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		APostProcessVolume* PPV = FindOrCreatePPV(World, VolumeName);
		if (!PPV) { Result = FMCPToolResult::Error(TEXT("No PostProcessVolume found")); return; }

		auto Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("volume"), PPV->GetActorLabel());
		Data->SetBoolField(TEXT("infinite_extent"), PPV->bUnbound);
		Data->SetNumberField(TEXT("bloom_intensity"), PPV->Settings.BloomIntensity);
		Data->SetNumberField(TEXT("exposure_compensation"), PPV->Settings.AutoExposureBias);
		Data->SetNumberField(TEXT("vignette_intensity"), PPV->Settings.VignetteIntensity);
		Data->SetNumberField(TEXT("motion_blur_amount"), PPV->Settings.MotionBlurAmount);
		Data->SetNumberField(TEXT("dof_fstop"), PPV->Settings.DepthOfFieldFstop);
		Data->SetNumberField(TEXT("dof_focal_distance"), PPV->Settings.DepthOfFieldFocalDistance);
		Result = FMCPToolResult::Success(TEXT("Post process settings retrieved"), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

// ─────────────────────────────────────────────
//  Lighting
// ─────────────────────────────────────────────

FMCPToolResult FMCPRenderingTools::RenderSetSkyLight(const TSharedPtr<FJsonObject>& Args)
{
	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [Args, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		ASkyLight* SkyLight = FindActorOfClass<ASkyLight>(World);
		if (!SkyLight) { Result = FMCPToolResult::Error(TEXT("No SkyLight found in level")); return; }

		USkyLightComponent* SLC = SkyLight->GetLightComponent();
		if (!SLC) { Result = FMCPToolResult::Error(TEXT("SkyLight has no component")); return; }

		SkyLight->Modify();

		double Intensity;
		if (Args->TryGetNumberField(TEXT("intensity"), Intensity))
			SLC->Intensity = (float)Intensity;

		bool bCastShadows;
		if (Args->TryGetBoolField(TEXT("cast_shadows"), bCastShadows))
			SLC->CastShadows = bCastShadows;

		bool bRealTime;
		if (Args->TryGetBoolField(TEXT("real_time_capture"), bRealTime))
			SLC->bRealTimeCapture = bRealTime;

		bool bRecapture;
		if (Args->TryGetBoolField(TEXT("recapture"), bRecapture))
		{
			if (bRecapture)
				SLC->RecaptureSky();
		}

		SLC->MarkRenderStateDirty();
		SkyLight->PostEditChange();

		auto Data = MakeShared<FJsonObject>();
		Data->SetNumberField(TEXT("intensity"), SLC->Intensity);
		Data->SetBoolField(TEXT("cast_shadows"), SLC->CastShadows);
		Data->SetBoolField(TEXT("real_time_capture"), SLC->bRealTimeCapture);
		Result = FMCPToolResult::Success(TEXT("SkyLight updated"), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPRenderingTools::RenderSetFog(const TSharedPtr<FJsonObject>& Args)
{
	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [Args, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AExponentialHeightFog* FogActor = FindActorOfClass<AExponentialHeightFog>(World);
		if (!FogActor)
		{
			FogActor = World->SpawnActor<AExponentialHeightFog>();
			if (!FogActor) { Result = FMCPToolResult::Error(TEXT("Failed to create ExponentialHeightFog")); return; }
			FogActor->SetActorLabel(TEXT("MCP_HeightFog"));
		}

		UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();
		if (!FogComp) { Result = FMCPToolResult::Error(TEXT("Fog has no component")); return; }

		FogActor->Modify();

		double Density;
		if (Args->TryGetNumberField(TEXT("density"), Density))
			FogComp->SetFogDensity((float)Density);

		double HeightFalloff;
		if (Args->TryGetNumberField(TEXT("height_falloff"), HeightFalloff))
			FogComp->SetFogHeightFalloff((float)HeightFalloff);

		double StartDist;
		if (Args->TryGetNumberField(TEXT("start_distance"), StartDist))
			FogComp->SetStartDistance((float)StartDist);

		FString ColorStr;
		if (Args->TryGetStringField(TEXT("inscattering_color"), ColorStr))
		{
			TArray<FString> Parts;
			ColorStr.ParseIntoArray(Parts, TEXT(","));
			FLinearColor C(
				Parts.IsValidIndex(0) ? FCString::Atof(*Parts[0]) : 1.f,
				Parts.IsValidIndex(1) ? FCString::Atof(*Parts[1]) : 1.f,
				Parts.IsValidIndex(2) ? FCString::Atof(*Parts[2]) : 1.f
			);
			FogComp->SetFogInscatteringColor(C);
		}

		bool bEnabled;
		if (Args->TryGetBoolField(TEXT("enabled"), bEnabled))
			FogActor->SetActorHiddenInGame(!bEnabled);

		FogComp->MarkRenderStateDirty();
		FogActor->PostEditChange();

		auto Data = MakeShared<FJsonObject>();
		Data->SetNumberField(TEXT("density"), FogComp->FogDensity);
		Data->SetNumberField(TEXT("height_falloff"), FogComp->FogHeightFalloff);
		Result = FMCPToolResult::Success(TEXT("Fog updated"), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPRenderingTools::RenderSetSkyAtmosphere(const TSharedPtr<FJsonObject>& Args)
{
	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [Args, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		// Find actor with SkyAtmosphereComponent
		USkyAtmosphereComponent* SkyAtm = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			SkyAtm = It->FindComponentByClass<USkyAtmosphereComponent>();
			if (SkyAtm) break;
		}
		if (!SkyAtm) { Result = FMCPToolResult::Error(TEXT("No SkyAtmosphereComponent found in level")); return; }

		SkyAtm->GetOwner()->Modify();

		double RayleighScale;
		if (Args->TryGetNumberField(TEXT("rayleigh_scattering_scale"), RayleighScale))
			SkyAtm->RayleighScatteringScale = (float)RayleighScale;

		double MieScale;
		if (Args->TryGetNumberField(TEXT("mie_scattering_scale"), MieScale))
			SkyAtm->MieScatteringScale = (float)MieScale;

		double AtmHeight;
		if (Args->TryGetNumberField(TEXT("atmosphere_height"), AtmHeight))
			SkyAtm->AtmosphereHeight = (float)AtmHeight;

		SkyAtm->MarkRenderStateDirty();
		SkyAtm->GetOwner()->PostEditChange();

		auto Data = MakeShared<FJsonObject>();
		Data->SetNumberField(TEXT("rayleigh_scale"), SkyAtm->RayleighScatteringScale);
		Data->SetNumberField(TEXT("mie_scale"), SkyAtm->MieScatteringScale);
		Data->SetNumberField(TEXT("atmosphere_height_km"), SkyAtm->AtmosphereHeight);
		Result = FMCPToolResult::Success(TEXT("SkyAtmosphere updated"), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

// ─────────────────────────────────────────────
//  Shadows & AO
// ─────────────────────────────────────────────

FMCPToolResult FMCPRenderingTools::RenderSetShadows(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("actor_name required"));

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [Args, ActorName, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByName(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UPrimitiveComponent* Prim = Actor->FindComponentByClass<UPrimitiveComponent>();
		if (!Prim) { Result = FMCPToolResult::Error(TEXT("No primitive component found")); return; }

		Prim->Modify();

		bool bCast;
		if (Args->TryGetBoolField(TEXT("cast_shadows"), bCast))
			Prim->SetCastShadow(bCast);

		bool bDynamic;
		if (Args->TryGetBoolField(TEXT("cast_dynamic_shadows"), bDynamic))
			Prim->bCastDynamicShadow = bDynamic;

		double Bias;
		if (Args->TryGetNumberField(TEXT("shadow_bias"), Bias))
			Prim->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Auto; // shadow_bias not directly settable via component

		Prim->MarkRenderStateDirty();

		auto Data = MakeShared<FJsonObject>();
		Data->SetBoolField(TEXT("cast_shadow"), Prim->CastShadow);
		Data->SetBoolField(TEXT("cast_dynamic_shadow"), Prim->bCastDynamicShadow);
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Shadow settings updated on '%s'"), *ActorName), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPRenderingTools::RenderSetAmbientOcclusion(const TSharedPtr<FJsonObject>& Args)
{
	FString VolumeName;
	Args->TryGetStringField(TEXT("volume_name"), VolumeName);

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [Args, VolumeName, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		APostProcessVolume* PPV = FindOrCreatePPV(World, VolumeName);
		if (!PPV)
		{
			PPV = World->SpawnActor<APostProcessVolume>();
			if (!PPV) { Result = FMCPToolResult::Error(TEXT("Failed to spawn PostProcessVolume")); return; }
			PPV->bUnbound = true;
			PPV->SetActorLabel(TEXT("MCP_PostProcessVolume"));
		}

		PPV->Modify();

		double Intensity;
		if (Args->TryGetNumberField(TEXT("intensity"), Intensity))
		{
			PPV->Settings.bOverride_AmbientOcclusionIntensity = true;
			PPV->Settings.AmbientOcclusionIntensity = (float)Intensity;
		}

		double Radius;
		if (Args->TryGetNumberField(TEXT("radius"), Radius))
		{
			PPV->Settings.bOverride_AmbientOcclusionRadius = true;
			PPV->Settings.AmbientOcclusionRadius = (float)Radius;
		}

		bool bEnabled;
		if (Args->TryGetBoolField(TEXT("enabled"), bEnabled))
		{
			PPV->Settings.bOverride_AmbientOcclusionIntensity = true;
			if (!bEnabled) PPV->Settings.AmbientOcclusionIntensity = 0.f;
		}

		PPV->PostEditChange();

		auto Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("volume"), PPV->GetActorLabel());
		Data->SetNumberField(TEXT("ao_intensity"), PPV->Settings.AmbientOcclusionIntensity);
		Data->SetNumberField(TEXT("ao_radius"), PPV->Settings.AmbientOcclusionRadius);
		Result = FMCPToolResult::Success(TEXT("Ambient occlusion settings updated"), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}
