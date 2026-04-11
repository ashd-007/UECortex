#include "Tools/MCPBlueprintTools.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "PackageTools.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Misc/PackageName.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/InheritableComponentHandler.h"
#include "UObject/UObjectHash.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

UBlueprint* FMCPBlueprintTools::LoadBP(const FString& Path)
{
	return LoadObject<UBlueprint>(nullptr, *Path);
}

UEdGraph* FMCPBlueprintTools::FindGraph(UBlueprint* BP, const FString& GraphName)
{
	if (!BP) return nullptr;

	if (GraphName.IsEmpty() || GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		return FBlueprintEditorUtils::FindEventGraph(BP);
	}

	// Search function graphs
	for (UEdGraph* G : BP->FunctionGraphs)
	{
		if (G && G->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
			return G;
	}
	// Search ubergraph (event graphs)
	for (UEdGraph* G : BP->UbergraphPages)
	{
		if (G && G->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
			return G;
	}
	return nullptr;
}

UEdGraphNode* FMCPBlueprintTools::FindNodeByName(UEdGraph* Graph, const FString& Name)
{
	if (!Graph) return nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && (Node->GetName().Equals(Name, ESearchCase::IgnoreCase) ||
		             Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString()
		                 .Equals(Name, ESearchCase::IgnoreCase)))
		{
			return Node;
		}
	}
	return nullptr;
}

static UClass* ResolveClass(const FString& Path, UClass* Fallback = nullptr)
{
	if (Path.IsEmpty()) return Fallback;
	UClass* C = LoadObject<UClass>(nullptr, *Path);
	if (!C)
	{
		// Try Blueprint generated class (_C suffix)
		FString GenPath = Path.EndsWith(TEXT("_C")) ? Path : Path + TEXT("_C");
		C = LoadObject<UClass>(nullptr, *GenPath);
	}
	if (!C)
	{
		// Try finding by short name
		C = FindObject<UClass>(nullptr, *Path);
	}
	return C ? C : Fallback;
}

static UEdGraph* GetOrCreateEventGraph(UBlueprint* BP)
{
	UEdGraph* G = FBlueprintEditorUtils::FindEventGraph(BP);
	if (!G && BP->UbergraphPages.Num() > 0)
		G = BP->UbergraphPages[0];
	return G;
}

