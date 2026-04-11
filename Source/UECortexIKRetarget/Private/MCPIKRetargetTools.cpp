#include "MCPIKRetargetTools.h"
#include "MCPToolRegistry.h"

#include "Retargeter/IKRetargeter.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Rig/IKRigDefinition.h"
#include "Rig/IKRigSkeleton.h"
#include "RigEditor/IKRigController.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "Engine/SkeletalMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "FileHelpers.h"
#include "Dom/JsonObject.h"

// ---------------------------------------------------------------------------
// NOTE: HTTP handlers are called from the game thread tick.
// Do NOT use AsyncTask(GameThread) + FEvent — deadlocks. Call UE APIs directly.
// ---------------------------------------------------------------------------

void FMCPIKRetargetTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	FMCPToolDef T;
	T.Name = TEXT("ik_retarget_create");
	T.Description = TEXT(
		"Create and configure an IK Retargeter asset in one call. "
		"Sets source/target IK rigs, optional preview meshes, and auto-maps chains. "
		"Saves the asset on completion.");
	T.Params = {
		{ TEXT("path"),           TEXT("string"), TEXT("Content path for the new asset, e.g. /Game/Animation/RTG_HeroToNPC"), true },
		{ TEXT("source_rig"),     TEXT("string"), TEXT("Content path to the source IKRig asset"), true },
		{ TEXT("target_rig"),     TEXT("string"), TEXT("Content path to the target IKRig asset"), true },
		{ TEXT("source_mesh"),    TEXT("string"), TEXT("Content path to the source preview SkeletalMesh (optional)"), false },
		{ TEXT("target_mesh"),    TEXT("string"), TEXT("Content path to the target preview SkeletalMesh (optional)"), false },
		{ TEXT("auto_map"),       TEXT("string"), TEXT("Chain auto-map mode: 'fuzzy' (default) or 'exact'"), false },
	};
	T.Handler = [](const TSharedPtr<FJsonObject>& P) { return IKRetargetCreate(P); };
	OutTools.Add(T);

	{
		FMCPToolDef R;
		R.Name = TEXT("ik_rig_set_retarget_root");
		R.Description = TEXT("Set the retarget root bone on an IK Rig asset. Must be called before adding retarget chains.");
		R.Params = {
			{ TEXT("ik_rig_path"), TEXT("string"), TEXT("Content path to the IKRig asset"), true },
			{ TEXT("root_bone"),   TEXT("string"), TEXT("Name of the bone to use as the retarget root"), true },
		};
		R.Handler = [](const TSharedPtr<FJsonObject>& P) { return IKRigSetRetargetRoot(P); };
		OutTools.Add(R);
	}
	{
		FMCPToolDef R;
		R.Name = TEXT("ik_rig_add_retarget_chain");
		R.Description = TEXT("Add a retarget chain to an IK Rig. Validates that start and end bones exist before adding.");
		R.Params = {
			{ TEXT("ik_rig_path"),  TEXT("string"), TEXT("Content path to the IKRig asset"), true },
			{ TEXT("chain_name"),   TEXT("string"), TEXT("Name for the retarget chain, e.g. 'Spine', 'LeftArm'"), true },
			{ TEXT("start_bone"),   TEXT("string"), TEXT("Name of the chain's start bone"), true },
			{ TEXT("end_bone"),     TEXT("string"), TEXT("Name of the chain's end bone"), true },
			{ TEXT("ik_goal"),      TEXT("string"), TEXT("Name of the IK goal to associate with this chain (optional)"), false },
		};
		R.Handler = [](const TSharedPtr<FJsonObject>& P) { return IKRigAddRetargetChain(P); };
		OutTools.Add(R);
	}
	{
		FMCPToolDef R;
		R.Name = TEXT("ik_rig_remove_retarget_chain");
		R.Description = TEXT("Remove a retarget chain from an IK Rig by name.");
		R.Params = {
			{ TEXT("ik_rig_path"), TEXT("string"), TEXT("Content path to the IKRig asset"), true },
			{ TEXT("chain_name"),  TEXT("string"), TEXT("Name of the chain to remove"), true },
		};
		R.Handler = [](const TSharedPtr<FJsonObject>& P) { return IKRigRemoveRetargetChain(P); };
		OutTools.Add(R);
	}
	{
		FMCPToolDef R;
		R.Name = TEXT("ik_rig_assign_mesh");
		R.Description = TEXT(
			"Assign a SkeletalMesh to an IK Rig, populating its internal skeleton. "
			"This must be done before ik_rig_set_retarget_root or ik_rig_add_retarget_chain — "
			"the bone list is empty until a mesh is assigned.");
		R.Params = {
			{ TEXT("ik_rig_path"), TEXT("string"), TEXT("Content path to the IKRig asset"), true },
			{ TEXT("mesh_path"),   TEXT("string"), TEXT("Content path to the SkeletalMesh to assign"), true },
		};
		R.Handler = [](const TSharedPtr<FJsonObject>& P) { return IKRigAssignMesh(P); };
		OutTools.Add(R);
	}
	{
		FMCPToolDef R;
		R.Name = TEXT("anim_retarget_batch");
		R.Description = TEXT(
			"Duplicate and retarget a list of animation assets using an IK Retargeter, "
			"then move all output assets to output_folder. "
			"source_assets is a JSON array of content paths, e.g. [\"/Game/Anims/AS_Idle\",\"/Game/Anims/AS_Walk\"].");
		R.Params = {
			{ TEXT("retargeter_path"),  TEXT("string"), TEXT("Content path to the IKRetargeter asset"), true },
			{ TEXT("source_mesh"),      TEXT("string"), TEXT("Content path to the source SkeletalMesh (animation is FROM this mesh)"), true },
			{ TEXT("target_mesh"),      TEXT("string"), TEXT("Content path to the target SkeletalMesh (animation is TO this mesh)"), true },
			{ TEXT("source_assets"),    TEXT("array"),  TEXT("JSON array of content paths to AnimSequence/BlendSpace assets to retarget"), true },
			{ TEXT("output_folder"),    TEXT("string"), TEXT("Destination content folder for retargeted assets, e.g. /Game/Anims/Retargeted"), true },
			{ TEXT("suffix"),           TEXT("string"), TEXT("Suffix appended to duplicated asset names before move (default: _RTG)"), false },
		};
		R.Handler = [](const TSharedPtr<FJsonObject>& P) { return AnimRetargetBatch(P); };
		OutTools.Add(R);
	}
}

