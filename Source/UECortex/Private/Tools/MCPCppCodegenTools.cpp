#include "Tools/MCPCppCodegenTools.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "UObject/UnrealType.h"
#include "Misc/HotReloadInterface.h"

// ---------------------------------------------------------------------------
// File helpers
// ---------------------------------------------------------------------------

FString FMCPCppCodegenTools::GetSourceDir(const FString& SubDir)
{
	FString ProjectName = FApp::GetProjectName();
	FString Base = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), ProjectName);
	return SubDir.IsEmpty() ? Base : FPaths::Combine(Base, SubDir);
}

FString FMCPCppCodegenTools::GetAPImacro()
{
	return FString(FApp::GetProjectName()).ToUpper() + TEXT("_API");
}

bool FMCPCppCodegenTools::WriteFile(const FString& Path, const FString& Content, bool bForce, FString& OutError)
{
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

	if (PF.FileExists(*Path) && !bForce)
	{
		OutError = FString::Printf(TEXT("File already exists: %s  (pass force=true to overwrite)"), *Path);
		return false;
	}

	// Ensure directory exists
	FString Dir = FPaths::GetPath(Path);
	if (!PF.DirectoryExists(*Dir))
		PF.CreateDirectoryTree(*Dir);

	if (!FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Failed to write file: %s"), *Path);
		return false;
	}
	return true;
}

