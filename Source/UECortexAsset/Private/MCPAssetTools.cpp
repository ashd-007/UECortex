#include "MCPAssetTools.h"
#include "MCPToolRegistry.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetData.h"
#include "ObjectTools.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "FileHelpers.h"

// ---------------------------------------------------------------------------
// NOTE: HTTP handlers are called from the game thread tick (FHttpServerModule::ProcessRequests).
// Do NOT use AsyncTask(GameThread) + FEvent — deadlocks. Call UE APIs directly.
// ---------------------------------------------------------------------------

static IAssetRegistry& AR()
{
	return FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
}

static IAssetTools& AT()
{
	return FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
}

void FMCPAssetTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	{
		FMCPToolDef T;
		T.Name = TEXT("asset_list");
		T.Description = TEXT("Search for assets by class, path, and/or name filter. class_name examples: StaticMesh, Blueprint, Material, Texture2D, SoundWave.");
		T.Params = {
			{ TEXT("class_name"),   TEXT("string"), TEXT("UClass name to filter by (e.g. StaticMesh). Omit for all classes."), false },
			{ TEXT("path"),         TEXT("string"), TEXT("Content path prefix to search under (default: /Game)"), false },
			{ TEXT("filter"),       TEXT("string"), TEXT("Substring filter on asset name"), false },
			{ TEXT("limit"),        TEXT("number"), TEXT("Max results to return (default 50)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return AssetList(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("asset_get_info");
		T.Description = TEXT("Get detailed information about a specific asset.");
		T.Params = { { TEXT("asset_path"), TEXT("string"), TEXT("Full content path to the asset"), true } };
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return AssetGetInfo(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("asset_get_references");
		T.Description = TEXT("Find all assets that reference (depend on) a given asset.");
		T.Params = {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Full content path to the asset"), true },
			{ TEXT("recursive"),  TEXT("boolean"), TEXT("Search recursively (default true)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return AssetGetReferences(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("asset_get_dependencies");
		T.Description = TEXT("Find all assets that a given asset depends on.");
		T.Params = {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Full content path to the asset"), true },
			{ TEXT("recursive"),  TEXT("boolean"), TEXT("Search recursively (default false)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return AssetGetDependencies(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("asset_move");
		T.Description = TEXT("Move or rename an asset to a new content path.");
		T.Params = {
			{ TEXT("source_path"), TEXT("string"), TEXT("Current full content path of the asset"), true },
			{ TEXT("dest_path"),   TEXT("string"), TEXT("New full content path for the asset"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return AssetMove(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("asset_copy");
		T.Description = TEXT("Duplicate an asset to a new content path.");
		T.Params = {
			{ TEXT("source_path"), TEXT("string"), TEXT("Full content path of the asset to copy"), true },
			{ TEXT("dest_path"),   TEXT("string"), TEXT("Full content path for the new copy"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return AssetCopy(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("asset_delete");
		T.Description = TEXT("Delete an asset. This cannot be undone.");
		T.Params = { { TEXT("asset_path"), TEXT("string"), TEXT("Full content path of the asset to delete"), true } };
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return AssetDelete(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("asset_create_folder");
		T.Description = TEXT("Create a new folder in the content browser.");
		T.Params = { { TEXT("path"), TEXT("string"), TEXT("Content path for the new folder, e.g. /Game/MyFolder"), true } };
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return AssetCreateFolder(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("asset_list_redirectors");
		T.Description = TEXT("List ObjectRedirector assets (stale references) under a content path.");
		T.Params = {
			{ TEXT("path"),      TEXT("string"), TEXT("Content path to search under (default: /Game)"), false },
			{ TEXT("recursive"), TEXT("boolean"), TEXT("Search recursively (default true)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return AssetListRedirectors(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("asset_fix_redirectors");
		T.Description = TEXT("Fix up ObjectRedirectors under a content path, updating all references to point to the final asset.");
		T.Params = {
			{ TEXT("path"),      TEXT("string"), TEXT("Content path to fix redirectors under (default: /Game)"), false },
			{ TEXT("recursive"), TEXT("boolean"), TEXT("Fix recursively (default true)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return AssetFixRedirectors(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("asset_migrate");
		T.Description = TEXT("Copy an asset and all its transitive /Game/ dependencies to a destination folder, preserving path structure. Equivalent to Content Browser → Migrate.");
		T.Params = {
			{ TEXT("source_path"), TEXT("string"), TEXT("Content path of the asset to migrate, e.g. /Game/Characters/SK_Hero"), true },
			{ TEXT("dest_folder"), TEXT("string"), TEXT("Destination folder, e.g. /Game/Imported. Path structure under /Game/ is preserved."), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return AssetMigrate(P); };
		OutTools.Add(T);
	}
}

// ---------------------------------------------------------------------------
// asset_list
// ---------------------------------------------------------------------------

FMCPToolResult FMCPAssetTools::AssetList(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName, Path, Filter;
	double Limit = 50;
	Params->TryGetStringField(TEXT("class_name"), ClassName);
	Params->TryGetStringField(TEXT("path"), Path);
	Params->TryGetStringField(TEXT("filter"), Filter);
	Params->TryGetNumberField(TEXT("limit"), Limit);
	if (Path.IsEmpty()) Path = TEXT("/Game");

	FARFilter ARFilter;
	ARFilter.bRecursivePaths = true;
	ARFilter.PackagePaths.Add(FName(*Path));
	if (!ClassName.IsEmpty())
	{
		// Try to resolve the class — fall back to a string-only match if not found
		if (UClass* Class = FindObject<UClass>(nullptr, *(TEXT("/Script/Engine.") + ClassName)))
			ARFilter.ClassPaths.Add(Class->GetClassPathName());
		else if (UClass* Class2 = FindObject<UClass>(nullptr, *(TEXT("/Script/UMGEditor.") + ClassName)))
			ARFilter.ClassPaths.Add(Class2->GetClassPathName());
		else
			ARFilter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), FName(*ClassName)));
	}

	TArray<FAssetData> Assets;
	AR().GetAssets(ARFilter, Assets);

	TArray<TSharedPtr<FJsonValue>> Arr;
	int32 LimitInt = (int32)Limit;
	for (const FAssetData& A : Assets)
	{
		if (Arr.Num() >= LimitInt) break;
		FString Name = A.AssetName.ToString();
		if (!Filter.IsEmpty() && !Name.Contains(Filter, ESearchCase::IgnoreCase)) continue;

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"),       Name);
		Obj->SetStringField(TEXT("class"),      A.AssetClassPath.GetAssetName().ToString());
		Obj->SetStringField(TEXT("path"),       A.GetSoftObjectPath().ToString());
		Obj->SetStringField(TEXT("package"),    A.PackageName.ToString());
		Arr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetArrayField(TEXT("assets"), Arr);
	Response->SetNumberField(TEXT("count"), Arr.Num());
	Response->SetBoolField(TEXT("truncated"), Arr.Num() >= LimitInt && Assets.Num() > LimitInt);
	return FMCPToolResult::Success(FString::Printf(TEXT("Found %d assets"), Arr.Num()), Response);
}

// ---------------------------------------------------------------------------
// asset_get_info
// ---------------------------------------------------------------------------

FMCPToolResult FMCPAssetTools::AssetGetInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FMCPToolResult::Error(TEXT("Missing required param: asset_path"));

	// Strip object sub-path if provided (e.g. /Game/Foo.Foo → /Game/Foo)
	FString PackagePath = AssetPath;
	int32 DotIdx;
	if (AssetPath.FindChar(TEXT('.'), DotIdx))
		PackagePath = AssetPath.Left(DotIdx);

	TArray<FAssetData> Assets;
	FARFilter ARFilter;
	ARFilter.PackageNames.Add(FName(*PackagePath));
	AR().GetAssets(ARFilter, Assets);

	if (Assets.IsEmpty())
		return FMCPToolResult::Error(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));

	const FAssetData& A = Assets[0];

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("name"),    A.AssetName.ToString());
	Response->SetStringField(TEXT("class"),   A.AssetClassPath.GetAssetName().ToString());
	Response->SetStringField(TEXT("path"),    A.GetSoftObjectPath().ToString());
	Response->SetStringField(TEXT("package"), A.PackageName.ToString());

	// File size on disk
	FString FilePath;
	if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, FilePath, FPackageName::GetAssetPackageExtension()))
	{
		int64 FileSize = IFileManager::Get().FileSize(*FilePath);
		if (FileSize >= 0)
			Response->SetNumberField(TEXT("file_size_bytes"), (double)FileSize);
	}

	// Tags/metadata
	TSharedPtr<FJsonObject> Tags = MakeShared<FJsonObject>();
	TArray<UObject::FAssetRegistryTag> AssetTags;
	// Load the asset to get its tags
	if (UObject* Obj = A.GetAsset())
	{
		Obj->GetAssetRegistryTags(AssetTags);
		for (const UObject::FAssetRegistryTag& Tag : AssetTags)
			Tags->SetStringField(Tag.Name.ToString(), Tag.Value);
	}
	Response->SetObjectField(TEXT("tags"), Tags);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Asset '%s' (%s)"), *A.AssetName.ToString(), *A.AssetClassPath.GetAssetName().ToString()),
		Response);
}

// ---------------------------------------------------------------------------
// asset_get_references
// ---------------------------------------------------------------------------

FMCPToolResult FMCPAssetTools::AssetGetReferences(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	bool bRecursive = true;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FMCPToolResult::Error(TEXT("Missing required param: asset_path"));
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);

	FString PackagePath = AssetPath;
	int32 DotIdx;
	if (AssetPath.FindChar(TEXT('.'), DotIdx))
		PackagePath = AssetPath.Left(DotIdx);

	TArray<FName> Referencers;
	AR().GetReferencers(FName(*PackagePath), Referencers,
		bRecursive ? UE::AssetRegistry::EDependencyCategory::All : UE::AssetRegistry::EDependencyCategory::Package);

	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FName& Ref : Referencers)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("package"), Ref.ToString());
		Arr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetArrayField(TEXT("referencers"), Arr);
	Response->SetNumberField(TEXT("count"), Arr.Num());
	return FMCPToolResult::Success(FString::Printf(TEXT("Found %d referencers for '%s'"), Arr.Num(), *PackagePath), Response);
}

// ---------------------------------------------------------------------------
// asset_get_dependencies
// ---------------------------------------------------------------------------

FMCPToolResult FMCPAssetTools::AssetGetDependencies(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	bool bRecursive = false;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FMCPToolResult::Error(TEXT("Missing required param: asset_path"));
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);

	FString PackagePath = AssetPath;
	int32 DotIdx;
	if (AssetPath.FindChar(TEXT('.'), DotIdx))
		PackagePath = AssetPath.Left(DotIdx);

	TArray<FName> Deps;
	AR().GetDependencies(FName(*PackagePath), Deps,
		UE::AssetRegistry::EDependencyCategory::Package);

	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FName& Dep : Deps)
	{
		// Skip engine/script packages
		FString DepStr = Dep.ToString();
		if (DepStr.StartsWith(TEXT("/Script/")) || DepStr.StartsWith(TEXT("/Engine/"))) continue;

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("package"), DepStr);
		Arr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetArrayField(TEXT("dependencies"), Arr);
	Response->SetNumberField(TEXT("count"), Arr.Num());
	return FMCPToolResult::Success(FString::Printf(TEXT("Found %d dependencies for '%s'"), Arr.Num(), *PackagePath), Response);
}

// ---------------------------------------------------------------------------
// asset_move
// ---------------------------------------------------------------------------

FMCPToolResult FMCPAssetTools::AssetMove(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath, DestPath;
	if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
		return FMCPToolResult::Error(TEXT("Missing required param: source_path"));
	if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
		return FMCPToolResult::Error(TEXT("Missing required param: dest_path"));

	// Strip dot-suffix from paths if present
	auto StripDot = [](FString& P) {
		int32 Dot;
		if (P.FindChar(TEXT('.'), Dot)) P = P.Left(Dot);
	};
	StripDot(SourcePath);
	StripDot(DestPath);

	int32 DestSlash;
	if (!DestPath.FindLastChar(TEXT('/'), DestSlash))
		return FMCPToolResult::Error(TEXT("Invalid dest_path"));
	FString DestName    = DestPath.Mid(DestSlash + 1);
	FString DestPackage = DestPath.Left(DestSlash);

	TArray<FAssetData> Assets;
	FARFilter ARFilter;
	ARFilter.PackageNames.Add(FName(*SourcePath));
	AR().GetAssets(ARFilter, Assets);
	if (Assets.IsEmpty())
		return FMCPToolResult::Error(FString::Printf(TEXT("Asset not found: %s"), *SourcePath));

	UObject* Obj = Assets[0].GetAsset();
	if (!Obj)
		return FMCPToolResult::Error(TEXT("Failed to load asset for move"));

	TArray<FAssetRenameData> RenameData;
	RenameData.Emplace(Obj, DestPackage, DestName);
	bool bOk = AT().RenameAssets(RenameData);
	if (!bOk)
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to move '%s' to '%s'"), *SourcePath, *DestPath));

	return FMCPToolResult::Success(FString::Printf(TEXT("Moved '%s' → '%s'"), *SourcePath, *DestPath));
}

// ---------------------------------------------------------------------------
// asset_copy
// ---------------------------------------------------------------------------

FMCPToolResult FMCPAssetTools::AssetCopy(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath, DestPath;
	if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
		return FMCPToolResult::Error(TEXT("Missing required param: source_path"));
	if (!Params->TryGetStringField(TEXT("dest_path"), DestPath))
		return FMCPToolResult::Error(TEXT("Missing required param: dest_path"));

	auto StripDot = [](FString& P) {
		int32 Dot;
		if (P.FindChar(TEXT('.'), Dot)) P = P.Left(Dot);
	};
	StripDot(SourcePath);
	StripDot(DestPath);

	int32 DestSlash;
	if (!DestPath.FindLastChar(TEXT('/'), DestSlash))
		return FMCPToolResult::Error(TEXT("Invalid dest_path"));
	FString DestName    = DestPath.Mid(DestSlash + 1);
	FString DestPackage = DestPath.Left(DestSlash);

	TArray<FAssetData> Assets;
	FARFilter ARFilter;
	ARFilter.PackageNames.Add(FName(*SourcePath));
	AR().GetAssets(ARFilter, Assets);
	if (Assets.IsEmpty())
		return FMCPToolResult::Error(FString::Printf(TEXT("Asset not found: %s"), *SourcePath));

	UObject* Obj = Assets[0].GetAsset();
	if (!Obj)
		return FMCPToolResult::Error(TEXT("Failed to load asset for copy"));

	UObject* NewAsset = AT().DuplicateAsset(DestName, DestPackage, Obj);
	if (!NewAsset)
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to copy '%s' to '%s'"), *SourcePath, *DestPath));

	return FMCPToolResult::Success(FString::Printf(TEXT("Copied '%s' → '%s'"), *SourcePath, *DestPath));
}

// ---------------------------------------------------------------------------
// asset_delete
// ---------------------------------------------------------------------------

FMCPToolResult FMCPAssetTools::AssetDelete(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FMCPToolResult::Error(TEXT("Missing required param: asset_path"));

	FString PackagePath = AssetPath;
	int32 DotIdx;
	if (AssetPath.FindChar(TEXT('.'), DotIdx))
		PackagePath = AssetPath.Left(DotIdx);

	TArray<FAssetData> Assets;
	FARFilter ARFilter;
	ARFilter.PackageNames.Add(FName(*PackagePath));
	AR().GetAssets(ARFilter, Assets);
	if (Assets.IsEmpty())
		return FMCPToolResult::Error(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));

	int32 Deleted = ObjectTools::DeleteAssets(Assets, /*bShowConfirmation=*/false);
	if (Deleted == 0)
		return FMCPToolResult::Error(FString::Printf(TEXT("Could not delete '%s' — it may be referenced by other assets"), *AssetPath));

	return FMCPToolResult::Success(FString::Printf(TEXT("Deleted '%s'"), *AssetPath));
}

// ---------------------------------------------------------------------------
// asset_create_folder
// ---------------------------------------------------------------------------

FMCPToolResult FMCPAssetTools::AssetCreateFolder(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	if (!Params->TryGetStringField(TEXT("path"), Path))
		return FMCPToolResult::Error(TEXT("Missing required param: path"));

	if (!Path.StartsWith(TEXT("/")))
		return FMCPToolResult::Error(TEXT("Path must be an absolute content path (e.g. /Game/MyFolder)"));

	FString FilesystemPath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(Path + TEXT("/"), FilesystemPath))
		return FMCPToolResult::Error(FString::Printf(TEXT("Invalid content path: %s"), *Path));

	if (IFileManager::Get().MakeDirectory(*FilesystemPath, /*Tree=*/true))
		return FMCPToolResult::Success(FString::Printf(TEXT("Created folder '%s'"), *Path));

	// Directory may already exist
	if (IFileManager::Get().DirectoryExists(*FilesystemPath))
		return FMCPToolResult::Success(FString::Printf(TEXT("Folder already exists: '%s'"), *Path));

	return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create folder '%s'"), *Path));
}

// ---------------------------------------------------------------------------
// asset_list_redirectors
// ---------------------------------------------------------------------------

FMCPToolResult FMCPAssetTools::AssetListRedirectors(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	bool bRecursive = true;
	Params->TryGetStringField(TEXT("path"), Path);
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);
	if (Path.IsEmpty()) Path = TEXT("/Game");

	FARFilter ARFilter;
	ARFilter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("ObjectRedirector")));
	ARFilter.PackagePaths.Add(FName(*Path));
	ARFilter.bRecursivePaths = bRecursive;

	TArray<FAssetData> Assets;
	AR().GetAssets(ARFilter, Assets);

	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FAssetData& A : Assets)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"),    A.AssetName.ToString());
		Obj->SetStringField(TEXT("package"), A.PackageName.ToString());
		Arr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetArrayField(TEXT("redirectors"), Arr);
	Response->SetNumberField(TEXT("count"), Arr.Num());
	return FMCPToolResult::Success(FString::Printf(TEXT("Found %d redirectors under '%s'"), Arr.Num(), *Path), Response);
}

// ---------------------------------------------------------------------------
// asset_fix_redirectors
// ---------------------------------------------------------------------------

FMCPToolResult FMCPAssetTools::AssetFixRedirectors(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	bool bRecursive = true;
	Params->TryGetStringField(TEXT("path"), Path);
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);
	if (Path.IsEmpty()) Path = TEXT("/Game");

	FARFilter ARFilter;
	ARFilter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("ObjectRedirector")));
	ARFilter.PackagePaths.Add(FName(*Path));
	ARFilter.bRecursivePaths = bRecursive;

	TArray<FAssetData> RedirectorAssets;
	AR().GetAssets(ARFilter, RedirectorAssets);

	if (RedirectorAssets.IsEmpty())
		return FMCPToolResult::Success(FString::Printf(TEXT("No redirectors found under '%s'"), *Path));

	TArray<UObjectRedirector*> Redirectors;
	for (const FAssetData& A : RedirectorAssets)
	{
		if (UObjectRedirector* R = Cast<UObjectRedirector>(A.GetAsset()))
			Redirectors.Add(R);
	}

	AT().FixupReferencers(Redirectors);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Fixed %d redirectors under '%s'"), Redirectors.Num(), *Path));
}

// ---------------------------------------------------------------------------
// asset_migrate
// ---------------------------------------------------------------------------

static void CollectMigrateDeps(const FName& PackageName, TSet<FName>& OutPackages, IAssetRegistry& AssetRegistry)
{
	if (OutPackages.Contains(PackageName)) return;
	if (!PackageName.ToString().StartsWith(TEXT("/Game/"))) return;
	OutPackages.Add(PackageName);

	TArray<FName> Deps;
	AssetRegistry.GetDependencies(PackageName, Deps, UE::AssetRegistry::EDependencyCategory::Package);
	for (const FName& Dep : Deps)
		CollectMigrateDeps(Dep, OutPackages, AssetRegistry);
}

FMCPToolResult FMCPAssetTools::AssetMigrate(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath, DestFolder;
	if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
		return FMCPToolResult::Error(TEXT("Missing: source_path"));
	if (!Params->TryGetStringField(TEXT("dest_folder"), DestFolder))
		return FMCPToolResult::Error(TEXT("Missing: dest_folder"));

	// Normalize: strip trailing slash, strip asset name if present (e.g. /Game/X.X → /Game/X)
	DestFolder = DestFolder.TrimEnd().TrimChar('/');
	FString SrcPackageStr = SourcePath;
	int32 DotIdx;
	if (SrcPackageStr.FindLastChar('.', DotIdx))
		SrcPackageStr = SrcPackageStr.Left(DotIdx);

	// Recursively collect all /Game/ dependencies
	TSet<FName> AllPackages;
	CollectMigrateDeps(FName(*SrcPackageStr), AllPackages, AR());

	if (AllPackages.IsEmpty())
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Source asset not found or is not a /Game/ asset: %s"), *SourcePath));

	int32 Copied = 0;
	TArray<FString> Skipped;

	for (const FName& PkgName : AllPackages)
	{
		FString PkgStr    = PkgName.ToString();                              // /Game/X/Y
		FString AssetName = FPackageName::GetLongPackageAssetName(PkgStr);  // Y
		FString RelPath   = PkgStr.Mid(6);                                  // strip "/Game/"
		FString DestPkg   = DestFolder + TEXT("/") + RelPath;               // /Game/Imported/X/Y

		// Skip if already exists at destination
		FARFilter ExistFilter;
		ExistFilter.PackageNames.Add(FName(*DestPkg));
		TArray<FAssetData> Existing;
		AR().GetAssets(ExistFilter, Existing);
		if (Existing.Num() > 0)
		{
			Skipped.Add(AssetName + TEXT(" (exists)"));
			continue;
		}

		// Load source asset
		UObject* Asset = LoadObject<UObject>(nullptr, *(PkgStr + TEXT(".") + AssetName));
		if (!Asset)
		{
			Skipped.Add(AssetName + TEXT(" (load failed)"));
			continue;
		}

		// Duplicate to destination
		FString DestPkgFolder = FPackageName::GetLongPackagePath(DestPkg);
		UObject* Dup = AT().DuplicateAsset(AssetName, DestPkgFolder, Asset);
		if (Dup)
		{
			Dup->GetOutermost()->MarkPackageDirty();
			Copied++;
		}
		else
		{
			Skipped.Add(AssetName + TEXT(" (duplicate failed)"));
		}
	}

	FEditorFileUtils::SaveDirtyPackages(false, true, true, false, false, false);

	FString SkippedStr;
	if (Skipped.Num() > 0)
		SkippedStr = FString::Printf(TEXT("\nSkipped (%d): %s"), Skipped.Num(), *FString::Join(Skipped, TEXT(", ")));

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Migrated %d / %d assets to '%s'%s"),
		Copied, AllPackages.Num(), *DestFolder, *SkippedStr));
}