FMCPToolResult FMCPIKRetargetTools::IKRetargetCreate(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath, SourceRigPath, TargetRigPath;
	if (!Params->TryGetStringField(TEXT("path"), AssetPath))
		return FMCPToolResult::Error(TEXT("Missing: path"));
	if (!Params->TryGetStringField(TEXT("source_rig"), SourceRigPath))
		return FMCPToolResult::Error(TEXT("Missing: source_rig"));
	if (!Params->TryGetStringField(TEXT("target_rig"), TargetRigPath))
		return FMCPToolResult::Error(TEXT("Missing: target_rig"));

	// Load rigs
	UIKRigDefinition* SourceRig = LoadObject<UIKRigDefinition>(nullptr, *SourceRigPath);
	if (!SourceRig)
		return FMCPToolResult::Error(FString::Printf(TEXT("Source IKRig not found: %s"), *SourceRigPath));

	UIKRigDefinition* TargetRig = LoadObject<UIKRigDefinition>(nullptr, *TargetRigPath);
	if (!TargetRig)
		return FMCPToolResult::Error(FString::Printf(TEXT("Target IKRig not found: %s"), *TargetRigPath));

	// Create the retargeter asset
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
	UPackage* Package = CreatePackage(*AssetPath);
	Package->FullyLoad();

	UIKRetargeter* Retargeter = NewObject<UIKRetargeter>(
		Package, UIKRetargeter::StaticClass(), *AssetName,
		RF_Public | RF_Standalone | RF_Transactional);

	if (!Retargeter)
		return FMCPToolResult::Error(TEXT("Failed to create UIKRetargeter object"));

	FAssetRegistryModule::AssetCreated(Retargeter);
	Package->MarkPackageDirty();

	// Get the controller (editor-side API for configuring the retargeter)
	UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	if (!Controller)
		return FMCPToolResult::Error(TEXT("Failed to get UIKRetargeterController"));

	// Assign rigs
	Controller->SetIKRig(ERetargetSourceOrTarget::Source, SourceRig);
	Controller->SetIKRig(ERetargetSourceOrTarget::Target, TargetRig);

	// Optional preview meshes
	FString SourceMeshPath, TargetMeshPath;
	if (Params->TryGetStringField(TEXT("source_mesh"), SourceMeshPath))
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *SourceMeshPath);
		if (Mesh) Controller->SetPreviewMesh(ERetargetSourceOrTarget::Source, Mesh);
	}
	if (Params->TryGetStringField(TEXT("target_mesh"), TargetMeshPath))
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *TargetMeshPath);
		if (Mesh) Controller->SetPreviewMesh(ERetargetSourceOrTarget::Target, Mesh);
	}

	// Auto-map chains
	FString AutoMapMode;
	Params->TryGetStringField(TEXT("auto_map"), AutoMapMode);
	EAutoMapChainType MapType = AutoMapMode.Equals(TEXT("exact"), ESearchCase::IgnoreCase)
		? EAutoMapChainType::Exact
		: EAutoMapChainType::Fuzzy;
	Controller->AutoMapChains(MapType, true);

	// Save
	FEditorFileUtils::SaveDirtyPackages(false, true, true, false, false, false);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created IK Retargeter '%s' — source: %s, target: %s, chains auto-mapped"),
		*AssetPath, *SourceRigPath, *TargetRigPath));
}

