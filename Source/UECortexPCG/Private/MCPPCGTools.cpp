#include "MCPPCGTools.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGEdge.h"
#include "PCGComponent.h"
#include "PCGVolume.h"
#include "PCGSubgraph.h"
#include "PCGSettings.h"
#include "UECortexModule.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Dom/JsonObject.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

UPCGGraph* FMCPPCGTools::LoadPCGGraph(const FString& Path)
{
	return LoadObject<UPCGGraph>(nullptr, *Path);
}

UPCGNode* FMCPPCGTools::FindNodeByName(UPCGGraph* Graph, const FString& Name)
{
	if (!Graph) return nullptr;
	for (UPCGNode* Node : Graph->GetNodes())
	{
		if (Node && Node->GetName().Equals(Name, ESearchCase::IgnoreCase))
			return Node;
	}
	if (Graph->GetInputNode() && Graph->GetInputNode()->GetName().Equals(Name, ESearchCase::IgnoreCase))
		return Graph->GetInputNode();
	if (Graph->GetOutputNode() && Graph->GetOutputNode()->GetName().Equals(Name, ESearchCase::IgnoreCase))
		return Graph->GetOutputNode();
	return nullptr;
}

static UWorld* GetEditorWorldPCG()
{
	return FUECortexModule::GetActiveWorld();
}

static AActor* FindActorPCG(UWorld* World, const FString& Label)
{
	if (!World) return nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == Label) return *It;
	}
	return nullptr;
}