bool FMCPCppCodegenTools::ReadFile(const FString& Path, FString& OutContent, FString& OutError)
{
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*Path))
	{
		OutError = FString::Printf(TEXT("File not found: %s"), *Path);
		return false;
	}
	if (!FFileHelper::LoadFileToString(OutContent, *Path))
	{
		OutError = FString::Printf(TEXT("Failed to read file: %s"), *Path);
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMCPCppCodegenTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	// cpp_create_actor
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_create_actor");
		Def.Description = TEXT("Scaffold a UCLASS Actor .h and .cpp with constructor, BeginPlay, and Tick.");
		Def.Params = {
			{ TEXT("class_name"), TEXT("string"), TEXT("Class name without prefix, e.g. 'HaloCharacter' → AHaloCharacter") },
			{ TEXT("parent"),     TEXT("string"), TEXT("Parent class, e.g. 'AActor', 'ACharacter'  (default: AActor)"), false },
			{ TEXT("subdir"),     TEXT("string"), TEXT("Subdirectory under Source/ProjectName/, e.g. 'Characters'"), false },
			{ TEXT("force"),      TEXT("boolean"),TEXT("Overwrite if file exists"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppCreateActor(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// cpp_create_component
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_create_component");
		Def.Description = TEXT("Scaffold a UActorComponent subclass .h and .cpp.");
		Def.Params = {
			{ TEXT("class_name"), TEXT("string"), TEXT("Class name without prefix, e.g. 'HaloComponent' → UHaloComponent") },
			{ TEXT("parent"),     TEXT("string"), TEXT("Parent class (default: UActorComponent)"), false },
			{ TEXT("subdir"),     TEXT("string"), TEXT("Subdirectory under Source/ProjectName/"), false },
			{ TEXT("force"),      TEXT("boolean"),TEXT("Overwrite if file exists"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppCreateComponent(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// cpp_create_interface
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_create_interface");
		Def.Description = TEXT("Scaffold a UInterface + IInterface pair .h file.");
		Def.Params = {
			{ TEXT("class_name"), TEXT("string"), TEXT("Interface name without prefix, e.g. 'Damageable'") },
			{ TEXT("subdir"),     TEXT("string"), TEXT("Subdirectory under Source/ProjectName/"), false },
			{ TEXT("force"),      TEXT("boolean"),TEXT("Overwrite if file exists"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppCreateInterface(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// cpp_create_gas_ability
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_create_gas_ability");
		Def.Description = TEXT("Scaffold a UGameplayAbility subclass with ActivateAbility and EndAbility stubs.");
		Def.Params = {
			{ TEXT("class_name"), TEXT("string"), TEXT("Class name without prefix, e.g. 'HaloStrike' → UGA_HaloStrike") },
			{ TEXT("subdir"),     TEXT("string"), TEXT("Subdirectory (default: 'GAS/Abilities')"), false },
			{ TEXT("force"),      TEXT("boolean"),TEXT("Overwrite if file exists"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppCreateGASAbility(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// cpp_create_gas_effect
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_create_gas_effect");
		Def.Description = TEXT("Scaffold a UGameplayEffect subclass.");
		Def.Params = {
			{ TEXT("class_name"), TEXT("string"), TEXT("Class name without prefix, e.g. 'RadiationDamage' → UGE_RadiationDamage") },
			{ TEXT("subdir"),     TEXT("string"), TEXT("Subdirectory (default: 'GAS/Effects')"), false },
			{ TEXT("force"),      TEXT("boolean"),TEXT("Overwrite if file exists"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppCreateGASEffect(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// cpp_create_attribute_set
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_create_attribute_set");
		Def.Description = TEXT("Scaffold a UAttributeSet with GAMEPLAYATTRIBUTE_REPNOTIFY pattern and example attribute.");
		Def.Params = {
			{ TEXT("class_name"),  TEXT("string"), TEXT("Class name without prefix, e.g. 'HaloAttributes' → UAS_HaloAttributes") },
			{ TEXT("attributes"),  TEXT("array"),  TEXT("Array of attribute name strings, e.g. ['Health','MaxHealth']"), false },
			{ TEXT("subdir"),      TEXT("string"), TEXT("Subdirectory (default: 'GAS/AttributeSets')"), false },
			{ TEXT("force"),       TEXT("boolean"),TEXT("Overwrite if file exists"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppCreateAttributeSet(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// cpp_add_uproperty
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_add_uproperty");
		Def.Description = TEXT("Append a UPROPERTY declaration to an existing .h file inside the class body.");
		Def.Params = {
			{ TEXT("file"),        TEXT("string"), TEXT("Path relative to Source/ProjectName/, e.g. 'Characters/HaloCharacter.h'") },
			{ TEXT("type"),        TEXT("string"), TEXT("C++ type, e.g. 'float', 'UStaticMeshComponent*'") },
			{ TEXT("name"),        TEXT("string"), TEXT("Property name") },
			{ TEXT("specifiers"),  TEXT("string"), TEXT("UPROPERTY specifiers, e.g. 'EditAnywhere, BlueprintReadWrite, Category=\"Halo\"'"), false },
			{ TEXT("section"),     TEXT("string"), TEXT("'public', 'protected', or 'private' (default: public)"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppAddUProperty(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// cpp_add_ufunction
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_add_ufunction");
		Def.Description = TEXT("Append a UFUNCTION declaration to .h and a stub implementation to .cpp.");
		Def.Params = {
			{ TEXT("header_file"), TEXT("string"), TEXT("Header path relative to Source/ProjectName/") },
			{ TEXT("return_type"), TEXT("string"), TEXT("Return type, e.g. 'void', 'float'") },
			{ TEXT("name"),        TEXT("string"), TEXT("Function name") },
			{ TEXT("params"),      TEXT("string"), TEXT("Parameter list string, e.g. 'float DeltaTime, int32 Count'"), false },
			{ TEXT("specifiers"),  TEXT("string"), TEXT("UFUNCTION specifiers, e.g. 'BlueprintCallable, Category=\"Halo\"'"), false },
			{ TEXT("section"),     TEXT("string"), TEXT("'public', 'protected', or 'private' (default: public)"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppAddUFunction(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// cpp_add_include
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_add_include");
		Def.Description = TEXT("Add a #include line to an existing .h or .cpp file.");
		Def.Params = {
			{ TEXT("file"),    TEXT("string"), TEXT("File path relative to Source/ProjectName/") },
			{ TEXT("include"), TEXT("string"), TEXT("Include path, e.g. 'GameFramework/Character.h'") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppAddInclude(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// cpp_hot_reload
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_hot_reload");
		Def.Description = TEXT("Trigger IHotReloadModule hot reload of the project module in the editor.");
		Def.Params = {};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppHotReload(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// cpp_get_class_info
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_get_class_info");
		Def.Description = TEXT("Return class hierarchy, UPROPERTYs, and UFUNCTIONs for a loaded UClass.");
		Def.Params = {
			{ TEXT("class_name"), TEXT("string"), TEXT("Class name with prefix, e.g. 'AHaloCharacter' or '/Script/ProjectAF1.AHaloCharacter'") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppGetClassInfo(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// cpp_run_ubt
	{
		FMCPToolDef Def;
		Def.Name = TEXT("cpp_run_ubt");
		Def.Description = TEXT("Run UnrealBuildTool for a full project compile (not hot reload).");
		Def.Params = {
			{ TEXT("configuration"), TEXT("string"), TEXT("Build config: Development (default), Debug, Shipping"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return CppRunUBT(A); };
		OutTools.Add(MoveTemp(Def));
	}
}

// ---------------------------------------------------------------------------
// Template generators
// ---------------------------------------------------------------------------

static FString MakeActorHeader(const FString& ClassName, const FString& Parent, const FString& API)
{
	FString ParentHeader = Parent.Contains(TEXT("Character")) ? TEXT("GameFramework/Character.h") :
	                       Parent.Contains(TEXT("Pawn"))      ? TEXT("GameFramework/Pawn.h") :
	                                                            TEXT("GameFramework/Actor.h");
	return FString::Printf(
		TEXT("#pragma once\n\n")
		TEXT("#include \"CoreMinimal.h\"\n")
		TEXT("#include \"%s\"\n")
		TEXT("#include \"A%s.generated.h\"\n\n")
		TEXT("UCLASS()\n")
		TEXT("class %s A%s : public %s\n")
		TEXT("{\n")
		TEXT("\tGENERATED_BODY()\n\n")
		TEXT("public:\n")
		TEXT("\tA%s();\n\n")
		TEXT("protected:\n")
		TEXT("\tvirtual void BeginPlay() override;\n\n")
		TEXT("public:\n")
		TEXT("\tvirtual void Tick(float DeltaTime) override;\n")
		TEXT("};\n"),
		*ParentHeader, *ClassName, *API, *ClassName, *Parent, *ClassName);
}

static FString MakeActorCpp(const FString& ClassName, const FString& SubDir)
{
	FString IncludePath = SubDir.IsEmpty()
		? FString::Printf(TEXT("A%s.h"), *ClassName)
		: FString::Printf(TEXT("%s/A%s.h"), *SubDir, *ClassName);
	return FString::Printf(
		TEXT("#include \"%s\"\n\n")
		TEXT("A%s::A%s()\n")
		TEXT("{\n")
		TEXT("\tPrimaryActorTick.bCanEverTick = true;\n")
		TEXT("}\n\n")
		TEXT("void A%s::BeginPlay()\n")
		TEXT("{\n")
		TEXT("\tSuper::BeginPlay();\n")
		TEXT("}\n\n")
		TEXT("void A%s::Tick(float DeltaTime)\n")
		TEXT("{\n")
		TEXT("\tSuper::Tick(DeltaTime);\n")
		TEXT("}\n"),
		*IncludePath, *ClassName, *ClassName, *ClassName, *ClassName);
}

static FString MakeComponentHeader(const FString& ClassName, const FString& Parent, const FString& API)
{
	return FString::Printf(
		TEXT("#pragma once\n\n")
		TEXT("#include \"CoreMinimal.h\"\n")
		TEXT("#include \"Components/ActorComponent.h\"\n")
		TEXT("#include \"U%s.generated.h\"\n\n")
		TEXT("UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))\n")
		TEXT("class %s U%s : public %s\n")
		TEXT("{\n")
		TEXT("\tGENERATED_BODY()\n\n")
		TEXT("public:\n")
		TEXT("\tU%s();\n\n")
		TEXT("protected:\n")
		TEXT("\tvirtual void BeginPlay() override;\n\n")
		TEXT("public:\n")
		TEXT("\tvirtual void TickComponent(float DeltaTime, ELevelTick TickType,\n")
		TEXT("\t\tFActorComponentTickFunction* ThisTickFunction) override;\n")
		TEXT("};\n"),
		*ClassName, *API, *ClassName, *Parent, *ClassName);
}

static FString MakeComponentCpp(const FString& ClassName, const FString& SubDir)
{
	FString IncludePath = SubDir.IsEmpty()
		? FString::Printf(TEXT("U%s.h"), *ClassName)
		: FString::Printf(TEXT("%s/U%s.h"), *SubDir, *ClassName);
	return FString::Printf(
		TEXT("#include \"%s\"\n\n")
		TEXT("U%s::U%s()\n")
		TEXT("{\n")
		TEXT("\tPrimaryComponentTick.bCanEverTick = true;\n")
		TEXT("}\n\n")
		TEXT("void U%s::BeginPlay()\n")
		TEXT("{\n")
		TEXT("\tSuper::BeginPlay();\n")
		TEXT("}\n\n")
		TEXT("void U%s::TickComponent(float DeltaTime, ELevelTick TickType,\n")
		TEXT("\tFActorComponentTickFunction* ThisTickFunction)\n")
		TEXT("{\n")
		TEXT("\tSuper::TickComponent(DeltaTime, TickType, ThisTickFunction);\n")
		TEXT("}\n"),
		*IncludePath, *ClassName, *ClassName, *ClassName, *ClassName);
}

static FString MakeInterfaceHeader(const FString& ClassName, const FString& API)
{
	return FString::Printf(
		TEXT("#pragma once\n\n")
		TEXT("#include \"CoreMinimal.h\"\n")
		TEXT("#include \"UObject/Interface.h\"\n")
		TEXT("#include \"I%s.generated.h\"\n\n")
		TEXT("UINTERFACE(MinimalAPI, BlueprintType)\n")
		TEXT("class U%s : public UInterface\n")
		TEXT("{\n")
		TEXT("\tGENERATED_BODY()\n")
		TEXT("};\n\n")
		TEXT("class %s I%s\n")
		TEXT("{\n")
		TEXT("\tGENERATED_BODY()\n\n")
		TEXT("public:\n")
		TEXT("\t// Add interface functions here. Pure virtual = must implement in Blueprint/C++.\n")
		TEXT("};\n"),
		*ClassName, *ClassName, *API, *ClassName);
}

static FString MakeGASAbilityHeader(const FString& ClassName, const FString& API)
{
	return FString::Printf(
		TEXT("#pragma once\n\n")
		TEXT("#include \"CoreMinimal.h\"\n")
		TEXT("#include \"Abilities/GameplayAbility.h\"\n")
		TEXT("#include \"GA_%s.generated.h\"\n\n")
		TEXT("UCLASS()\n")
		TEXT("class %s UGA_%s : public UGameplayAbility\n")
		TEXT("{\n")
		TEXT("\tGENERATED_BODY()\n\n")
		TEXT("public:\n")
		TEXT("\tUGA_%s();\n\n")
		TEXT("\tvirtual void ActivateAbility(\n")
		TEXT("\t\tconst FGameplayAbilitySpecHandle Handle,\n")
		TEXT("\t\tconst FGameplayAbilityActorInfo* ActorInfo,\n")
		TEXT("\t\tconst FGameplayAbilityActivationInfo ActivationInfo,\n")
		TEXT("\t\tconst FGameplayEventData* TriggerEventData) override;\n\n")
		TEXT("\tvirtual void EndAbility(\n")
		TEXT("\t\tconst FGameplayAbilitySpecHandle Handle,\n")
		TEXT("\t\tconst FGameplayAbilityActorInfo* ActorInfo,\n")
		TEXT("\t\tconst FGameplayAbilityActivationInfo ActivationInfo,\n")
		TEXT("\t\tbool bReplicateEndAbility, bool bWasCancelled) override;\n")
		TEXT("};\n"),
		*ClassName, *API, *ClassName, *ClassName);
}

static FString MakeGASAbilityCpp(const FString& ClassName, const FString& SubDir)
{
	FString IncludePath = SubDir.IsEmpty()
		? FString::Printf(TEXT("GA_%s.h"), *ClassName)
		: FString::Printf(TEXT("%s/GA_%s.h"), *SubDir, *ClassName);
	return FString::Printf(
		TEXT("#include \"%s\"\n\n")
		TEXT("UGA_%s::UGA_%s()\n")
		TEXT("{\n")
		TEXT("\tInstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;\n")
		TEXT("}\n\n")
		TEXT("void UGA_%s::ActivateAbility(\n")
		TEXT("\tconst FGameplayAbilitySpecHandle Handle,\n")
		TEXT("\tconst FGameplayAbilityActorInfo* ActorInfo,\n")
		TEXT("\tconst FGameplayAbilityActivationInfo ActivationInfo,\n")
		TEXT("\tconst FGameplayEventData* TriggerEventData)\n")
		TEXT("{\n")
		TEXT("\tSuper::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);\n\n")
		TEXT("\t// TODO: implement ability logic\n\n")
		TEXT("\tEndAbility(Handle, ActorInfo, ActivationInfo, true, false);\n")
		TEXT("}\n\n")
		TEXT("void UGA_%s::EndAbility(\n")
		TEXT("\tconst FGameplayAbilitySpecHandle Handle,\n")
		TEXT("\tconst FGameplayAbilityActorInfo* ActorInfo,\n")
		TEXT("\tconst FGameplayAbilityActivationInfo ActivationInfo,\n")
		TEXT("\tbool bReplicateEndAbility, bool bWasCancelled)\n")
		TEXT("{\n")
		TEXT("\tSuper::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);\n")
		TEXT("}\n"),
		*IncludePath, *ClassName, *ClassName, *ClassName, *ClassName);
}

static FString MakeGASEffectHeader(const FString& ClassName, const FString& API)
{
	return FString::Printf(
		TEXT("#pragma once\n\n")
		TEXT("#include \"CoreMinimal.h\"\n")
		TEXT("#include \"GameplayEffect.h\"\n")
		TEXT("#include \"GE_%s.generated.h\"\n\n")
		TEXT("UCLASS()\n")
		TEXT("class %s UGE_%s : public UGameplayEffect\n")
		TEXT("{\n")
		TEXT("\tGENERATED_BODY()\n\n")
		TEXT("public:\n")
		TEXT("\tUGE_%s();\n")
		TEXT("};\n"),
		*ClassName, *API, *ClassName, *ClassName);
}

static FString MakeGASEffectCpp(const FString& ClassName, const FString& SubDir)
{
	FString IncludePath = SubDir.IsEmpty()
		? FString::Printf(TEXT("GE_%s.h"), *ClassName)
		: FString::Printf(TEXT("%s/GE_%s.h"), *SubDir, *ClassName);
	return FString::Printf(
		TEXT("#include \"%s\"\n\n")
		TEXT("UGE_%s::UGE_%s()\n")
		TEXT("{\n")
		TEXT("\t// Configure duration, period, and modifiers in Blueprint\n")
		TEXT("\t// or set them here via FGameplayModifierInfo\n")
		TEXT("}\n"),
		*IncludePath, *ClassName, *ClassName);
}

static FString MakeAttributeSetHeader(const FString& ClassName, const TArray<FString>& Attrs, const FString& API)
{
	// Build attribute declarations
	FString AttrDecls;
	FString RepNotifyDecls;
	for (const FString& Attr : Attrs)
	{
		AttrDecls += FString::Printf(
			TEXT("\tUPROPERTY(BlueprintReadOnly, Category = \"Attributes\", ReplicatedUsing = OnRep_%s)\n")
			TEXT("\tFGameplayAttributeData %s;\n")
			TEXT("\tATTRIBUTE_ACCESSORS(UAS_%s, %s)\n\n"),
			*Attr, *Attr, *ClassName, *Attr);

		RepNotifyDecls += FString::Printf(
			TEXT("\tUFUNCTION()\n")
			TEXT("\tvirtual void OnRep_%s(const FGameplayAttributeData& Old%s);\n\n"),
			*Attr, *Attr);
	}

	return FString::Printf(
		TEXT("#pragma once\n\n")
		TEXT("#include \"CoreMinimal.h\"\n")
		TEXT("#include \"AttributeSet.h\"\n")
		TEXT("#include \"AbilitySystemComponent.h\"\n")
		TEXT("#include \"AS_%s.generated.h\"\n\n")
		TEXT("#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \\\n")
		TEXT("\tGAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \\\n")
		TEXT("\tGAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \\\n")
		TEXT("\tGAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \\\n")
		TEXT("\tGAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)\n\n")
		TEXT("UCLASS()\n")
		TEXT("class %s UAS_%s : public UAttributeSet\n")
		TEXT("{\n")
		TEXT("\tGENERATED_BODY()\n\n")
		TEXT("public:\n")
		TEXT("\tUAS_%s();\n\n")
		TEXT("\tvirtual void GetLifetimeReplicatedProps(\n")
		TEXT("\t\tTArray<FLifetimeProperty>& OutLifetimeProps) const override;\n\n")
		TEXT("\tvirtual void PreAttributeChange(\n")
		TEXT("\t\tconst FGameplayAttribute& Attribute, float& NewValue) override;\n\n")
		TEXT("%s")
		TEXT("%s")
		TEXT("};\n"),
		*ClassName, *API, *ClassName, *ClassName,
		*AttrDecls, *RepNotifyDecls);
}

static FString MakeAttributeSetCpp(const FString& ClassName, const TArray<FString>& Attrs, const FString& SubDir)
{
	FString IncludePath = SubDir.IsEmpty()
		? FString::Printf(TEXT("AS_%s.h"), *ClassName)
		: FString::Printf(TEXT("%s/AS_%s.h"), *SubDir, *ClassName);

	FString RepProps;
	FString RepNotifyImpls;
	for (const FString& Attr : Attrs)
	{
		RepProps += FString::Printf(
			TEXT("\tDOREPLIFETIME_CONDITION_NOTIFY(UAS_%s, %s, COND_None, REPNOTIFY_Always);\n"),
			*ClassName, *Attr);

		RepNotifyImpls += FString::Printf(
			TEXT("void UAS_%s::OnRep_%s(const FGameplayAttributeData& Old%s)\n")
			TEXT("{\n")
			TEXT("\tGAMEPLAYATTRIBUTE_REPNOTIFY(UAS_%s, %s, Old%s);\n")
			TEXT("}\n\n"),
			*ClassName, *Attr, *Attr, *ClassName, *Attr, *Attr);
	}

	return FString::Printf(
		TEXT("#include \"%s\"\n")
		TEXT("#include \"Net/UnrealNetwork.h\"\n\n")
		TEXT("UAS_%s::UAS_%s()\n")
		TEXT("{\n")
		TEXT("}\n\n")
		TEXT("void UAS_%s::GetLifetimeReplicatedProps(\n")
		TEXT("\tTArray<FLifetimeProperty>& OutLifetimeProps) const\n")
		TEXT("{\n")
		TEXT("\tSuper::GetLifetimeReplicatedProps(OutLifetimeProps);\n")
		TEXT("%s")
		TEXT("}\n\n")
		TEXT("void UAS_%s::PreAttributeChange(\n")
		TEXT("\tconst FGameplayAttribute& Attribute, float& NewValue)\n")
		TEXT("{\n")
		TEXT("\tSuper::PreAttributeChange(Attribute, NewValue);\n")
		TEXT("}\n\n")
		TEXT("%s"),
		*IncludePath, *ClassName, *ClassName, *ClassName,
		*RepProps, *ClassName, *RepNotifyImpls);
}

// ---------------------------------------------------------------------------
// Implementations
// ---------------------------------------------------------------------------

FMCPToolResult FMCPCppCodegenTools::CppCreateActor(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassName, Parent, SubDir;
	bool bForce = false;
	if (!Args->TryGetStringField(TEXT("class_name"), ClassName))
		return FMCPToolResult::Error(TEXT("Missing: class_name"));
	Args->TryGetStringField(TEXT("parent"),  Parent);
	Args->TryGetStringField(TEXT("subdir"),  SubDir);
	Args->TryGetBoolField  (TEXT("force"),   bForce);
	if (Parent.IsEmpty()) Parent = TEXT("AActor");

	FString API = GetAPImacro();
	FString Dir = GetSourceDir(SubDir);
	FString HPath = FPaths::Combine(Dir, FString::Printf(TEXT("A%s.h"),   *ClassName));
	FString CPath = FPaths::Combine(Dir, FString::Printf(TEXT("A%s.cpp"), *ClassName));

	FString Err;
	if (!WriteFile(HPath, MakeActorHeader(ClassName, Parent, API), bForce, Err))
		return FMCPToolResult::Error(Err);
	if (!WriteFile(CPath, MakeActorCpp(ClassName, SubDir), bForce, Err))
		return FMCPToolResult::Error(Err);

	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("header"), HPath);
	Data->SetStringField(TEXT("source"), CPath);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created A%s — run cpp_hot_reload to compile"), *ClassName), Data);
}

FMCPToolResult FMCPCppCodegenTools::CppCreateComponent(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassName, Parent, SubDir;
	bool bForce = false;
	if (!Args->TryGetStringField(TEXT("class_name"), ClassName))
		return FMCPToolResult::Error(TEXT("Missing: class_name"));
	Args->TryGetStringField(TEXT("parent"), Parent);
	Args->TryGetStringField(TEXT("subdir"), SubDir);
	Args->TryGetBoolField  (TEXT("force"),  bForce);
	if (Parent.IsEmpty()) Parent = TEXT("UActorComponent");

	FString API = GetAPImacro();
	FString Dir = GetSourceDir(SubDir);
	FString HPath = FPaths::Combine(Dir, FString::Printf(TEXT("U%s.h"),   *ClassName));
	FString CPath = FPaths::Combine(Dir, FString::Printf(TEXT("U%s.cpp"), *ClassName));

	FString Err;
	if (!WriteFile(HPath, MakeComponentHeader(ClassName, Parent, API), bForce, Err)) return FMCPToolResult::Error(Err);
	if (!WriteFile(CPath, MakeComponentCpp(ClassName, SubDir), bForce, Err))         return FMCPToolResult::Error(Err);

	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("header"), HPath);
	Data->SetStringField(TEXT("source"), CPath);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created U%s — run cpp_hot_reload to compile"), *ClassName), Data);
}

FMCPToolResult FMCPCppCodegenTools::CppCreateInterface(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassName, SubDir;
	bool bForce = false;
	if (!Args->TryGetStringField(TEXT("class_name"), ClassName))
		return FMCPToolResult::Error(TEXT("Missing: class_name"));
	Args->TryGetStringField(TEXT("subdir"), SubDir);
	Args->TryGetBoolField  (TEXT("force"),  bForce);

	FString API = GetAPImacro();
	FString Dir = GetSourceDir(SubDir);
	FString HPath = FPaths::Combine(Dir, FString::Printf(TEXT("I%s.h"), *ClassName));

	FString Err;
	if (!WriteFile(HPath, MakeInterfaceHeader(ClassName, API), bForce, Err))
		return FMCPToolResult::Error(Err);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created I%s / U%s interface — run cpp_hot_reload to compile"), *ClassName, *ClassName),
		[&]{ auto D = MakeShared<FJsonObject>(); D->SetStringField(TEXT("header"), HPath); return D; }());
}

FMCPToolResult FMCPCppCodegenTools::CppCreateGASAbility(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassName, SubDir;
	bool bForce = false;
	if (!Args->TryGetStringField(TEXT("class_name"), ClassName))
		return FMCPToolResult::Error(TEXT("Missing: class_name"));
	Args->TryGetStringField(TEXT("subdir"), SubDir);
	Args->TryGetBoolField  (TEXT("force"),  bForce);
	if (SubDir.IsEmpty()) SubDir = TEXT("GAS/Abilities");

	FString API = GetAPImacro();
	FString Dir = GetSourceDir(SubDir);
	FString HPath = FPaths::Combine(Dir, FString::Printf(TEXT("GA_%s.h"),   *ClassName));
	FString CPath = FPaths::Combine(Dir, FString::Printf(TEXT("GA_%s.cpp"), *ClassName));

	FString Err;
	if (!WriteFile(HPath, MakeGASAbilityHeader(ClassName, API), bForce, Err)) return FMCPToolResult::Error(Err);
	if (!WriteFile(CPath, MakeGASAbilityCpp(ClassName, SubDir), bForce, Err)) return FMCPToolResult::Error(Err);

	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("header"), HPath);
	Data->SetStringField(TEXT("source"), CPath);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created UGA_%s — run cpp_hot_reload to compile"), *ClassName), Data);
}

FMCPToolResult FMCPCppCodegenTools::CppCreateGASEffect(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassName, SubDir;
	bool bForce = false;
	if (!Args->TryGetStringField(TEXT("class_name"), ClassName))
		return FMCPToolResult::Error(TEXT("Missing: class_name"));
	Args->TryGetStringField(TEXT("subdir"), SubDir);
	Args->TryGetBoolField  (TEXT("force"),  bForce);
	if (SubDir.IsEmpty()) SubDir = TEXT("GAS/Effects");

	FString API = GetAPImacro();
	FString Dir = GetSourceDir(SubDir);
	FString HPath = FPaths::Combine(Dir, FString::Printf(TEXT("GE_%s.h"),   *ClassName));
	FString CPath = FPaths::Combine(Dir, FString::Printf(TEXT("GE_%s.cpp"), *ClassName));

	FString Err;
	if (!WriteFile(HPath, MakeGASEffectHeader(ClassName, API), bForce, Err)) return FMCPToolResult::Error(Err);
	if (!WriteFile(CPath, MakeGASEffectCpp(ClassName, SubDir), bForce, Err)) return FMCPToolResult::Error(Err);

	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("header"), HPath);
	Data->SetStringField(TEXT("source"), CPath);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created UGE_%s — run cpp_hot_reload to compile"), *ClassName), Data);
}

FMCPToolResult FMCPCppCodegenTools::CppCreateAttributeSet(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassName, SubDir;
	bool bForce = false;
	if (!Args->TryGetStringField(TEXT("class_name"), ClassName))
		return FMCPToolResult::Error(TEXT("Missing: class_name"));
	Args->TryGetStringField(TEXT("subdir"), SubDir);
	Args->TryGetBoolField  (TEXT("force"),  bForce);
	if (SubDir.IsEmpty()) SubDir = TEXT("GAS/AttributeSets");

	// Parse attribute names
	TArray<FString> Attrs;
	const TArray<TSharedPtr<FJsonValue>>* AttrsArr;
	if (Args->TryGetArrayField(TEXT("attributes"), AttrsArr))
	{
		for (const TSharedPtr<FJsonValue>& V : *AttrsArr)
			Attrs.Add(V->AsString());
	}
	if (Attrs.IsEmpty()) Attrs.Add(TEXT("Health")); // default

	FString API = GetAPImacro();
	FString Dir = GetSourceDir(SubDir);
	FString HPath = FPaths::Combine(Dir, FString::Printf(TEXT("AS_%s.h"),   *ClassName));
	FString CPath = FPaths::Combine(Dir, FString::Printf(TEXT("AS_%s.cpp"), *ClassName));

	FString Err;
	if (!WriteFile(HPath, MakeAttributeSetHeader(ClassName, Attrs, API), bForce, Err)) return FMCPToolResult::Error(Err);
	if (!WriteFile(CPath, MakeAttributeSetCpp(ClassName, Attrs, SubDir), bForce, Err)) return FMCPToolResult::Error(Err);

	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("header"), HPath);
	Data->SetStringField(TEXT("source"), CPath);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created UAS_%s with %d attribute(s) — run cpp_hot_reload to compile"),
		*ClassName, Attrs.Num()), Data);
}

FMCPToolResult FMCPCppCodegenTools::CppAddUProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString RelFile, Type, Name, Specifiers, Section;
	if (!Args->TryGetStringField(TEXT("file"), RelFile)) return FMCPToolResult::Error(TEXT("Missing: file"));
	if (!Args->TryGetStringField(TEXT("type"), Type))    return FMCPToolResult::Error(TEXT("Missing: type"));
	if (!Args->TryGetStringField(TEXT("name"), Name))    return FMCPToolResult::Error(TEXT("Missing: name"));
	Args->TryGetStringField(TEXT("specifiers"), Specifiers);
	Args->TryGetStringField(TEXT("section"),    Section);
	if (Specifiers.IsEmpty()) Specifiers = TEXT("EditAnywhere, BlueprintReadWrite, Category=\"Default\"");
	if (Section.IsEmpty())    Section    = TEXT("public");

	FString FilePath = FPaths::Combine(GetSourceDir(), RelFile);
	FString Content, Err;
	if (!ReadFile(FilePath, Content, Err)) return FMCPToolResult::Error(Err);

	// Insert before the closing '};' of the class
	FString NewProp = FString::Printf(TEXT("\n\tUPROPERTY(%s)\n\t%s %s;\n"), *Specifiers, *Type, *Name);

	int32 BracePos = Content.Find(TEXT("};"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (BracePos == INDEX_NONE)
		return FMCPToolResult::Error(TEXT("Could not find closing '}; in file"));

	Content.InsertAt(BracePos, NewProp);

	if (!FFileHelper::SaveStringToFile(Content, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to write: %s"), *FilePath));

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added UPROPERTY %s %s to %s"), *Type, *Name, *RelFile));
}

FMCPToolResult FMCPCppCodegenTools::CppAddUFunction(const TSharedPtr<FJsonObject>& Args)
{
	FString HeaderFile, ReturnType, FuncName, Params, Specifiers, Section;
	if (!Args->TryGetStringField(TEXT("header_file"),  HeaderFile))  return FMCPToolResult::Error(TEXT("Missing: header_file"));
	if (!Args->TryGetStringField(TEXT("return_type"),  ReturnType))  return FMCPToolResult::Error(TEXT("Missing: return_type"));
	if (!Args->TryGetStringField(TEXT("name"),         FuncName))    return FMCPToolResult::Error(TEXT("Missing: name"));
	Args->TryGetStringField(TEXT("params"),     Params);
	Args->TryGetStringField(TEXT("specifiers"), Specifiers);
	Args->TryGetStringField(TEXT("section"),    Section);
	if (Specifiers.IsEmpty()) Specifiers = TEXT("BlueprintCallable, Category=\"Default\"");

	FString HPath = FPaths::Combine(GetSourceDir(), HeaderFile);
	FString HContent, Err;
	if (!ReadFile(HPath, HContent, Err)) return FMCPToolResult::Error(Err);

	// Add declaration to header before closing '};'
	FString Decl = FString::Printf(TEXT("\n\tUFUNCTION(%s)\n\t%s %s(%s);\n"),
		*Specifiers, *ReturnType, *FuncName, *Params);

	int32 BracePos = HContent.Find(TEXT("};"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (BracePos == INDEX_NONE)
		return FMCPToolResult::Error(TEXT("Could not find closing '};' in header"));

	HContent.InsertAt(BracePos, Decl);
	FFileHelper::SaveStringToFile(HContent, *HPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	// Try to add stub to matching .cpp
	FString CppFile = FPaths::ChangeExtension(HPath, TEXT("cpp"));
	FString ClassName;
	{
		// Extract class name from filename (e.g. AHaloCharacter.h → AHaloCharacter)
		ClassName = FPaths::GetBaseFilename(HPath);
	}

	FString ReturnStmt = ReturnType.Equals(TEXT("void")) ? TEXT("") : TEXT("\treturn {};\n");
	FString Stub = FString::Printf(TEXT("\n%s %s::%s(%s)\n{\n%s}\n"),
		*ReturnType, *ClassName, *FuncName, *Params, *ReturnStmt);

	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*CppFile))
	{
		FString CppContent;
		FFileHelper::LoadFileToString(CppContent, *CppFile);
		CppContent += Stub;
		FFileHelper::SaveStringToFile(CppContent, *CppFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added UFUNCTION %s %s to %s"), *ReturnType, *FuncName, *HeaderFile));
}

FMCPToolResult FMCPCppCodegenTools::CppAddInclude(const TSharedPtr<FJsonObject>& Args)
{
	FString RelFile, Include;
	if (!Args->TryGetStringField(TEXT("file"),    RelFile))  return FMCPToolResult::Error(TEXT("Missing: file"));
	if (!Args->TryGetStringField(TEXT("include"), Include))  return FMCPToolResult::Error(TEXT("Missing: include"));

	FString FilePath = FPaths::Combine(GetSourceDir(), RelFile);
	FString Content, Err;
	if (!ReadFile(FilePath, Content, Err)) return FMCPToolResult::Error(Err);

	FString IncludeLine = FString::Printf(TEXT("#include \"%s\""), *Include);

	// Don't add if already present
	if (Content.Contains(IncludeLine))
		return FMCPToolResult::Success(FString::Printf(TEXT("Already included: %s"), *Include));

	// Insert after the last #include line
	int32 LastInclude = INDEX_NONE;
	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines);
	for (int32 i = Lines.Num() - 1; i >= 0; --i)
	{
		if (Lines[i].StartsWith(TEXT("#include")))
		{
			LastInclude = i;
			break;
		}
	}

	if (LastInclude != INDEX_NONE)
	{
		Lines.Insert(IncludeLine, LastInclude + 1);
		Content = FString::Join(Lines, TEXT("\n")) + TEXT("\n");
	}
	else
	{
		Content = IncludeLine + TEXT("\n") + Content;
	}

	FFileHelper::SaveStringToFile(Content, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	return FMCPToolResult::Success(FString::Printf(TEXT("Added #include \"%s\" to %s"), *Include, *RelFile));
}

FMCPToolResult FMCPCppCodegenTools::CppHotReload(const TSharedPtr<FJsonObject>& Args)
{
	IHotReloadInterface* HotReload = FModuleManager::GetModulePtr<IHotReloadInterface>("HotReload");
	if (!HotReload)
		return FMCPToolResult::Error(TEXT("HotReload module not available"));

	ECompilationResult::Type Result = HotReload->DoHotReloadFromEditor(EHotReloadFlags::None);

	if (Result == ECompilationResult::Succeeded || Result == ECompilationResult::UpToDate)
	{
		return FMCPToolResult::Success(FString::Printf(
			TEXT("Hot reload succeeded (%s)"),
			Result == ECompilationResult::UpToDate ? TEXT("up to date") : TEXT("compiled")));
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Hot reload failed (result code: %d)"), (int32)Result));
}

FMCPToolResult FMCPCppCodegenTools::CppGetClassInfo(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassName;
	if (!Args->TryGetStringField(TEXT("class_name"), ClassName))
		return FMCPToolResult::Error(TEXT("Missing: class_name"));

	// Try multiple resolution strategies
	UClass* Class = FindObject<UClass>(nullptr, *ClassName);
	if (!Class) Class = LoadObject<UClass>(nullptr, *ClassName);
	if (!Class)
	{
		// Try /Script/ProjectName.ClassName
		FString ProjectName = FApp::GetProjectName();
		Class = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/%s.%s"), *ProjectName, *ClassName));
	}
	if (!Class)
		return FMCPToolResult::Error(FString::Printf(TEXT("Class not found: %s"), *ClassName));

	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("class"),      Class->GetName());
	Data->SetStringField(TEXT("path"),       Class->GetPathName());
	Data->SetStringField(TEXT("parent"),     Class->GetSuperClass() ? Class->GetSuperClass()->GetName() : TEXT("None"));

	// Properties
	TArray<TSharedPtr<FJsonValue>> Props;
	for (TFieldIterator<FProperty> It(Class, EFieldIterationFlags::None); It; ++It)
	{
		FProperty* Prop = *It;
		auto P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("name"),     Prop->GetName());
		P->SetStringField(TEXT("type"),     Prop->GetClass()->GetName());
		P->SetBoolField  (TEXT("editable"), Prop->HasAnyPropertyFlags(CPF_Edit));
		P->SetBoolField  (TEXT("bp_read"),  Prop->HasAnyPropertyFlags(CPF_BlueprintVisible));
		Props.Add(MakeShared<FJsonValueObject>(P));
	}
	Data->SetArrayField(TEXT("properties"), Props);

	// Functions
	TArray<TSharedPtr<FJsonValue>> Funcs;
	for (TFieldIterator<UFunction> It(Class, EFieldIterationFlags::None); It; ++It)
	{
		UFunction* Func = *It;
		auto F = MakeShared<FJsonObject>();
		F->SetStringField(TEXT("name"),          Func->GetName());
		F->SetBoolField  (TEXT("bp_callable"),   Func->HasAnyFunctionFlags(FUNC_BlueprintCallable));
		F->SetBoolField  (TEXT("bp_pure"),       Func->HasAnyFunctionFlags(FUNC_BlueprintPure));
		F->SetBoolField  (TEXT("event"),         Func->HasAnyFunctionFlags(FUNC_BlueprintEvent));
		Funcs.Add(MakeShared<FJsonValueObject>(F));
	}
	Data->SetArrayField(TEXT("functions"), Funcs);
	Data->SetNumberField(TEXT("property_count"), Props.Num());
	Data->SetNumberField(TEXT("function_count"), Funcs.Num());

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Class %s: %d properties, %d functions"),
		*Class->GetName(), Props.Num(), Funcs.Num()), Data);
}

FMCPToolResult FMCPCppCodegenTools::CppRunUBT(const TSharedPtr<FJsonObject>& Args)
{
	FString Config;
	Args->TryGetStringField(TEXT("configuration"), Config);
	if (Config.IsEmpty()) Config = TEXT("Development");

	FString ProjectName  = FApp::GetProjectName();
	FString ProjectFile  = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());

	// Build.bat/Build.sh live in a platform-specific subfolder (or not, on Windows), and the target
	// platform name passed to UBT is likewise host-specific -- this previously hardcoded the
	// Windows-only Build.bat/Win64 pair, so this tool silently couldn't work on Mac/Linux at all.
#if PLATFORM_MAC
	static const TCHAR* BatchFileRelPath = TEXT("Build/BatchFiles/Mac/Build.sh");
	static const TCHAR* TargetPlatform   = TEXT("Mac");
#elif PLATFORM_LINUX
	static const TCHAR* BatchFileRelPath = TEXT("Build/BatchFiles/Linux/Build.sh");
	static const TCHAR* TargetPlatform   = TEXT("Linux");
#else
	static const TCHAR* BatchFileRelPath = TEXT("Build/BatchFiles/Build.bat");
	static const TCHAR* TargetPlatform   = TEXT("Win64");
#endif

	FString BatchFile = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::EngineDir(), BatchFileRelPath));

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*BatchFile))
		return FMCPToolResult::Error(FString::Printf(TEXT("Build script not found: %s"), *BatchFile));

	FString Args_UBT = FString::Printf(TEXT("%sEditor %s %s \"%s\" -WaitMutex -FromMsBuild"),
		*ProjectName, TargetPlatform, *Config, *ProjectFile);

	uint32 ProcId = 0;
	FProcHandle Proc = FPlatformProcess::CreateProc(
		*BatchFile, *Args_UBT, true, false, false, &ProcId, 0, nullptr, nullptr);

	if (!Proc.IsValid())
		return FMCPToolResult::Error(TEXT("Failed to launch UBT process"));

	FPlatformProcess::CloseProc(Proc);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("UBT launched for %sEditor %s (PID %u) — check Output Log for results"),
		*ProjectName, *Config, ProcId));
}