static FEdGraphPinType MakePinType(const FString& TypeStr)
{
	FEdGraphPinType PT;
	FString T = TypeStr.ToLower();

	if      (T == TEXT("bool"))      PT.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	else if (T == TEXT("int"))       PT.PinCategory = UEdGraphSchema_K2::PC_Int;
	else if (T == TEXT("int64"))     PT.PinCategory = UEdGraphSchema_K2::PC_Int64;
	else if (T == TEXT("float") || T == TEXT("double"))
	{
		PT.PinCategory    = UEdGraphSchema_K2::PC_Real;
		PT.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (T == TEXT("string"))    PT.PinCategory = UEdGraphSchema_K2::PC_String;
	else if (T == TEXT("name"))      PT.PinCategory = UEdGraphSchema_K2::PC_Name;
	else if (T == TEXT("text"))      PT.PinCategory = UEdGraphSchema_K2::PC_Text;
	else if (T == TEXT("vector"))
	{
		PT.PinCategory  = UEdGraphSchema_K2::PC_Struct;
		PT.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (T == TEXT("rotator"))
	{
		PT.PinCategory  = UEdGraphSchema_K2::PC_Struct;
		PT.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (T == TEXT("transform"))
	{
		PT.PinCategory  = UEdGraphSchema_K2::PC_Struct;
		PT.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else if (T == TEXT("actor"))
	{
		PT.PinCategory = UEdGraphSchema_K2::PC_Object;
		PT.PinSubCategoryObject = AActor::StaticClass();
	}
	else if (T == TEXT("object"))
	{
		PT.PinCategory = UEdGraphSchema_K2::PC_Object;
		PT.PinSubCategoryObject = UObject::StaticClass();
	}
	else
	{
		// Default to wildcard
		PT.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
	}
	return PT;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMCPBlueprintTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	// blueprint_create
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_create");
		Def.Description = TEXT("Create a new Blueprint asset at the given content path.");
		Def.Params = {
			{ TEXT("path"),         TEXT("string"), TEXT("Content path, e.g. '/Game/Blueprints/BP_MyActor'") },
			{ TEXT("parent_class"), TEXT("string"), TEXT("Parent class path, e.g. '/Script/Engine.Actor'"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintCreate(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_add_component
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_add_component");
		Def.Description = TEXT("Add a component to a Blueprint via its SimpleConstructionScript.");
		Def.Params = {
			{ TEXT("blueprint_path"),  TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("component_type"),  TEXT("string"), TEXT("Component class name, e.g. 'StaticMeshComponent'") },
			{ TEXT("component_name"),  TEXT("string"), TEXT("Variable name for the new component"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintAddComponent(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_set_component_property
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_set_component_property");
		Def.Description = TEXT("Set a property on a Blueprint component template.");
		Def.Params = {
			{ TEXT("blueprint_path"),  TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("component_name"),  TEXT("string"), TEXT("Component variable name") },
			{ TEXT("property"),        TEXT("string"), TEXT("Property name") },
			{ TEXT("value"),           TEXT("string"), TEXT("Value as string") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintSetComponentProperty(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_add_variable
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_add_variable");
		Def.Description = TEXT("Add a new Blueprint variable with the given type.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("name"),           TEXT("string"), TEXT("Variable name") },
			{ TEXT("type"),           TEXT("string"), TEXT("Type: bool/int/float/string/name/vector/rotator/transform/actor/object") },
			{ TEXT("default_value"),  TEXT("string"), TEXT("Default value as string"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintAddVariable(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_set_variable
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_set_variable");
		Def.Description = TEXT("Set the default value of a Blueprint variable on the CDO.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("name"),           TEXT("string"), TEXT("Variable name") },
			{ TEXT("value"),          TEXT("string"), TEXT("New default value as string") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintSetVariable(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_add_function
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_add_function");
		Def.Description = TEXT("Add a new function graph to a Blueprint.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("function_name"),  TEXT("string"), TEXT("Name of the new function") },
			{ TEXT("inputs"),         TEXT("array"),  TEXT("Array of {name, type} input params"), false },
			{ TEXT("outputs"),        TEXT("array"),  TEXT("Array of {name, type} output params"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintAddFunction(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_add_event_node
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_add_event_node");
		Def.Description = TEXT("Add an event node (BeginPlay/Tick/EndPlay or custom) to the event graph.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("event_name"),     TEXT("string"), TEXT("Event: BeginPlay, Tick, EndPlay, or custom name") },
			{ TEXT("x"),              TEXT("number"), TEXT("Node X position"), false },
			{ TEXT("y"),              TEXT("number"), TEXT("Node Y position"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintAddEventNode(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_add_function_call
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_add_function_call");
		Def.Description = TEXT("Add a function call node to a Blueprint graph.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("graph_name"),     TEXT("string"), TEXT("Target graph name (default: EventGraph)"), false },
			{ TEXT("function"),       TEXT("string"), TEXT("Function name to call") },
			{ TEXT("class_path"),     TEXT("string"), TEXT("Class that owns the function (for static/external calls)"), false },
			{ TEXT("x"),              TEXT("number"), TEXT("Node X position"), false },
			{ TEXT("y"),              TEXT("number"), TEXT("Node Y position"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintAddFunctionCall(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_connect_pins
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_connect_pins");
		Def.Description = TEXT("Connect an output pin of one node to an input pin of another.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("graph_name"),     TEXT("string"), TEXT("Graph name (default: EventGraph)"), false },
			{ TEXT("from_node"),      TEXT("string"), TEXT("Source node name or title") },
			{ TEXT("from_pin"),       TEXT("string"), TEXT("Source pin name") },
			{ TEXT("to_node"),        TEXT("string"), TEXT("Target node name or title") },
			{ TEXT("to_pin"),         TEXT("string"), TEXT("Target pin name") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintConnectPins(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_compile
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_compile");
		Def.Description = TEXT("Compile a Blueprint asset. Returns any errors.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintCompile(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_spawn_in_level
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_spawn_in_level");
		Def.Description = TEXT("Spawn a Blueprint actor in the current level.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("name"),           TEXT("string"), TEXT("Actor label"), false },
			{ TEXT("location"),       TEXT("object"), TEXT("{x,y,z} world location"), false },
			{ TEXT("rotation"),       TEXT("object"), TEXT("{pitch,yaw,roll} in degrees"), false },
			{ TEXT("scale"),          TEXT("object"), TEXT("{x,y,z} scale"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintSpawnInLevel(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_get_graph
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_get_graph");
		Def.Description = TEXT("Return the full node graph of a Blueprint as JSON (nodes, pins, connections).");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("graph_name"),     TEXT("string"), TEXT("Graph name (default: EventGraph)"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintGetGraph(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_find_node
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_find_node");
		Def.Description = TEXT("Find nodes in a Blueprint graph by type or title substring.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("graph_name"),     TEXT("string"), TEXT("Graph name (default: EventGraph)"), false },
			{ TEXT("node_type"),      TEXT("string"), TEXT("Node class name substring, e.g. 'K2Node_Event'"), false },
			{ TEXT("title"),          TEXT("string"), TEXT("Node title substring to match"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintFindNode(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_set_node_property
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_set_node_property");
		Def.Description = TEXT("Set a property on a node in a Blueprint graph.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path") },
			{ TEXT("graph_name"),     TEXT("string"), TEXT("Graph name (default: EventGraph)"), false },
			{ TEXT("node_name"),      TEXT("string"), TEXT("Node name or title") },
			{ TEXT("property"),       TEXT("string"), TEXT("Property name on the node") },
			{ TEXT("value"),          TEXT("string"), TEXT("Value as string") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintSetNodeProperty(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_create_input_action
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_create_input_action");
		Def.Description = TEXT("Create an Enhanced Input InputAction asset.");
		Def.Params = {
			{ TEXT("path"),       TEXT("string"), TEXT("Content path, e.g. '/Game/Input/IA_Jump'") },
			{ TEXT("value_type"), TEXT("string"), TEXT("bool | float | vector2 | vector3 (default: bool)"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintCreateInputAction(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_add_gamemode
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_add_gamemode");
		Def.Description = TEXT("Create a GameMode Blueprint with optional pawn/controller/HUD class refs.");
		Def.Params = {
			{ TEXT("path"),                    TEXT("string"), TEXT("Content path for the new GameMode Blueprint") },
			{ TEXT("default_pawn_class"),      TEXT("string"), TEXT("Default pawn class path"), false },
			{ TEXT("player_controller_class"), TEXT("string"), TEXT("Player controller class path"), false },
			{ TEXT("hud_class"),               TEXT("string"), TEXT("HUD class path"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintAddGameMode(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_set_cdo_property
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_set_cdo_property");
		Def.Description = TEXT("Set a property directly on a Blueprint's Class Default Object (CDO). Handles the Default__ClassName_C lookup internally. Use for Blueprint-defined properties not accessible via component tools.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Content path to the Blueprint asset") },
			{ TEXT("property"),       TEXT("string"), TEXT("Property name on the Blueprint class") },
			{ TEXT("value"),          TEXT("string"), TEXT("Value as string (object refs as content paths, e.g. /Game/Meshes/SK_Hero.SK_Hero)") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintSetCDOProperty(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// blueprint_set_inherited_component_property
	{
		FMCPToolDef Def;
		Def.Name = TEXT("blueprint_set_inherited_component_property");
		Def.Description = TEXT("Set a property on an inherited component (from a parent Blueprint or C++ class) using the InheritableComponentHandler. Use for components like CharacterMesh0 that are not in this Blueprint's own SCS.");
		Def.Params = {
			{ TEXT("blueprint_path"),  TEXT("string"), TEXT("Content path to the Blueprint asset") },
			{ TEXT("component_name"),  TEXT("string"), TEXT("Component variable name, e.g. 'CharacterMesh0' or 'CapsuleComponent'") },
			{ TEXT("property"),        TEXT("string"), TEXT("Property name on the component, e.g. 'SkeletalMesh', 'AnimClass'") },
			{ TEXT("value"),           TEXT("string"), TEXT("Value as string (object refs as content paths)") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return BlueprintSetInheritedComponentProperty(A); };
		OutTools.Add(MoveTemp(Def));
	}
}

// ---------------------------------------------------------------------------
// Implementations
// ---------------------------------------------------------------------------

FMCPToolResult FMCPBlueprintTools::BlueprintCreate(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath;
	if (!Args->TryGetStringField(TEXT("path"), BPPath))
		return FMCPToolResult::Error(TEXT("Missing: path"));

	FString ParentClassPath;
	Args->TryGetStringField(TEXT("parent_class"), ParentClassPath);

	UClass* ParentClass = ResolveClass(ParentClassPath, AActor::StaticClass());

	FString PackageName = BPPath;
	FString AssetName   = FPackageName::GetLongPackageAssetName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
		ParentClass, Package, *AssetName,
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass());

	if (!NewBP)
		return FMCPToolResult::Error(TEXT("Failed to create Blueprint"));

	FAssetRegistryModule::AssetCreated(NewBP);
	Package->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created Blueprint '%s' with parent '%s'"),
		*BPPath, *ParentClass->GetName()));
}

FMCPToolResult FMCPBlueprintTools::BlueprintAddComponent(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, CompType;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))   return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("component_type"), CompType)) return FMCPToolResult::Error(TEXT("Missing: component_type"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	// Resolve component class
	UClass* CompClass = ResolveClass(CompType);
	if (!CompClass)
	{
		// Try common short names
		FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *CompType);
		CompClass = LoadObject<UClass>(nullptr, *FullPath);
	}
	if (!CompClass || !CompClass->IsChildOf(UActorComponent::StaticClass()))
		return FMCPToolResult::Error(FString::Printf(TEXT("Component class not found: %s"), *CompType));

	FString CompName;
	if (!Args->TryGetStringField(TEXT("component_name"), CompName))
		CompName = CompType; // default name = type name

	if (!BP->SimpleConstructionScript)
		return FMCPToolResult::Error(TEXT("Blueprint has no SimpleConstructionScript"));

	USCS_Node* NewNode = BP->SimpleConstructionScript->CreateNode(CompClass, *CompName);

	USCS_Node* RootNode = BP->SimpleConstructionScript->GetDefaultSceneRootNode();
	if (RootNode)
		RootNode->AddChildNode(NewNode);
	else
		BP->SimpleConstructionScript->AddNode(NewNode);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added '%s' (%s) to '%s'"), *CompName, *CompClass->GetName(), *BPPath));
}

FMCPToolResult FMCPBlueprintTools::BlueprintSetComponentProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, CompName, PropName, Value;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))    return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("component_name"), CompName))  return FMCPToolResult::Error(TEXT("Missing: component_name"));
	if (!Args->TryGetStringField(TEXT("property"),       PropName))  return FMCPToolResult::Error(TEXT("Missing: property"));
	if (!Args->TryGetStringField(TEXT("value"),          Value))     return FMCPToolResult::Error(TEXT("Missing: value"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	// Find SCS node by variable name
	USCS_Node* TargetNode = nullptr;
	if (BP->SimpleConstructionScript)
	{
		for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString().Equals(CompName, ESearchCase::IgnoreCase))
			{
				TargetNode = Node;
				break;
			}
		}
	}

	if (!TargetNode || !TargetNode->ComponentTemplate)
		return FMCPToolResult::Error(FString::Printf(TEXT("Component not found: %s"), *CompName));

	UActorComponent* Template = TargetNode->ComponentTemplate;
	FProperty* Prop = FindFProperty<FProperty>(Template->GetClass(), FName(*PropName));
	if (!Prop)
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Property '%s' not found on %s"), *PropName, *Template->GetClass()->GetName()));

	Template->Modify();
	Prop->ImportText_InContainer(*Value, Template, Template, PPF_None);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Set %s.%s = %s"), *CompName, *PropName, *Value));
}

FMCPToolResult FMCPBlueprintTools::BlueprintAddVariable(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, VarName, TypeStr;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))   return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("name"),           VarName))  return FMCPToolResult::Error(TEXT("Missing: name"));
	if (!Args->TryGetStringField(TEXT("type"),           TypeStr))  return FMCPToolResult::Error(TEXT("Missing: type"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	FEdGraphPinType PinType = MakePinType(TypeStr);
	FBlueprintEditorUtils::AddMemberVariable(BP, FName(*VarName), PinType);

	// Set default value if provided
	FString DefaultValue;
	if (Args->TryGetStringField(TEXT("default_value"), DefaultValue))
	{
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, FName(*VarName), nullptr,
			TEXT("DefaultValue"), DefaultValue);
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added variable '%s' (%s) to '%s'"), *VarName, *TypeStr, *BPPath));
}

FMCPToolResult FMCPBlueprintTools::BlueprintSetVariable(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, VarName, Value;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))   return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("name"),           VarName))  return FMCPToolResult::Error(TEXT("Missing: name"));
	if (!Args->TryGetStringField(TEXT("value"),          Value))    return FMCPToolResult::Error(TEXT("Missing: value"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	if (!BP->GeneratedClass)
		return FMCPToolResult::Error(TEXT("Blueprint has no GeneratedClass — compile first"));

	UObject* CDO = BP->GeneratedClass->GetDefaultObject();
	FProperty* Prop = FindFProperty<FProperty>(BP->GeneratedClass, FName(*VarName));
	if (!Prop)
		return FMCPToolResult::Error(FString::Printf(TEXT("Variable '%s' not found"), *VarName));

	CDO->Modify();
	Prop->ImportText_InContainer(*Value, CDO, CDO, PPF_None);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Set variable '%s' = '%s' on '%s'"), *VarName, *Value, *BPPath));
}

FMCPToolResult FMCPBlueprintTools::BlueprintAddFunction(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, FuncName;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))    return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("function_name"),  FuncName))  return FMCPToolResult::Error(TEXT("Missing: function_name"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, FName(*FuncName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());

	FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, FuncGraph, true, (UClass*)nullptr);

	// Add input/output pins if specified
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : FuncGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode) break;
	}

	const TArray<TSharedPtr<FJsonValue>>* InputsArr;
	if (EntryNode && Args->TryGetArrayField(TEXT("inputs"), InputsArr))
	{
		for (const TSharedPtr<FJsonValue>& V : *InputsArr)
		{
			const TSharedPtr<FJsonObject>* PinObj;
			if (!V->TryGetObject(PinObj)) continue;
			FString PName, PType;
			(*PinObj)->TryGetStringField(TEXT("name"), PName);
			(*PinObj)->TryGetStringField(TEXT("type"), PType);
			if (PName.IsEmpty()) continue;

			TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
			NewPin->PinName = FName(*PName);
			NewPin->PinType = MakePinType(PType);
			NewPin->DesiredPinDirection = EGPD_Output; // inputs appear as outputs on entry node
			EntryNode->UserDefinedPins.Add(NewPin);
		}
		EntryNode->ReconstructNode();
	}

	// Find result node and add outputs
	UK2Node_FunctionResult* ResultNode = nullptr;
	for (UEdGraphNode* Node : FuncGraph->Nodes)
	{
		ResultNode = Cast<UK2Node_FunctionResult>(Node);
		if (ResultNode) break;
	}

	const TArray<TSharedPtr<FJsonValue>>* OutputsArr;
	if (ResultNode && Args->TryGetArrayField(TEXT("outputs"), OutputsArr))
	{
		for (const TSharedPtr<FJsonValue>& V : *OutputsArr)
		{
			const TSharedPtr<FJsonObject>* PinObj;
			if (!V->TryGetObject(PinObj)) continue;
			FString PName, PType;
			(*PinObj)->TryGetStringField(TEXT("name"), PName);
			(*PinObj)->TryGetStringField(TEXT("type"), PType);
			if (PName.IsEmpty()) continue;

			TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
			NewPin->PinName = FName(*PName);
			NewPin->PinType = MakePinType(PType);
			NewPin->DesiredPinDirection = EGPD_Input; // outputs appear as inputs on result node
			ResultNode->UserDefinedPins.Add(NewPin);
		}
		ResultNode->ReconstructNode();
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added function '%s' to '%s'"), *FuncName, *BPPath));
}

FMCPToolResult FMCPBlueprintTools::BlueprintAddEventNode(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, EventName;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))    return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("event_name"),     EventName)) return FMCPToolResult::Error(TEXT("Missing: event_name"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	UEdGraph* EventGraph = GetOrCreateEventGraph(BP);
	if (!EventGraph)
		return FMCPToolResult::Error(TEXT("Could not find or create event graph"));

	// Map friendly names to internal function names
	static const TMap<FString, TPair<FString, UClass*>> EventMap =
	{
		{ TEXT("BeginPlay"), { TEXT("ReceiveBeginPlay"), AActor::StaticClass() } },
		{ TEXT("Tick"),      { TEXT("ReceiveTick"),      AActor::StaticClass() } },
		{ TEXT("EndPlay"),   { TEXT("ReceiveEndPlay"),   AActor::StaticClass() } },
	};

	int32 NodeX = Args->HasField(TEXT("x")) ? (int32)Args->GetNumberField(TEXT("x")) : 0;
	int32 NodeY = Args->HasField(TEXT("y")) ? (int32)Args->GetNumberField(TEXT("y")) : 0;

	UK2Node_Event* EventNode = NewObject<UK2Node_Event>(EventGraph);

	const TPair<FString, UClass*>* Mapped = EventMap.Find(EventName);
	if (Mapped)
	{
		EventNode->EventReference.SetExternalMember(FName(*Mapped->Key), Mapped->Value);
		EventNode->bOverrideFunction = true;
	}
	else
	{
		// Custom event — use as-is
		EventNode->CustomFunctionName = FName(*EventName);
		EventNode->bIsEditable = true;
	}

	EventNode->NodePosX = NodeX;
	EventNode->NodePosY = NodeY;
	EventGraph->AddNode(EventNode, false, false);
	EventNode->AllocateDefaultPins();
	EventNode->ReconstructNode();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added event node '%s' to '%s' at (%d,%d)"),
		*EventName, *BPPath, NodeX, NodeY));
}

FMCPToolResult FMCPBlueprintTools::BlueprintAddFunctionCall(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, FuncName;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))   return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("function"),       FuncName)) return FMCPToolResult::Error(TEXT("Missing: function"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	FString GraphName;
	Args->TryGetStringField(TEXT("graph_name"), GraphName);
	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return FMCPToolResult::Error(TEXT("Graph not found"));

	FString ClassPath;
	Args->TryGetStringField(TEXT("class_path"), ClassPath);

	// Resolve function
	UFunction* Func = nullptr;
	if (!ClassPath.IsEmpty())
	{
		UClass* C = ResolveClass(ClassPath);
		if (C) Func = C->FindFunctionByName(FName(*FuncName));
	}
	if (!Func && BP->GeneratedClass)
	{
		Func = BP->GeneratedClass->FindFunctionByName(FName(*FuncName));
	}
	if (!Func)
	{
		// Try AActor base
		Func = AActor::StaticClass()->FindFunctionByName(FName(*FuncName));
	}

	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
	if (Func)
		CallNode->SetFromFunction(Func);
	else
		CallNode->FunctionReference.SetExternalMember(FName(*FuncName),
			ClassPath.IsEmpty() ? AActor::StaticClass() : ResolveClass(ClassPath, AActor::StaticClass()));

	CallNode->NodePosX = Args->HasField(TEXT("x")) ? (int32)Args->GetNumberField(TEXT("x")) : 200;
	CallNode->NodePosY = Args->HasField(TEXT("y")) ? (int32)Args->GetNumberField(TEXT("y")) : 0;

	Graph->AddNode(CallNode, false, false);
	CallNode->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added function call '%s' to graph '%s' — node: %s"),
		*FuncName, *Graph->GetName(), *CallNode->GetName()));
}

FMCPToolResult FMCPBlueprintTools::BlueprintConnectPins(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, FromNodeName, FromPinName, ToNodeName, ToPinName;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))       return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("from_node"),      FromNodeName)) return FMCPToolResult::Error(TEXT("Missing: from_node"));
	if (!Args->TryGetStringField(TEXT("from_pin"),       FromPinName))  return FMCPToolResult::Error(TEXT("Missing: from_pin"));
	if (!Args->TryGetStringField(TEXT("to_node"),        ToNodeName))   return FMCPToolResult::Error(TEXT("Missing: to_node"));
	if (!Args->TryGetStringField(TEXT("to_pin"),         ToPinName))    return FMCPToolResult::Error(TEXT("Missing: to_pin"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	FString GraphName;
	Args->TryGetStringField(TEXT("graph_name"), GraphName);
	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return FMCPToolResult::Error(TEXT("Graph not found"));

	UEdGraphNode* FromNode = FindNodeByName(Graph, FromNodeName);
	UEdGraphNode* ToNode   = FindNodeByName(Graph, ToNodeName);

	if (!FromNode) return FMCPToolResult::Error(FString::Printf(TEXT("Node not found: %s"), *FromNodeName));
	if (!ToNode)   return FMCPToolResult::Error(FString::Printf(TEXT("Node not found: %s"), *ToNodeName));

	UEdGraphPin* FromPin = FromNode->FindPin(FName(*FromPinName), EGPD_Output);
	if (!FromPin)  FromPin = FromNode->FindPin(FName(*FromPinName)); // fallback: any direction

	UEdGraphPin* ToPin = ToNode->FindPin(FName(*ToPinName), EGPD_Input);
	if (!ToPin)    ToPin = ToNode->FindPin(FName(*ToPinName));

	if (!FromPin) return FMCPToolResult::Error(FString::Printf(TEXT("Pin not found on %s: %s"), *FromNodeName, *FromPinName));
	if (!ToPin)   return FMCPToolResult::Error(FString::Printf(TEXT("Pin not found on %s: %s"), *ToNodeName, *ToPinName));

	FromPin->MakeLinkTo(ToPin);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Connected %s.%s → %s.%s"),
		*FromNodeName, *FromPinName, *ToNodeName, *ToPinName));
}

FMCPToolResult FMCPBlueprintTools::BlueprintCompile(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);

	int32 Errors   = Results.NumErrors;
	int32 Warnings = Results.NumWarnings;

	if (Errors > 0)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Compile failed: %d error(s), %d warning(s)"), Errors, Warnings));
	}

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Compiled '%s' — %d warning(s)"), *BPPath, Warnings));
}

FMCPToolResult FMCPBlueprintTools::BlueprintSpawnInLevel(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP || !BP->GeneratedClass)
		return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found or not compiled: %s"), *BPPath));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return FMCPToolResult::Error(TEXT("No editor world"));

	FVector Location  = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale(1.f);

	const TSharedPtr<FJsonObject>* LocObj, *RotObj, *ScaleObj;
	if (Args->TryGetObjectField(TEXT("location"), LocObj))
	{
		Location = FVector(
			(*LocObj)->HasField(TEXT("x")) ? (*LocObj)->GetNumberField(TEXT("x")) : 0.0,
			(*LocObj)->HasField(TEXT("y")) ? (*LocObj)->GetNumberField(TEXT("y")) : 0.0,
			(*LocObj)->HasField(TEXT("z")) ? (*LocObj)->GetNumberField(TEXT("z")) : 0.0);
	}
	if (Args->TryGetObjectField(TEXT("rotation"), RotObj))
	{
		Rotation = FRotator(
			(*RotObj)->HasField(TEXT("pitch")) ? (*RotObj)->GetNumberField(TEXT("pitch")) : 0.0,
			(*RotObj)->HasField(TEXT("yaw"))   ? (*RotObj)->GetNumberField(TEXT("yaw"))   : 0.0,
			(*RotObj)->HasField(TEXT("roll"))  ? (*RotObj)->GetNumberField(TEXT("roll"))  : 0.0);
	}
	if (Args->TryGetObjectField(TEXT("scale"), ScaleObj))
	{
		Scale = FVector(
			(*ScaleObj)->HasField(TEXT("x")) ? (*ScaleObj)->GetNumberField(TEXT("x")) : 1.0,
			(*ScaleObj)->HasField(TEXT("y")) ? (*ScaleObj)->GetNumberField(TEXT("y")) : 1.0,
			(*ScaleObj)->HasField(TEXT("z")) ? (*ScaleObj)->GetNumberField(TEXT("z")) : 1.0);
	}

	FTransform T(Rotation, Location, Scale);
	AActor* Actor = World->SpawnActor(BP->GeneratedClass, &T, FActorSpawnParameters());
	if (!Actor)
		return FMCPToolResult::Error(TEXT("SpawnActor failed"));

	FString Label;
	if (Args->TryGetStringField(TEXT("name"), Label))
		Actor->SetActorLabel(Label);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Spawned '%s' at (%.1f,%.1f,%.1f)"),
		*Actor->GetActorLabel(), Location.X, Location.Y, Location.Z));
}

FMCPToolResult FMCPBlueprintTools::BlueprintGetGraph(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	FString GraphName;
	Args->TryGetStringField(TEXT("graph_name"), GraphName);
	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return FMCPToolResult::Error(TEXT("Graph not found"));

	TArray<TSharedPtr<FJsonValue>> NodesArr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		auto NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("name"),  Node->GetName());
		NodeObj->SetStringField(TEXT("type"),  Node->GetClass()->GetName());
		NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		NodeObj->SetNumberField(TEXT("x"),     Node->NodePosX);
		NodeObj->SetNumberField(TEXT("y"),     Node->NodePosY);

		TArray<TSharedPtr<FJsonValue>> PinsArr;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;
			auto PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"),      Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("category"),  Pin->PinType.PinCategory.ToString());
			PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
			PinObj->SetStringField(TEXT("default"),   Pin->DefaultValue);

			TArray<TSharedPtr<FJsonValue>> Links;
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Linked->GetOwningNode())
				{
					Links.Add(MakeShared<FJsonValueString>(
						Linked->GetOwningNode()->GetName() + TEXT(".") + Linked->PinName.ToString()));
				}
			}
			PinObj->SetArrayField(TEXT("connections"), Links);
			PinsArr.Add(MakeShared<FJsonValueObject>(PinObj));
		}
		NodeObj->SetArrayField(TEXT("pins"), PinsArr);
		NodesArr.Add(MakeShared<FJsonValueObject>(NodeObj));
	}

	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("graph_name"), Graph->GetName());
	Data->SetArrayField(TEXT("nodes"),       NodesArr);
	Data->SetNumberField(TEXT("node_count"), NodesArr.Num());

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Graph '%s': %d nodes"), *Graph->GetName(), NodesArr.Num()), Data);
}

FMCPToolResult FMCPBlueprintTools::BlueprintFindNode(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	FString GraphName;
	Args->TryGetStringField(TEXT("graph_name"), GraphName);
	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return FMCPToolResult::Error(TEXT("Graph not found"));

	FString NodeType, TitleFilter;
	Args->TryGetStringField(TEXT("node_type"), NodeType);
	Args->TryGetStringField(TEXT("title"),     TitleFilter);

	TArray<TSharedPtr<FJsonValue>> Matches;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		bool bTypeMatch  = NodeType.IsEmpty()  || Node->GetClass()->GetName().Contains(NodeType, ESearchCase::IgnoreCase);
		bool bTitleMatch = TitleFilter.IsEmpty() || Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString()
		                       .Contains(TitleFilter, ESearchCase::IgnoreCase);

		if (bTypeMatch && bTitleMatch)
		{
			auto Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"),  Node->GetName());
			Obj->SetStringField(TEXT("type"),  Node->GetClass()->GetName());
			Obj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			Obj->SetNumberField(TEXT("x"),     Node->NodePosX);
			Obj->SetNumberField(TEXT("y"),     Node->NodePosY);
			Matches.Add(MakeShared<FJsonValueObject>(Obj));
		}
	}

	auto Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("nodes"), Matches);
	Data->SetNumberField(TEXT("count"), Matches.Num());

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Found %d matching node(s)"), Matches.Num()), Data);
}

FMCPToolResult FMCPBlueprintTools::BlueprintSetNodeProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, NodeName, PropName, Value;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))    return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("node_name"),      NodeName))  return FMCPToolResult::Error(TEXT("Missing: node_name"));
	if (!Args->TryGetStringField(TEXT("property"),       PropName))  return FMCPToolResult::Error(TEXT("Missing: property"));
	if (!Args->TryGetStringField(TEXT("value"),          Value))     return FMCPToolResult::Error(TEXT("Missing: value"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP) return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	FString GraphName;
	Args->TryGetStringField(TEXT("graph_name"), GraphName);
	UEdGraph* Graph = FindGraph(BP, GraphName);
	if (!Graph) return FMCPToolResult::Error(TEXT("Graph not found"));

	UEdGraphNode* Node = FindNodeByName(Graph, NodeName);
	if (!Node) return FMCPToolResult::Error(FString::Printf(TEXT("Node not found: %s"), *NodeName));

	FProperty* Prop = FindFProperty<FProperty>(Node->GetClass(), FName(*PropName));
	if (!Prop)
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Property '%s' not found on node type %s"), *PropName, *Node->GetClass()->GetName()));

	Node->Modify();
	Prop->ImportText_InContainer(*Value, Node, Node, PPF_None);
	Node->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Set node '%s'.%s = %s"), *NodeName, *PropName, *Value));
}

FMCPToolResult FMCPBlueprintTools::BlueprintCreateInputAction(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath;
	if (!Args->TryGetStringField(TEXT("path"), AssetPath))
		return FMCPToolResult::Error(TEXT("Missing: path"));

	FString AssetName   = FPackageName::GetLongPackageAssetName(AssetPath);
	UPackage* Package   = CreatePackage(*AssetPath);
	Package->FullyLoad();

	UInputAction* Action = NewObject<UInputAction>(Package, *AssetName, RF_Public | RF_Standalone);

	FString TypeStr;
	Args->TryGetStringField(TEXT("value_type"), TypeStr);
	TypeStr = TypeStr.ToLower();

	if      (TypeStr == TEXT("float"))   Action->ValueType = EInputActionValueType::Axis1D;
	else if (TypeStr == TEXT("vector2")) Action->ValueType = EInputActionValueType::Axis2D;
	else if (TypeStr == TEXT("vector3")) Action->ValueType = EInputActionValueType::Axis3D;
	else                                 Action->ValueType = EInputActionValueType::Boolean;

	FAssetRegistryModule::AssetCreated(Action);
	Package->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created InputAction '%s' (type: %s)"),
		*AssetPath, *UEnum::GetValueAsString(Action->ValueType)));
}

FMCPToolResult FMCPBlueprintTools::BlueprintAddGameMode(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath;
	if (!Args->TryGetStringField(TEXT("path"), BPPath))
		return FMCPToolResult::Error(TEXT("Missing: path"));

	FString AssetName = FPackageName::GetLongPackageAssetName(BPPath);
	UPackage* Package = CreatePackage(*BPPath);
	Package->FullyLoad();

	UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(
		AGameModeBase::StaticClass(), Package, *AssetName,
		BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());

	if (!BP) return FMCPToolResult::Error(TEXT("Failed to create GameMode Blueprint"));

	FKismetEditorUtilities::CompileBlueprint(BP);

	// Set class refs on CDO
	AGameModeBase* CDO = BP->GeneratedClass
		? Cast<AGameModeBase>(BP->GeneratedClass->GetDefaultObject()) : nullptr;

	TArray<FString> SetFields;
	if (CDO)
	{
		FString PawnPath, PCPath, HUDPath;
		if (Args->TryGetStringField(TEXT("default_pawn_class"), PawnPath))
		{
			UClass* C = ResolveClass(PawnPath);
			if (C) { CDO->DefaultPawnClass = C; SetFields.Add(TEXT("pawn")); }
		}
		if (Args->TryGetStringField(TEXT("player_controller_class"), PCPath))
		{
			UClass* C = ResolveClass(PCPath);
			if (C) { CDO->PlayerControllerClass = C; SetFields.Add(TEXT("controller")); }
		}
		if (Args->TryGetStringField(TEXT("hud_class"), HUDPath))
		{
			UClass* C = ResolveClass(HUDPath);
			if (C) { CDO->HUDClass = C; SetFields.Add(TEXT("hud")); }
		}
	}

	FAssetRegistryModule::AssetCreated(BP);
	Package->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created GameMode '%s' — set: %s"),
		*BPPath, SetFields.Num() > 0 ? *FString::Join(SetFields, TEXT(", ")) : TEXT("defaults")));
}

// ---------------------------------------------------------------------------
// blueprint_set_cdo_property
// ---------------------------------------------------------------------------

FMCPToolResult FMCPBlueprintTools::BlueprintSetCDOProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, PropName, Value;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("property"), PropName))
		return FMCPToolResult::Error(TEXT("Missing: property"));
	if (!Args->TryGetStringField(TEXT("value"), Value))
		return FMCPToolResult::Error(TEXT("Missing: value"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP)
		return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	if (!BP->GeneratedClass)
		FKismetEditorUtilities::CompileBlueprint(BP);
	if (!BP->GeneratedClass)
		return FMCPToolResult::Error(TEXT("Blueprint has no generated class — compile failed"));

	UObject* CDO = BP->GeneratedClass->GetDefaultObject(false);
	if (!CDO)
		return FMCPToolResult::Error(TEXT("CDO unavailable — try compiling the blueprint first"));

	FProperty* Prop = FindFProperty<FProperty>(BP->GeneratedClass, *PropName);
	if (!Prop)
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Property '%s' not found on blueprint class '%s'"), *PropName, *BP->GeneratedClass->GetName()));

	void* PropAddr = Prop->ContainerPtrToValuePtr<void>(CDO);
	if (!Prop->ImportText_Direct(*Value, PropAddr, CDO, PPF_None))
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Failed to import value '%s' into property '%s'"), *Value, *PropName));

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	BP->GetOutermost()->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Set CDO property '%s.%s = %s'"), *BP->GetName(), *PropName, *Value));
}

// ---------------------------------------------------------------------------
// blueprint_set_inherited_component_property
// ---------------------------------------------------------------------------

FMCPToolResult FMCPBlueprintTools::BlueprintSetInheritedComponentProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, CompName, PropName, Value;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("component_name"), CompName))
		return FMCPToolResult::Error(TEXT("Missing: component_name"));
	if (!Args->TryGetStringField(TEXT("property"), PropName))
		return FMCPToolResult::Error(TEXT("Missing: property"));
	if (!Args->TryGetStringField(TEXT("value"), Value))
		return FMCPToolResult::Error(TEXT("Missing: value"));

	UBlueprint* BP = LoadBP(BPPath);
	if (!BP)
		return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	if (!BP->GeneratedClass)
		FKismetEditorUtilities::CompileBlueprint(BP);
	if (!BP->GeneratedClass)
		return FMCPToolResult::Error(TEXT("Blueprint has no generated class — compile failed"));

	auto ApplyProperty = [&](UActorComponent* Template) -> FMCPToolResult
	{
		FProperty* Prop = FindFProperty<FProperty>(Template->GetClass(), *PropName);
		if (!Prop)
			return FMCPToolResult::Error(FString::Printf(
				TEXT("Property '%s' not found on component class '%s'"), *PropName, *Template->GetClass()->GetName()));
		void* Addr = Prop->ContainerPtrToValuePtr<void>(Template);
		if (!Prop->ImportText_Direct(*Value, Addr, Template, PPF_None))
			return FMCPToolResult::Error(FString::Printf(
				TEXT("Failed to import value '%s' into '%s'"), *Value, *PropName));
		Template->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		return FMCPToolResult::Success(FString::Printf(
			TEXT("Set '%s.%s = %s'"), *CompName, *PropName, *Value));
	};

	// ── Strategy 1: SCS node from a parent Blueprint class ───────────────────
	{
		USCS_Node* TargetNode = nullptr;
		UClass* SearchClass = BP->ParentClass;
		while (SearchClass && !TargetNode)
		{
			UBlueprintGeneratedClass* ParentBPGC = Cast<UBlueprintGeneratedClass>(SearchClass);
			if (ParentBPGC && ParentBPGC->SimpleConstructionScript)
			{
				for (USCS_Node* Node : ParentBPGC->SimpleConstructionScript->GetAllNodes())
				{
					if (Node && Node->GetVariableName().ToString().Equals(CompName, ESearchCase::IgnoreCase))
					{
						TargetNode = Node;
						break;
					}
				}
			}
			SearchClass = SearchClass->GetSuperClass();
		}

		if (TargetNode)
		{
			UInheritableComponentHandler* ICH = BP->GetInheritableComponentHandler(true);
			if (ICH)
			{
				FComponentKey Key(TargetNode);
				UActorComponent* Template = ICH->GetOverridenComponentTemplate(Key);
				if (!Template)
					Template = ICH->CreateOverridenComponentTemplate(Key);
				if (Template)
					return ApplyProperty(Template);
			}
		}
	}

	// ── Strategy 2: Native component via CDO subobject ────────────────────────
	{
		UObject* CDO = BP->GeneratedClass->GetDefaultObject(false);
		if (CDO)
		{
			TArray<UObject*> SubObjects;
			GetObjectsWithOuter(CDO, SubObjects, false);
			for (UObject* SubObj : SubObjects)
			{
				UActorComponent* Comp = Cast<UActorComponent>(SubObj);
				if (Comp && Comp->GetName().Equals(CompName, ESearchCase::IgnoreCase))
					return ApplyProperty(Comp);
			}
		}
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Component '%s' not found in parent class hierarchy. For components in this Blueprint's own SCS, use blueprint_set_component_property instead."),
		*CompName));
}