static TSharedPtr<FJsonObject> NodeToJson(UPCGNode* Node)
{
	auto Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), Node->GetName());

	int32 X, Y;
	Node->GetNodePosition(X, Y);
	Obj->SetNumberField(TEXT("x"), X);
	Obj->SetNumberField(TEXT("y"), Y);

	UPCGSettings* Settings = Node->GetSettings();
	Obj->SetStringField(TEXT("settings_class"),
		Settings ? Settings->GetClass()->GetName() : TEXT("None"));

	TArray<TSharedPtr<FJsonValue>> InPins;
	for (const TObjectPtr<UPCGPin>& Pin : Node->GetInputPins())
	{
		if (!Pin) continue;
		auto P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("label"), Pin->Properties.Label.ToString());
		P->SetNumberField(TEXT("edges"), Pin->Edges.Num());
		InPins.Add(MakeShared<FJsonValueObject>(P));
	}
	Obj->SetArrayField(TEXT("input_pins"), InPins);

	TArray<TSharedPtr<FJsonValue>> OutPins;
	for (const TObjectPtr<UPCGPin>& Pin : Node->GetOutputPins())
	{
		if (!Pin) continue;
		auto P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("label"), Pin->Properties.Label.ToString());
		P->SetNumberField(TEXT("edges"), Pin->Edges.Num());

		TArray<TSharedPtr<FJsonValue>> Connections;
		for (UPCGEdge* Edge : Pin->Edges)
		{
			if (Edge && Edge->InputPin && Edge->InputPin->Node)
			{
				FString ConnStr = Edge->InputPin->Node->GetName()
					+ TEXT(".") + Edge->InputPin->Properties.Label.ToString();
				Connections.Add(MakeShared<FJsonValueString>(ConnStr));
			}
		}
		P->SetArrayField(TEXT("connections"), Connections);
		OutPins.Add(MakeShared<FJsonValueObject>(P));
	}
	Obj->SetArrayField(TEXT("output_pins"), OutPins);

	return Obj;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMCPPCGTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	// pcg_create_graph
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_create_graph");
		Def.Description = TEXT("Create a new PCG Graph asset at the given content path.");
		Def.Params = {
			{ TEXT("path"), TEXT("string"), TEXT("Content path, e.g. '/Game/PCG/PCG_MyGraph'") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGCreateGraph(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_add_node
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_add_node");
		Def.Description = TEXT("Add a node to a PCG graph by its Settings class name.");
		Def.Params = {
			{ TEXT("graph_path"),     TEXT("string"), TEXT("PCG Graph asset path") },
			{ TEXT("settings_class"), TEXT("string"), TEXT("Settings class, e.g. 'PCGSurfaceSamplerSettings'") },
			{ TEXT("x"),              TEXT("number"), TEXT("Node X position in graph"), false },
			{ TEXT("y"),              TEXT("number"), TEXT("Node Y position in graph"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGAddNode(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_connect_pins
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_connect_pins");
		Def.Description = TEXT("Connect an output pin of one PCG node to an input pin of another.");
		Def.Params = {
			{ TEXT("graph_path"),  TEXT("string"), TEXT("PCG Graph asset path") },
			{ TEXT("from_node"),   TEXT("string"), TEXT("Source node name (use 'InputNode'/'OutputNode' for graph I/O)") },
			{ TEXT("from_pin"),    TEXT("string"), TEXT("Source pin label (default: 'Out')"), false },
			{ TEXT("to_node"),     TEXT("string"), TEXT("Target node name") },
			{ TEXT("to_pin"),      TEXT("string"), TEXT("Target pin label (default: 'In')"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGConnectPins(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_disconnect_pins
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_disconnect_pins");
		Def.Description = TEXT("Remove the edge between two PCG node pins.");
		Def.Params = {
			{ TEXT("graph_path"), TEXT("string"), TEXT("PCG Graph asset path") },
			{ TEXT("from_node"),  TEXT("string"), TEXT("Source node name") },
			{ TEXT("from_pin"),   TEXT("string"), TEXT("Source pin label") },
			{ TEXT("to_node"),    TEXT("string"), TEXT("Target node name") },
			{ TEXT("to_pin"),     TEXT("string"), TEXT("Target pin label") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGDisconnectPins(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_set_node_param
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_set_node_param");
		Def.Description = TEXT("Set a parameter on a PCG node's Settings object by property name.");
		Def.Params = {
			{ TEXT("graph_path"), TEXT("string"), TEXT("PCG Graph asset path") },
			{ TEXT("node_name"),  TEXT("string"), TEXT("Node name") },
			{ TEXT("param"),      TEXT("string"), TEXT("Property name on the Settings object") },
			{ TEXT("value"),      TEXT("string"), TEXT("Value as string") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGSetNodeParam(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_get_graph
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_get_graph");
		Def.Description = TEXT("Return the full PCG graph structure as JSON (nodes, pins, connections).");
		Def.Params = {
			{ TEXT("graph_path"), TEXT("string"), TEXT("PCG Graph asset path") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGGetGraph(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_create_subgraph
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_create_subgraph");
		Def.Description = TEXT("Create a PCG subgraph asset and add a subgraph node referencing it into a parent graph.");
		Def.Params = {
			{ TEXT("path"),        TEXT("string"), TEXT("Content path for the new subgraph PCGGraph asset") },
			{ TEXT("parent_graph"), TEXT("string"), TEXT("Parent graph to add the subgraph node into"), false },
			{ TEXT("x"),           TEXT("number"), TEXT("Node X in parent graph"), false },
			{ TEXT("y"),           TEXT("number"), TEXT("Node Y in parent graph"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGCreateSubgraph(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_place_volume
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_place_volume");
		Def.Description = TEXT("Place a PCGVolume actor in the level and link it to a PCG Graph asset.");
		Def.Params = {
			{ TEXT("graph_path"), TEXT("string"), TEXT("PCG Graph asset path to link") },
			{ TEXT("name"),       TEXT("string"), TEXT("Actor label for the volume") },
			{ TEXT("location"),   TEXT("object"), TEXT("{x,y,z} world location"), false },
			{ TEXT("extent"),     TEXT("object"), TEXT("{x,y,z} half-extents in cm"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGPlaceVolume(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_place_seed_actor
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_place_seed_actor");
		Def.Description = TEXT("Spawn a Blueprint actor at a location and optionally add a PCG actor tag.");
		Def.Params = {
			{ TEXT("blueprint_path"), TEXT("string"), TEXT("Blueprint asset path (e.g. '/Game/BP_TendrilSeed')") },
			{ TEXT("name"),           TEXT("string"), TEXT("Actor label") },
			{ TEXT("location"),       TEXT("object"), TEXT("{x,y,z} world location"), false },
			{ TEXT("tag"),            TEXT("string"), TEXT("PCG actor tag to add (e.g. 'TendrilSeed')"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGPlaceSeedActor(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_execute_graph
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_execute_graph");
		Def.Description = TEXT("Trigger PCG graph generation on an actor's PCGComponent in the editor.");
		Def.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Name of the actor with a PCGComponent (e.g. a PCGVolume)") },
			{ TEXT("force"),      TEXT("boolean"), TEXT("Force regeneration even if up to date"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGExecuteGraph(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_list_node_types
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_list_node_types");
		Def.Description = TEXT("List all available UPCGSettings subclasses (node types) for discovery.");
		Def.Params = {
			{ TEXT("filter"), TEXT("string"), TEXT("Optional substring filter on class name"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGListNodeTypes(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_get_node_params
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_get_node_params");
		Def.Description = TEXT("Return all editable properties on a PCG Settings class.");
		Def.Params = {
			{ TEXT("settings_class"), TEXT("string"), TEXT("Settings class name, e.g. 'PCGSurfaceSamplerSettings'") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGGetNodeParams(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_set_graph_param
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_set_graph_param");
		Def.Description = TEXT("Set a UPROPERTY on a PCG Graph asset directly.");
		Def.Params = {
			{ TEXT("graph_path"), TEXT("string"), TEXT("PCG Graph asset path") },
			{ TEXT("param"),      TEXT("string"), TEXT("Property name on UPCGGraph") },
			{ TEXT("value"),      TEXT("string"), TEXT("Value as string") },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGSetGraphParam(A); };
		OutTools.Add(MoveTemp(Def));
	}

	// pcg_create_attribute_node
	{
		FMCPToolDef Def;
		Def.Name = TEXT("pcg_create_attribute_node");
		Def.Description = TEXT("Add a metadata attribute operation node (get/set/transform) to a PCG graph.");
		Def.Params = {
			{ TEXT("graph_path"),     TEXT("string"), TEXT("PCG Graph asset path") },
			{ TEXT("operation"),      TEXT("string"), TEXT("Operation: 'cast' | 'filter' | 'noise' | 'remap'") },
			{ TEXT("x"),              TEXT("number"), TEXT("Node X position"), false },
			{ TEXT("y"),              TEXT("number"), TEXT("Node Y position"), false },
		};
		Def.Handler = [](const TSharedPtr<FJsonObject>& A) { return PCGCreateAttributeNode(A); };
		OutTools.Add(MoveTemp(Def));
	}
}

// ---------------------------------------------------------------------------
// Implementations
// ---------------------------------------------------------------------------

FMCPToolResult FMCPPCGTools::PCGCreateGraph(const TSharedPtr<FJsonObject>& Args)
{
	FString Path;
	if (!Args->TryGetStringField(TEXT("path"), Path))
		return FMCPToolResult::Error(TEXT("Missing: path"));

	FString AssetName = FPackageName::GetLongPackageAssetName(Path);
	UPackage* Package = CreatePackage(*Path);
	Package->FullyLoad();

	UPCGGraph* Graph = NewObject<UPCGGraph>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!Graph) return FMCPToolResult::Error(TEXT("Failed to create PCGGraph"));

	FAssetRegistryModule::AssetCreated(Graph);
	Package->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created PCG Graph '%s'"), *Path));
}

FMCPToolResult FMCPPCGTools::PCGAddNode(const TSharedPtr<FJsonObject>& Args)
{
	FString GraphPath, SettingsClassName;
	if (!Args->TryGetStringField(TEXT("graph_path"),     GraphPath))        return FMCPToolResult::Error(TEXT("Missing: graph_path"));
	if (!Args->TryGetStringField(TEXT("settings_class"), SettingsClassName)) return FMCPToolResult::Error(TEXT("Missing: settings_class"));

	UPCGGraph* Graph = LoadPCGGraph(GraphPath);
	if (!Graph) return FMCPToolResult::Error(FString::Printf(TEXT("PCG Graph not found: %s"), *GraphPath));

	UClass* SettingsClass = LoadObject<UClass>(nullptr,
		*FString::Printf(TEXT("/Script/PCG.%s"), *SettingsClassName));
	if (!SettingsClass)
		SettingsClass = FindObject<UClass>(nullptr, *SettingsClassName);
	if (!SettingsClass || !SettingsClass->IsChildOf(UPCGSettings::StaticClass()))
		return FMCPToolResult::Error(FString::Printf(TEXT("PCGSettings class not found: %s"), *SettingsClassName));

	UPCGSettings* DefaultSettings = nullptr;
	UPCGNode* NewNode = Graph->AddNodeOfType(SettingsClass, DefaultSettings);
	if (!NewNode) return FMCPToolResult::Error(TEXT("Failed to add node"));

	int32 X = Args->HasField(TEXT("x")) ? (int32)Args->GetNumberField(TEXT("x")) : 0;
	int32 Y = Args->HasField(TEXT("y")) ? (int32)Args->GetNumberField(TEXT("y")) : 0;
	NewNode->SetNodePosition(X, Y);

	Graph->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added node '%s' (%s) at (%d,%d)"),
		*NewNode->GetName(), *SettingsClassName, X, Y));
}

FMCPToolResult FMCPPCGTools::PCGConnectPins(const TSharedPtr<FJsonObject>& Args)
{
	FString GraphPath, FromNodeName, ToNodeName;
	if (!Args->TryGetStringField(TEXT("graph_path"), GraphPath))    return FMCPToolResult::Error(TEXT("Missing: graph_path"));
	if (!Args->TryGetStringField(TEXT("from_node"),  FromNodeName)) return FMCPToolResult::Error(TEXT("Missing: from_node"));
	if (!Args->TryGetStringField(TEXT("to_node"),    ToNodeName))   return FMCPToolResult::Error(TEXT("Missing: to_node"));

	FString FromPinLabel = TEXT("Out");
	FString ToPinLabel   = TEXT("In");
	Args->TryGetStringField(TEXT("from_pin"), FromPinLabel);
	Args->TryGetStringField(TEXT("to_pin"),   ToPinLabel);

	UPCGGraph* Graph = LoadPCGGraph(GraphPath);
	if (!Graph) return FMCPToolResult::Error(FString::Printf(TEXT("PCG Graph not found: %s"), *GraphPath));

	auto ResolveNode = [&](const FString& Name) -> UPCGNode*
	{
		if (Name.Equals(TEXT("InputNode"),  ESearchCase::IgnoreCase)) return Graph->GetInputNode();
		if (Name.Equals(TEXT("OutputNode"), ESearchCase::IgnoreCase)) return Graph->GetOutputNode();
		return FindNodeByName(Graph, Name);
	};

	UPCGNode* FromNode = ResolveNode(FromNodeName);
	UPCGNode* ToNode   = ResolveNode(ToNodeName);

	if (!FromNode) return FMCPToolResult::Error(FString::Printf(TEXT("Node not found: %s"), *FromNodeName));
	if (!ToNode)   return FMCPToolResult::Error(FString::Printf(TEXT("Node not found: %s"), *ToNodeName));

	UPCGNode* Added = Graph->AddEdge(FromNode, FName(*FromPinLabel), ToNode, FName(*ToPinLabel));
	if (!Added)
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Failed to connect %s.%s → %s.%s"), *FromNodeName, *FromPinLabel, *ToNodeName, *ToPinLabel));

	Graph->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Connected %s.%s → %s.%s"), *FromNodeName, *FromPinLabel, *ToNodeName, *ToPinLabel));
}

FMCPToolResult FMCPPCGTools::PCGDisconnectPins(const TSharedPtr<FJsonObject>& Args)
{
	FString GraphPath, FromNodeName, ToNodeName;
	if (!Args->TryGetStringField(TEXT("graph_path"), GraphPath))    return FMCPToolResult::Error(TEXT("Missing: graph_path"));
	if (!Args->TryGetStringField(TEXT("from_node"),  FromNodeName)) return FMCPToolResult::Error(TEXT("Missing: from_node"));
	if (!Args->TryGetStringField(TEXT("to_node"),    ToNodeName))   return FMCPToolResult::Error(TEXT("Missing: to_node"));

	FString FromPinLabel = TEXT("Out");
	FString ToPinLabel   = TEXT("In");
	Args->TryGetStringField(TEXT("from_pin"), FromPinLabel);
	Args->TryGetStringField(TEXT("to_pin"),   ToPinLabel);

	UPCGGraph* Graph = LoadPCGGraph(GraphPath);
	if (!Graph) return FMCPToolResult::Error(FString::Printf(TEXT("PCG Graph not found: %s"), *GraphPath));

	auto ResolveNode = [&](const FString& Name) -> UPCGNode*
	{
		if (Name.Equals(TEXT("InputNode"),  ESearchCase::IgnoreCase)) return Graph->GetInputNode();
		if (Name.Equals(TEXT("OutputNode"), ESearchCase::IgnoreCase)) return Graph->GetOutputNode();
		return FindNodeByName(Graph, Name);
	};

	UPCGNode* FromNode = ResolveNode(FromNodeName);
	UPCGNode* ToNode   = ResolveNode(ToNodeName);

	if (!FromNode) return FMCPToolResult::Error(FString::Printf(TEXT("Node not found: %s"), *FromNodeName));
	if (!ToNode)   return FMCPToolResult::Error(FString::Printf(TEXT("Node not found: %s"), *ToNodeName));

	bool bRemoved = Graph->RemoveEdge(FromNode, FName(*FromPinLabel), ToNode, FName(*ToPinLabel));
	if (!bRemoved)
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Edge not found: %s.%s → %s.%s"), *FromNodeName, *FromPinLabel, *ToNodeName, *ToPinLabel));

	Graph->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Removed edge %s.%s → %s.%s"), *FromNodeName, *FromPinLabel, *ToNodeName, *ToPinLabel));
}

FMCPToolResult FMCPPCGTools::PCGSetNodeParam(const TSharedPtr<FJsonObject>& Args)
{
	FString GraphPath, NodeName, Param, Value;
	if (!Args->TryGetStringField(TEXT("graph_path"), GraphPath))  return FMCPToolResult::Error(TEXT("Missing: graph_path"));
	if (!Args->TryGetStringField(TEXT("node_name"),  NodeName))   return FMCPToolResult::Error(TEXT("Missing: node_name"));
	if (!Args->TryGetStringField(TEXT("param"),      Param))      return FMCPToolResult::Error(TEXT("Missing: param"));
	if (!Args->TryGetStringField(TEXT("value"),      Value))      return FMCPToolResult::Error(TEXT("Missing: value"));

	UPCGGraph* Graph = LoadPCGGraph(GraphPath);
	if (!Graph) return FMCPToolResult::Error(FString::Printf(TEXT("PCG Graph not found: %s"), *GraphPath));

	UPCGNode* Node = FindNodeByName(Graph, NodeName);
	if (!Node) return FMCPToolResult::Error(FString::Printf(TEXT("Node not found: %s"), *NodeName));

	UPCGSettings* Settings = Node->GetSettings();
	if (!Settings)
		return FMCPToolResult::Error(FString::Printf(TEXT("Node '%s' has no settings"), *NodeName));

	FProperty* Prop = FindFProperty<FProperty>(Settings->GetClass(), FName(*Param));
	if (!Prop)
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Property '%s' not found on %s"), *Param, *Settings->GetClass()->GetName()));

#if UE_MCP_VERSION_5_7_PLUS
	return FMCPToolResult::Error(TEXT("pcg_set_node_param GPU variant requires 5.7+. Use param name directly for CPU params."));
#endif

	Settings->Modify();
	Prop->ImportText_InContainer(*Value, Settings, Settings, PPF_None);
	Graph->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Set %s.%s.%s = %s"), *NodeName, *Settings->GetClass()->GetName(), *Param, *Value));
}

FMCPToolResult FMCPPCGTools::PCGGetGraph(const TSharedPtr<FJsonObject>& Args)
{
	FString GraphPath;
	if (!Args->TryGetStringField(TEXT("graph_path"), GraphPath))
		return FMCPToolResult::Error(TEXT("Missing: graph_path"));

	UPCGGraph* Graph = LoadPCGGraph(GraphPath);
	if (!Graph) return FMCPToolResult::Error(FString::Printf(TEXT("PCG Graph not found: %s"), *GraphPath));

	TArray<TSharedPtr<FJsonValue>> NodesArr;

	if (Graph->GetInputNode())
		NodesArr.Add(MakeShared<FJsonValueObject>(NodeToJson(Graph->GetInputNode())));
	if (Graph->GetOutputNode())
		NodesArr.Add(MakeShared<FJsonValueObject>(NodeToJson(Graph->GetOutputNode())));

	for (UPCGNode* Node : Graph->GetNodes())
	{
		if (!Node) continue;
		NodesArr.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
	}

	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("graph_path"), GraphPath);
	Data->SetArrayField(TEXT("nodes"),       NodesArr);
	Data->SetNumberField(TEXT("node_count"), NodesArr.Num());

	return FMCPToolResult::Success(FString::Printf(
		TEXT("PCG Graph '%s': %d nodes"), *GraphPath, NodesArr.Num()), Data);
}

FMCPToolResult FMCPPCGTools::PCGCreateSubgraph(const TSharedPtr<FJsonObject>& Args)
{
	FString Path;
	if (!Args->TryGetStringField(TEXT("path"), Path))
		return FMCPToolResult::Error(TEXT("Missing: path"));

	FString AssetName = FPackageName::GetLongPackageAssetName(Path);
	UPackage* Package = CreatePackage(*Path);
	Package->FullyLoad();

	UPCGGraph* SubGraph = NewObject<UPCGGraph>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!SubGraph) return FMCPToolResult::Error(TEXT("Failed to create subgraph"));

	FAssetRegistryModule::AssetCreated(SubGraph);
	Package->MarkPackageDirty();

	FString ParentGraphPath;
	if (Args->TryGetStringField(TEXT("parent_graph"), ParentGraphPath))
	{
		UPCGGraph* ParentGraph = LoadPCGGraph(ParentGraphPath);
		if (!ParentGraph)
			return FMCPToolResult::Error(FString::Printf(TEXT("Parent graph not found: %s"), *ParentGraphPath));

		UPCGSettings* DefaultSettings = nullptr;
		UPCGNode* SubgraphNode = ParentGraph->AddNodeOfType(
			UPCGSubgraphSettings::StaticClass(), DefaultSettings);

		if (SubgraphNode)
		{
			UPCGSubgraphSettings* SubSettings = Cast<UPCGSubgraphSettings>(SubgraphNode->GetSettings());
			if (SubSettings)
				SubSettings->SetSubgraph(SubGraph);

			int32 X = Args->HasField(TEXT("x")) ? (int32)Args->GetNumberField(TEXT("x")) : 0;
			int32 Y = Args->HasField(TEXT("y")) ? (int32)Args->GetNumberField(TEXT("y")) : 0;
			SubgraphNode->SetNodePosition(X, Y);
		}

		ParentGraph->MarkPackageDirty();

		return FMCPToolResult::Success(FString::Printf(
			TEXT("Created subgraph '%s' and added node '%s' to parent graph"),
			*Path, SubgraphNode ? *SubgraphNode->GetName() : TEXT("?")));
	}

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Created subgraph '%s'"), *Path));
}

FMCPToolResult FMCPPCGTools::PCGPlaceVolume(const TSharedPtr<FJsonObject>& Args)
{
	FString GraphPath, ActorName;
	if (!Args->TryGetStringField(TEXT("graph_path"), GraphPath))  return FMCPToolResult::Error(TEXT("Missing: graph_path"));
	if (!Args->TryGetStringField(TEXT("name"),       ActorName))  return FMCPToolResult::Error(TEXT("Missing: name"));

	UPCGGraph* Graph = LoadPCGGraph(GraphPath);
	if (!Graph) return FMCPToolResult::Error(FString::Printf(TEXT("PCG Graph not found: %s"), *GraphPath));

	UWorld* World = GetEditorWorldPCG();
	if (!World) return FMCPToolResult::Error(TEXT("No editor world"));

	FVector Location = FVector::ZeroVector;
	FVector Extent(1000.f, 1000.f, 500.f);

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
			(*ExtObj)->HasField(TEXT("x")) ? (*ExtObj)->GetNumberField(TEXT("x")) : 1000.0,
			(*ExtObj)->HasField(TEXT("y")) ? (*ExtObj)->GetNumberField(TEXT("y")) : 1000.0,
			(*ExtObj)->HasField(TEXT("z")) ? (*ExtObj)->GetNumberField(TEXT("z")) : 500.0);
	}

	FActorSpawnParameters Params;
	APCGVolume* Volume = World->SpawnActor<APCGVolume>(APCGVolume::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (!Volume) return FMCPToolResult::Error(TEXT("Failed to spawn PCGVolume"));

	Volume->SetActorLabel(ActorName);
	Volume->SetActorScale3D(Extent / 100.f);

	UPCGComponent* PCGComp = Volume->FindComponentByClass<UPCGComponent>();
	if (PCGComp)
		PCGComp->SetGraph(Graph);

	Volume->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Placed PCGVolume '%s' at (%.1f,%.1f,%.1f) linked to '%s'"),
		*ActorName, Location.X, Location.Y, Location.Z, *GraphPath));
}

FMCPToolResult FMCPPCGTools::PCGPlaceSeedActor(const TSharedPtr<FJsonObject>& Args)
{
	FString BPPath, ActorName;
	if (!Args->TryGetStringField(TEXT("blueprint_path"), BPPath))    return FMCPToolResult::Error(TEXT("Missing: blueprint_path"));
	if (!Args->TryGetStringField(TEXT("name"),           ActorName)) return FMCPToolResult::Error(TEXT("Missing: name"));

	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BPPath);
	if (!BP || !BP->GeneratedClass)
		return FMCPToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	UWorld* World = GetEditorWorldPCG();
	if (!World) return FMCPToolResult::Error(TEXT("No editor world"));

	FVector Location = FVector::ZeroVector;
	const TSharedPtr<FJsonObject>* LocObj;
	if (Args->TryGetObjectField(TEXT("location"), LocObj))
	{
		Location = FVector(
			(*LocObj)->HasField(TEXT("x")) ? (*LocObj)->GetNumberField(TEXT("x")) : 0.0,
			(*LocObj)->HasField(TEXT("y")) ? (*LocObj)->GetNumberField(TEXT("y")) : 0.0,
			(*LocObj)->HasField(TEXT("z")) ? (*LocObj)->GetNumberField(TEXT("z")) : 0.0);
	}

	FTransform T(FRotator::ZeroRotator, Location);
	AActor* Actor = World->SpawnActor(BP->GeneratedClass, &T, FActorSpawnParameters());
	if (!Actor) return FMCPToolResult::Error(TEXT("SpawnActor failed"));

	Actor->SetActorLabel(ActorName);

	FString Tag;
	if (Args->TryGetStringField(TEXT("tag"), Tag) && !Tag.IsEmpty())
		Actor->Tags.AddUnique(FName(*Tag));

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Placed seed actor '%s' at (%.1f,%.1f,%.1f)%s"),
		*ActorName, Location.X, Location.Y, Location.Z,
		Tag.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" [tag: %s]"), *Tag)));
}

FMCPToolResult FMCPPCGTools::PCGExecuteGraph(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("Missing: actor_name"));

	bool bForce = false;
	Args->TryGetBoolField(TEXT("force"), bForce);

	UWorld* World = GetEditorWorldPCG();
	AActor* Actor = FindActorPCG(World, ActorName);
	if (!Actor) return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));

	UPCGComponent* PCGComp = Actor->FindComponentByClass<UPCGComponent>();
	if (!PCGComp)
		return FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no PCGComponent"), *ActorName));

	PCGComp->GenerateLocal(bForce);

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Triggered PCG generation on '%s' (force=%s)"),
		*ActorName, bForce ? TEXT("true") : TEXT("false")));
}

FMCPToolResult FMCPPCGTools::PCGListNodeTypes(const TSharedPtr<FJsonObject>& Args)
{
	FString Filter;
	Args->TryGetStringField(TEXT("filter"), Filter);

	TArray<TSharedPtr<FJsonValue>> Types;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UPCGSettings::StaticClass())) continue;
		if (Class->HasAnyClassFlags(CLASS_Abstract))         continue;

		const FString ClassName = Class->GetName();
		if (!Filter.IsEmpty() && !ClassName.Contains(Filter, ESearchCase::IgnoreCase)) continue;

		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("class"), ClassName);
		Obj->SetStringField(TEXT("path"),  Class->GetPathName());
		Types.Add(MakeShared<FJsonValueObject>(Obj));
	}

	Types.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		FString NameA, NameB;
		A->AsObject()->TryGetStringField(TEXT("class"), NameA);
		B->AsObject()->TryGetStringField(TEXT("class"), NameB);
		return NameA < NameB;
	});

	auto Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("types"), Types);
	Data->SetNumberField(TEXT("count"), Types.Num());

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Found %d PCG node types"), Types.Num()), Data);
}

FMCPToolResult FMCPPCGTools::PCGGetNodeParams(const TSharedPtr<FJsonObject>& Args)
{
	FString ClassName;
	if (!Args->TryGetStringField(TEXT("settings_class"), ClassName))
		return FMCPToolResult::Error(TEXT("Missing: settings_class"));

	UClass* SettingsClass = LoadObject<UClass>(nullptr,
		*FString::Printf(TEXT("/Script/PCG.%s"), *ClassName));
	if (!SettingsClass)
		SettingsClass = FindObject<UClass>(nullptr, *ClassName);
	if (!SettingsClass || !SettingsClass->IsChildOf(UPCGSettings::StaticClass()))
		return FMCPToolResult::Error(FString::Printf(TEXT("PCGSettings class not found: %s"), *ClassName));

	TArray<TSharedPtr<FJsonValue>> Params;
	for (TFieldIterator<FProperty> It(SettingsClass); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;

		auto P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("name"),    Prop->GetName());
		P->SetStringField(TEXT("type"),    Prop->GetClass()->GetName());
		P->SetStringField(TEXT("tooltip"), Prop->GetToolTipText().ToString());
		Params.Add(MakeShared<FJsonValueObject>(P));
	}

	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("class"),  ClassName);
	Data->SetArrayField(TEXT("params"),  Params);
	Data->SetNumberField(TEXT("count"),  Params.Num());

	return FMCPToolResult::Success(FString::Printf(
		TEXT("%s has %d editable params"), *ClassName, Params.Num()), Data);
}

FMCPToolResult FMCPPCGTools::PCGSetGraphParam(const TSharedPtr<FJsonObject>& Args)
{
	FString GraphPath, Param, Value;
	if (!Args->TryGetStringField(TEXT("graph_path"), GraphPath)) return FMCPToolResult::Error(TEXT("Missing: graph_path"));
	if (!Args->TryGetStringField(TEXT("param"),      Param))     return FMCPToolResult::Error(TEXT("Missing: param"));
	if (!Args->TryGetStringField(TEXT("value"),      Value))     return FMCPToolResult::Error(TEXT("Missing: value"));

	UPCGGraph* Graph = LoadPCGGraph(GraphPath);
	if (!Graph) return FMCPToolResult::Error(FString::Printf(TEXT("PCG Graph not found: %s"), *GraphPath));

	FProperty* Prop = FindFProperty<FProperty>(Graph->GetClass(), FName(*Param));
	if (!Prop)
		return FMCPToolResult::Error(FString::Printf(TEXT("Property '%s' not found on UPCGGraph"), *Param));

	Graph->Modify();
	Prop->ImportText_InContainer(*Value, Graph, Graph, PPF_None);
	Graph->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Set PCG Graph param %s = %s"), *Param, *Value));
}

FMCPToolResult FMCPPCGTools::PCGCreateAttributeNode(const TSharedPtr<FJsonObject>& Args)
{
	FString GraphPath, Operation;
	if (!Args->TryGetStringField(TEXT("graph_path"), GraphPath))  return FMCPToolResult::Error(TEXT("Missing: graph_path"));
	if (!Args->TryGetStringField(TEXT("operation"),  Operation))  return FMCPToolResult::Error(TEXT("Missing: operation"));

	UPCGGraph* Graph = LoadPCGGraph(GraphPath);
	if (!Graph) return FMCPToolResult::Error(FString::Printf(TEXT("PCG Graph not found: %s"), *GraphPath));

	static const TMap<FString, FString> OpToClass =
	{
		{ TEXT("cast"),   TEXT("PCGAttributeCastSettings") },
		{ TEXT("filter"), TEXT("PCGAttributeFilterSettings") },
		{ TEXT("noise"),  TEXT("PCGAttributeNoiseSettings") },
		{ TEXT("remap"),  TEXT("PCGAttributeRemapSettings") },
	};

	const FString* ClassName = OpToClass.Find(Operation.ToLower());
	if (!ClassName)
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Unknown operation '%s'. Use: cast, filter, noise, remap"), *Operation));

	UClass* SettingsClass = LoadObject<UClass>(nullptr,
		*FString::Printf(TEXT("/Script/PCG.%s"), **ClassName));
	if (!SettingsClass)
		return FMCPToolResult::Error(FString::Printf(TEXT("Attribute settings class not found: %s"), **ClassName));

	UPCGSettings* DefaultSettings = nullptr;
	UPCGNode* NewNode = Graph->AddNodeOfType(SettingsClass, DefaultSettings);
	if (!NewNode) return FMCPToolResult::Error(TEXT("Failed to add attribute node"));

	int32 X = Args->HasField(TEXT("x")) ? (int32)Args->GetNumberField(TEXT("x")) : 0;
	int32 Y = Args->HasField(TEXT("y")) ? (int32)Args->GetNumberField(TEXT("y")) : 0;
	NewNode->SetNodePosition(X, Y);

	Graph->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added attribute node '%s' (%s) at (%d,%d)"),
		*NewNode->GetName(), **ClassName, X, Y));
}