// ---------------------------------------------------------------------------
// Shared helper: load IKRig + get controller, returning error on any failure
// ---------------------------------------------------------------------------

static bool LoadIKRigAndController(const FString& RigPath,
	UIKRigDefinition*& OutRig, UIKRigController*& OutController, FString& OutError)
{
	OutRig = LoadObject<UIKRigDefinition>(nullptr, *RigPath);
	if (!OutRig)
	{
		OutError = FString::Printf(TEXT("IKRig not found: %s"), *RigPath);
		return false;
	}
	OutController = UIKRigController::GetController(OutRig);
	if (!OutController)
	{
		OutError = FString::Printf(TEXT("Failed to get UIKRigController for: %s"), *RigPath);
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// ik_rig_set_retarget_root
// ---------------------------------------------------------------------------

FMCPToolResult FMCPIKRetargetTools::IKRigSetRetargetRoot(const TSharedPtr<FJsonObject>& Params)
{
	FString RigPath, RootBone;
	if (!Params->TryGetStringField(TEXT("ik_rig_path"), RigPath))
		return FMCPToolResult::Error(TEXT("Missing: ik_rig_path"));
	if (!Params->TryGetStringField(TEXT("root_bone"), RootBone))
		return FMCPToolResult::Error(TEXT("Missing: root_bone"));

	UIKRigDefinition* Rig; UIKRigController* Controller; FString Err;
	if (!LoadIKRigAndController(RigPath, Rig, Controller, Err))
		return FMCPToolResult::Error(Err);

	// Validate bone exists before calling into the controller
	const FIKRigSkeleton& IKSkel = Controller->GetIKRigSkeleton();
	if (IKSkel.GetBoneIndexFromName(FName(*RootBone)) == INDEX_NONE)
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Bone '%s' not found in IKRig skeleton. Call anim_get_skeleton_bones to list valid names."), *RootBone));

	if (!Controller->SetRetargetRoot(FName(*RootBone)))
		return FMCPToolResult::Error(FString::Printf(TEXT("SetRetargetRoot failed for bone '%s'"), *RootBone));

	Rig->GetOutermost()->MarkPackageDirty();
	FEditorFileUtils::SaveDirtyPackages(false, true, true, false, false, false);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Set retarget root to '%s' on '%s'"), *RootBone, *RigPath));
}

// ---------------------------------------------------------------------------
// ik_rig_add_retarget_chain
// ---------------------------------------------------------------------------

FMCPToolResult FMCPIKRetargetTools::IKRigAddRetargetChain(const TSharedPtr<FJsonObject>& Params)
{
	FString RigPath, ChainName, StartBone, EndBone;
	if (!Params->TryGetStringField(TEXT("ik_rig_path"),  RigPath))    return FMCPToolResult::Error(TEXT("Missing: ik_rig_path"));
	if (!Params->TryGetStringField(TEXT("chain_name"),   ChainName))  return FMCPToolResult::Error(TEXT("Missing: chain_name"));
	if (!Params->TryGetStringField(TEXT("start_bone"),   StartBone))  return FMCPToolResult::Error(TEXT("Missing: start_bone"));
	if (!Params->TryGetStringField(TEXT("end_bone"),     EndBone))    return FMCPToolResult::Error(TEXT("Missing: end_bone"));

	UIKRigDefinition* Rig; UIKRigController* Controller; FString Err;
	if (!LoadIKRigAndController(RigPath, Rig, Controller, Err))
		return FMCPToolResult::Error(Err);

	// Validate bones exist — wrong bone names create broken chains silently
	const FIKRigSkeleton& IKSkel = Controller->GetIKRigSkeleton();
	if (IKSkel.GetBoneIndexFromName(FName(*StartBone)) == INDEX_NONE)
		return FMCPToolResult::Error(FString::Printf(TEXT("start_bone '%s' not found in IKRig skeleton"), *StartBone));
	if (IKSkel.GetBoneIndexFromName(FName(*EndBone)) == INDEX_NONE)
		return FMCPToolResult::Error(FString::Printf(TEXT("end_bone '%s' not found in IKRig skeleton"), *EndBone));

	// Prevent duplicates — AddRetargetChain silently mangles the name if it clashes
	for (const FBoneChain& Chain : Controller->GetRetargetChains())
	{
		if (Chain.ChainName == FName(*ChainName))
			return FMCPToolResult::Error(FString::Printf(TEXT("Chain '%s' already exists on this IKRig"), *ChainName));
	}

	FString IKGoal;
	Params->TryGetStringField(TEXT("ik_goal"), IKGoal);
	FName GoalName = IKGoal.IsEmpty() ? NAME_None : FName(*IKGoal);

	FName Result = Controller->AddRetargetChain(FName(*ChainName), FName(*StartBone), FName(*EndBone), GoalName);
	if (Result == NAME_None)
		return FMCPToolResult::Error(FString::Printf(TEXT("AddRetargetChain failed for chain '%s'"), *ChainName));

	Rig->GetOutermost()->MarkPackageDirty();
	FEditorFileUtils::SaveDirtyPackages(false, true, true, false, false, false);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added retarget chain '%s' (%s → %s) to '%s'"), *Result.ToString(), *StartBone, *EndBone, *RigPath));
}

// ---------------------------------------------------------------------------
// ik_rig_remove_retarget_chain
// ---------------------------------------------------------------------------

FMCPToolResult FMCPIKRetargetTools::IKRigRemoveRetargetChain(const TSharedPtr<FJsonObject>& Params)
{
	FString RigPath, ChainName;
	if (!Params->TryGetStringField(TEXT("ik_rig_path"), RigPath))   return FMCPToolResult::Error(TEXT("Missing: ik_rig_path"));
	if (!Params->TryGetStringField(TEXT("chain_name"),  ChainName)) return FMCPToolResult::Error(TEXT("Missing: chain_name"));

	UIKRigDefinition* Rig; UIKRigController* Controller; FString Err;
	if (!LoadIKRigAndController(RigPath, Rig, Controller, Err))
		return FMCPToolResult::Error(Err);

	// Verify chain exists before removing — RemoveRetargetChain returns false silently
	bool bFound = false;
	for (const FBoneChain& Chain : Controller->GetRetargetChains())
	{
		if (Chain.ChainName == FName(*ChainName)) { bFound = true; break; }
	}
	if (!bFound)
		return FMCPToolResult::Error(FString::Printf(TEXT("Chain '%s' not found on IKRig '%s'"), *ChainName, *RigPath));

	if (!Controller->RemoveRetargetChain(FName(*ChainName)))
		return FMCPToolResult::Error(FString::Printf(TEXT("RemoveRetargetChain failed for '%s'"), *ChainName));

	Rig->GetOutermost()->MarkPackageDirty();
	FEditorFileUtils::SaveDirtyPackages(false, true, true, false, false, false);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Removed retarget chain '%s' from '%s'"), *ChainName, *RigPath));
}

// ---------------------------------------------------------------------------
// ik_rig_assign_mesh
// ---------------------------------------------------------------------------

FMCPToolResult FMCPIKRetargetTools::IKRigAssignMesh(const TSharedPtr<FJsonObject>& Params)
{
	FString RigPath, MeshPath;
	if (!Params->TryGetStringField(TEXT("ik_rig_path"), RigPath))  return FMCPToolResult::Error(TEXT("Missing: ik_rig_path"));
	if (!Params->TryGetStringField(TEXT("mesh_path"),   MeshPath)) return FMCPToolResult::Error(TEXT("Missing: mesh_path"));

	UIKRigDefinition* Rig; UIKRigController* Controller; FString Err;
	if (!LoadIKRigAndController(RigPath, Rig, Controller, Err))
		return FMCPToolResult::Error(Err);

	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!Mesh)
		return FMCPToolResult::Error(FString::Printf(TEXT("SkeletalMesh not found: %s"), *MeshPath));

	if (!Controller->SetSkeletalMesh(Mesh))
		return FMCPToolResult::Error(FString::Printf(
			TEXT("SetSkeletalMesh failed — mesh may be incompatible with this IKRig: %s"), *MeshPath));

	Rig->GetOutermost()->MarkPackageDirty();
	FEditorFileUtils::SaveDirtyPackages(false, true, true, false, false, false);

	const FIKRigSkeleton& Skel = Controller->GetIKRigSkeleton();
	return FMCPToolResult::Success(FString::Printf(
		TEXT("Assigned mesh '%s' to IKRig '%s' — %d bones now available"),
		*MeshPath, *RigPath, Skel.BoneNames.Num()));
}

// ---------------------------------------------------------------------------
// anim_retarget_batch
// ---------------------------------------------------------------------------

FMCPToolResult FMCPIKRetargetTools::AnimRetargetBatch(const TSharedPtr<FJsonObject>& Params)
{
	FString RetargeterPath, SourceMeshPath, TargetMeshPath, OutputFolder, Suffix;
	if (!Params->TryGetStringField(TEXT("retargeter_path"), RetargeterPath))  return FMCPToolResult::Error(TEXT("Missing: retargeter_path"));
	if (!Params->TryGetStringField(TEXT("source_mesh"),     SourceMeshPath))  return FMCPToolResult::Error(TEXT("Missing: source_mesh"));
	if (!Params->TryGetStringField(TEXT("target_mesh"),     TargetMeshPath))  return FMCPToolResult::Error(TEXT("Missing: target_mesh"));
	if (!Params->TryGetStringField(TEXT("output_folder"),   OutputFolder))    return FMCPToolResult::Error(TEXT("Missing: output_folder"));

	const TArray<TSharedPtr<FJsonValue>>* AssetsJson = nullptr;
	if (!Params->TryGetArrayField(TEXT("source_assets"), AssetsJson) || !AssetsJson || AssetsJson->IsEmpty())
		return FMCPToolResult::Error(TEXT("source_assets must be a non-empty JSON array of content paths"));

	Params->TryGetStringField(TEXT("suffix"), Suffix);
	if (Suffix.IsEmpty()) Suffix = TEXT("_RTG");

	// Load required assets
	UIKRetargeter* Retargeter = LoadObject<UIKRetargeter>(nullptr, *RetargeterPath);
	if (!Retargeter)
		return FMCPToolResult::Error(FString::Printf(TEXT("IKRetargeter not found: %s"), *RetargeterPath));

	USkeletalMesh* SrcMesh = LoadObject<USkeletalMesh>(nullptr, *SourceMeshPath);
	if (!SrcMesh)
		return FMCPToolResult::Error(FString::Printf(TEXT("Source SkeletalMesh not found: %s"), *SourceMeshPath));

	USkeletalMesh* TgtMesh = LoadObject<USkeletalMesh>(nullptr, *TargetMeshPath);
	if (!TgtMesh)
		return FMCPToolResult::Error(FString::Printf(TEXT("Target SkeletalMesh not found: %s"), *TargetMeshPath));

	if (SrcMesh == TgtMesh)
		return FMCPToolResult::Error(TEXT("source_mesh and target_mesh must be different assets"));

	// Resolve source asset paths to FAssetData via registry (avoids loading every asset upfront)
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> AssetsToRetarget;
	TArray<FString> NotFound;
	for (const TSharedPtr<FJsonValue>& Val : *AssetsJson)
	{
		FString Path = Val->AsString();
		FAssetData AD = AR.GetAssetByObjectPath(FSoftObjectPath(*Path));
		if (AD.IsValid())
			AssetsToRetarget.Add(AD);
		else
			NotFound.Add(Path);
	}
	if (!NotFound.IsEmpty())
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Asset(s) not found in registry: %s"), *FString::Join(NotFound, TEXT(", "))));

	// Run the batch retarget — creates duplicates alongside source assets with the given suffix
	TArray<FAssetData> NewAssets = UIKRetargetBatchOperation::DuplicateAndRetarget(
		AssetsToRetarget, SrcMesh, TgtMesh, Retargeter,
		TEXT(""), TEXT(""), TEXT(""), Suffix,
		/*bIncludeReferencedAssets=*/true);

	if (NewAssets.IsEmpty())
		return FMCPToolResult::Error(TEXT("DuplicateAndRetarget returned no assets — check retargeter compatibility"));

	// Normalize output folder (strip trailing slash, ensure /Game/ prefix)
	OutputFolder = OutputFolder.TrimEnd().TrimChar(TEXT('/'));
	if (!OutputFolder.StartsWith(TEXT("/Game")))
		return FMCPToolResult::Error(FString::Printf(TEXT("output_folder must start with /Game, got: %s"), *OutputFolder));

	// Move all new assets to output_folder, stripping the temp suffix from the name
	IAssetTools& AT = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TArray<FAssetRenameData> RenameList;
	for (const FAssetData& AD : NewAssets)
	{
		UObject* Obj = AD.GetAsset();
		if (!Obj) continue;

		// Strip suffix to get the clean target name
		FString FinalName = AD.AssetName.ToString();
		if (FinalName.EndsWith(Suffix)) FinalName = FinalName.LeftChop(Suffix.Len());

		RenameList.Emplace(Obj, OutputFolder, FinalName);
	}

	if (!RenameList.IsEmpty())
		AT.RenameAssets(RenameList);

	// Fix up redirectors left behind in source locations
	TArray<FAssetData> Redirectors;
	FARFilter RedirFilter;
	RedirFilter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("ObjectRedirector")));
	RedirFilter.bRecursivePaths = true;
	RedirFilter.PackagePaths.Add(FName(*OutputFolder));
	AR.GetAssets(RedirFilter, Redirectors);
	if (!Redirectors.IsEmpty())
	{
		TArray<UObjectRedirector*> RedirObjs;
		for (const FAssetData& RD : Redirectors)
			if (UObjectRedirector* R = Cast<UObjectRedirector>(RD.GetAsset()))
				RedirObjs.Add(R);
		if (!RedirObjs.IsEmpty())
			AT.FixupReferencers(RedirObjs);
	}

	FEditorFileUtils::SaveDirtyPackages(false, true, true, false, false, false);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Retargeted %d asset(s) → '%s' (suffix '%s' stripped from names)"),
		RenameList.Num(), *OutputFolder, *Suffix));
}
