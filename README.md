# UECortex

Native Unreal Engine 5 plugin that runs a full [MCP (Model Context Protocol)](https://modelcontextprotocol.io) server inside the Unreal Editor. Control your project directly from Claude Code, VS Code, or any MCP client — no Python sidecar, no Node.js process, no external dependencies.

**167 built-in tools** across 17 categories, plus a Python-backed dynamic tool layer.

---

## Quick Start

### 1. Install

Copy the `UECortex` folder into your project's `Plugins/` directory and enable it in your `.uproject`:

```json
{
  "Name": "UECortex",
  "Enabled": true
}
```

Reopen the project. The server starts automatically on editor launch.

### 2. Connect

**Claude Code:**
```bash
claude mcp add uecortex --transport http http://localhost:7777/mcp
```

**VS Code** (`.vscode/mcp.json`):
```json
{ "servers": { "uecortex": { "url": "http://localhost:7777/mcp" } } }
```

**Claude Desktop** (`claude_desktop_config.json`):
```json
{ "mcpServers": { "uecortex": { "url": "http://localhost:7777/mcp" } } }
```

**Health check:**
```
GET http://localhost:7777/health
```

---

## Tool Categories

### Meta
Discover what's available before diving into the full tools list.

| Tool | What it does |
|------|-------------|
| `list_tool_groups` | Lists all categories with tool counts and names |

---

### Level & Actors
Spawn, move, delete, and inspect actors in the current level.

| Tool | What it does |
|------|-------------|
| `actor_spawn` | Spawn an actor by class at a given location/rotation/scale |
| `actor_spawn_on_surface` | Spawn an actor snapped to the nearest surface below a point |
| `actor_delete` | Delete an actor from the level |
| `actor_duplicate` | Duplicate an actor with optional offset |
| `actor_set_transform` | Set an actor's position, rotation, and/or scale |
| `actor_set_property` | Set a property on an actor by name |
| `actor_get_properties` | Read all exposed properties of an actor |
| `actor_get_selected` | Get the currently selected actors in the editor |
| `actor_list` | List all actors in the level, with optional class filter |
| `actor_find_by_name` | Find an actor by label name |
| `level_get_info` | Get level name, world bounds, and actor count |
| `line_trace` | Cast a ray and return what it hits (location, normal, actor) |
| `viewport_focus` | Focus the editor viewport on an actor |
| `viewport_set_camera` | Set the editor camera position and rotation |
| `console_command` | Execute an Unreal console command |

---

### Blueprints
Create and wire Blueprint assets programmatically.

| Tool | What it does |
|------|-------------|
| `blueprint_create` | Create a new Blueprint class from a parent class |
| `blueprint_add_component` | Add a component (mesh, camera, audio, etc.) to a Blueprint |
| `blueprint_set_component_property` | Set a property on a Blueprint component |
| `blueprint_set_cdo_property` | Set a Class Default Object property |
| `blueprint_set_inherited_component_property` | Set a property on an inherited component |
| `blueprint_add_variable` | Add a typed variable to a Blueprint |
| `blueprint_set_variable` | Set the default value of a Blueprint variable |
| `blueprint_add_function` | Add a new function to a Blueprint |
| `blueprint_add_event_node` | Add an event node (BeginPlay, Tick, custom event, etc.) |
| `blueprint_add_function_call` | Add a function call node to a Blueprint graph |
| `blueprint_connect_pins` | Connect two pins in a Blueprint graph |
| `blueprint_compile` | Compile a Blueprint asset |
| `blueprint_get_graph` | Read the full node/pin/connection graph as JSON |
| `blueprint_find_node` | Find a node by class or title |
| `blueprint_set_node_property` | Set a property on a graph node |
| `blueprint_spawn_in_level` | Add a Blueprint actor to the current level |
| `blueprint_create_input_action` | Create an Enhanced Input action and bind it |
| `blueprint_add_gamemode` | Create a GameMode Blueprint and set it as the default |

---

### C++ Code Generation
Generate C++ source files and trigger editor recompilation.

| Tool | What it does |
|------|-------------|
| `cpp_create_actor` | Generate a new AActor subclass (.h + .cpp) |
| `cpp_create_component` | Generate a new UActorComponent subclass |
| `cpp_create_interface` | Generate a new UInterface |
| `cpp_create_gas_ability` | Generate a UGameplayAbility subclass |
| `cpp_create_gas_effect` | Generate a UGameplayEffect subclass |
| `cpp_create_attribute_set` | Generate a UAttributeSet subclass |
| `cpp_add_include` | Add a #include to an existing header |
| `cpp_add_uproperty` | Add a UPROPERTY to an existing class |
| `cpp_add_ufunction` | Add a UFUNCTION to an existing class |
| `cpp_get_class_info` | Read the properties and functions declared on a class |
| `cpp_hot_reload` | Trigger a hot reload of the project module |
| `cpp_run_ubt` | Run a full UnrealBuildTool compile |

---

### Rendering & Materials
Control Lumen, Nanite, shadows, post-process, fog, sky, and materials at runtime.

| Tool | What it does |
|------|-------------|
| `render_set_lumen` | Enable/disable Lumen GI and configure quality settings |
| `render_get_lumen_settings` | Read current Lumen settings |
| `render_set_nanite` | Enable/disable Nanite on a Static Mesh |
| `render_get_nanite_settings` | Read Nanite settings for a mesh |
| `render_set_shadows` | Configure shadow distance, cascades, and quality |
| `render_set_post_process` | Set post-process volume settings (bloom, AO, DOF, etc.) |
| `render_get_post_process` | Read current post-process settings |
| `render_set_fog` | Configure exponential height fog |
| `render_set_sky_atmosphere` | Configure the sky atmosphere component |
| `render_set_sky_light` | Set sky light intensity and source type |
| `render_set_ambient_occlusion` | Configure SSAO settings |
| `render_get_materials` | List materials assigned to a mesh |
| `render_set_material` | Assign a material to a mesh slot |
| `render_set_material_param` | Set a scalar or vector parameter on a material instance |
| `render_create_material_instance` | Create a Material Instance from a parent material |

---

### Splines & Volumes
Build spline paths and place trigger/blocking volumes.

| Tool | What it does |
|------|-------------|
| `spline_create` | Create a new Spline actor in the level |
| `spline_add_point` | Add a point to a spline |
| `spline_modify_point` | Move or change the tangent of an existing spline point |
| `spline_query` | Read all points and metadata from a spline |
| `spline_attach_mesh` | Attach a Static Mesh along a spline |
| `volume_create` | Create a volume actor (trigger, blocking, post-process, etc.) |
| `volume_set_properties` | Resize and reposition an existing volume |

---

### PCG *(requires Procedural Content Generation plugin)*
Build and execute PCG graphs for procedural level population.

| Tool | What it does |
|------|-------------|
| `pcg_create_graph` | Create a new PCG Graph asset |
| `pcg_add_node` | Add a node to a PCG graph |
| `pcg_connect_pins` | Connect two nodes in a PCG graph |
| `pcg_disconnect_pins` | Disconnect a pin connection |
| `pcg_get_graph` | Read the full PCG graph as JSON |
| `pcg_get_node_params` | Read the parameters on a PCG node |
| `pcg_set_node_param` | Set a parameter on a PCG node |
| `pcg_set_graph_param` | Set a graph-level parameter (exposed input) |
| `pcg_list_node_types` | List all available PCG node types |
| `pcg_execute_graph` | Trigger a PCG graph to regenerate |
| `pcg_create_attribute_node` | Create an attribute node with a given name/type |
| `pcg_create_subgraph` | Create a subgraph node referencing another PCG asset |
| `pcg_place_volume` | Place a PCG Volume in the level and assign a graph |
| `pcg_place_seed_actor` | Place a PCG seed actor for point-based generation |

---

### Gameplay Ability System *(requires GameplayAbilities plugin)*
Wire up GAS components, abilities, attributes, and gameplay tags.

| Tool | What it does |
|------|-------------|
| `gas_add_ability_component` | Add an Ability System Component to a Blueprint |
| `gas_grant_ability` | Grant a gameplay ability to an actor |
| `gas_revoke_ability` | Remove a granted ability from an actor |
| `gas_get_abilities` | List all abilities granted to an actor |
| `gas_apply_effect` | Apply a Gameplay Effect to an actor |
| `gas_get_active_effects` | List active effects on an actor |
| `gas_get_attributes` | Read all attribute values on an actor |
| `gas_set_attribute` | Set an attribute value directly |
| `gas_add_gameplay_tag` | Add a gameplay tag to an actor |
| `gas_remove_gameplay_tag` | Remove a gameplay tag from an actor |
| `gas_get_gameplay_tags` | List all gameplay tags on an actor |
| `gas_list_registered_tags` | List all gameplay tags registered in the project |

---

### Niagara VFX
Spawn, configure, and control Niagara particle systems.

| Tool | What it does |
|------|-------------|
| `niagara_list_systems` | List all Niagara System assets in the project |
| `niagara_create_system` | Create a new Niagara System asset |
| `niagara_list_emitters` | List emitters inside a Niagara System |
| `niagara_get_parameters` | Read user-exposed parameters on a Niagara System |
| `niagara_set_parameter` | Set a parameter value on a Niagara System asset |
| `niagara_spawn_component` | Spawn a Niagara component on an actor in the level |
| `niagara_activate_component` | Activate or deactivate a spawned Niagara component |
| `niagara_set_component_parameter` | Set a runtime parameter on a spawned Niagara component |

---

### Animation
Build Animation Blueprints, state machines, and retarget animations across skeletons.

**Inspection**

| Tool | What it does |
|------|-------------|
| `anim_list_skeletons` | List all Skeleton assets in the project |
| `anim_list_skeletal_meshes` | List all Skeletal Mesh assets |
| `anim_list_anim_blueprints` | List all Animation Blueprints |
| `anim_list_sequences` | List AnimSequences compatible with a given skeleton |
| `anim_get_skeleton_bones` | List every bone name and parent in a skeleton |
| `anim_get_anim_graph` | Inspect all graphs, states, and nodes in an AnimBP |

**Building Animation Blueprints**

| Tool | What it does |
|------|-------------|
| `anim_create_anim_blueprint` | Create a new Animation Blueprint for a skeleton |
| `anim_add_state_machine` | Add a State Machine node to the AnimGraph |
| `anim_add_state` | Add a named state to a State Machine |
| `anim_set_state_animation` | Assign an AnimSequence or BlendSpace to a state |
| `anim_add_transition` | Add a transition arrow between two states |
| `anim_set_transition_condition` | Wire a variable condition onto a transition rule |
| `anim_compile_anim_blueprint` | Compile and save an Animation Blueprint |

**IK Retargeting** *(requires IKRig plugin)*

| Tool | What it does |
|------|-------------|
| `ik_rig_assign_mesh` | Assign a SkeletalMesh to an IK Rig (required first step) |
| `ik_rig_set_retarget_root` | Set the retarget root bone on an IK Rig |
| `ik_rig_add_retarget_chain` | Add a retarget chain (limb, spine, head, etc.) |
| `ik_rig_remove_retarget_chain` | Remove a retarget chain by name |
| `ik_retarget_create` | Create an IK Retargeter pairing two IK Rigs with auto chain-mapping |
| `anim_retarget_batch` | Batch-retarget a list of animations to a new skeleton |

**Maintenance**

| Tool | What it does |
|------|-------------|
| `skeleton_fix_broken_references` | Repair broken skeleton references across anim assets |

> **Animation Blueprint workflow:** add variable → add state machine → add states → assign animations → add transitions → **compile** → set transition conditions → compile again. Compile must happen before `anim_set_transition_condition` so the variable getter can resolve its type.

---

### UMG Widgets
Create and populate Widget Blueprint UIs.

| Tool | What it does |
|------|-------------|
| `umg_list_widgets` | List all Widget Blueprint assets |
| `umg_create_widget` | Create a new Widget Blueprint |
| `umg_get_widget_tree` | Read the full widget tree as JSON |
| `umg_add_widget` | Add a widget (Text, Button, Image, etc.) to the tree |
| `umg_set_text` | Set the text content of a Text widget |
| `umg_set_canvas_slot` | Set position and size of a widget on a Canvas Panel |
| `umg_set_anchor` | Set anchor alignment on a Canvas Slot |
| `umg_set_visibility` | Show or hide a widget |
| `umg_add_variable` | Add a variable to a Widget Blueprint |
| `umg_compile_widget` | Compile a Widget Blueprint |

---

### Sequencer
Build cinematic Level Sequences with camera cuts, tracks, and keyframes.

| Tool | What it does |
|------|-------------|
| `seq_create` | Create a new Level Sequence asset |
| `seq_list` | List all Level Sequences in the project |
| `seq_get_info` | Read sequence duration, framerate, and track layout |
| `seq_set_playback_range` | Set the in/out point of a sequence |
| `seq_add_actor_binding` | Bind an actor to a sequence for animation |
| `seq_add_transform_track` | Add a transform (movement) track to a bound actor |
| `seq_add_keyframe` | Set a keyframe value at a given frame |
| `seq_add_camera_cut` | Add a camera cut section |
| `seq_add_spawnable_camera` | Add a new spawnable camera actor to the sequence |
| `seq_add_visibility_track` | Add a visibility track to a bound actor |

---

### Asset Management
Browse, move, copy, delete, and migrate content browser assets.

| Tool | What it does |
|------|-------------|
| `asset_list` | List assets in a folder (with optional class filter) |
| `asset_get_info` | Get metadata for an asset (class, path, size, dependencies) |
| `asset_get_references` | List assets that reference a given asset |
| `asset_get_dependencies` | List assets that a given asset depends on |
| `asset_move` | Move or rename an asset |
| `asset_copy` | Duplicate an asset to a new path |
| `asset_delete` | Delete an asset permanently |
| `asset_create_folder` | Create a new content browser folder |
| `asset_list_redirectors` | List all redirector assets in a path |
| `asset_fix_redirectors` | Consolidate and remove redirectors |
| `asset_migrate` | Migrate an asset and all dependencies to another project |

---

### Viewport & Input
Capture screenshots and drive the editor UI programmatically.

| Tool | What it does |
|------|-------------|
| `viewport_capture` | Capture the 3D viewport or full editor window as a PNG |
| `viewport_focus` | Focus the viewport on an actor |
| `viewport_set_camera` | Set the editor camera position and rotation |
| `editor_input` | Simulate mouse clicks and keyboard input on the editor window |

---

### Fab Marketplace
Search and inspect assets from the Fab marketplace inside the editor.

| Tool | What it does |
|------|-------------|
| `fab_login_status` | Check EOS login state (required before purchases) |
| `fab_search` | Search Fab for assets by keyword *(dynamic — load once per session)* |
| `fab_get_asset` | Get full details for a Fab asset by ID *(dynamic — load once per session)* |

Load the dynamic tools once per session:
```
python_reload_tools_from_file  →  Scripts/fab_tools.json
```

> **Purchase consent policy:** `fab_get_asset` is information-only. Every response includes an explicit `CONSENT REQUIRED` notice. AI agents must never trigger a purchase, add-to-cart, or any payment action without explicit user confirmation.

---

### Python & Dynamic Tools
Execute Python in the editor context and register new tools at runtime without recompiling.

| Tool | What it does |
|------|-------------|
| `python_exec` | Run a Python snippet in the editor's Python environment |
| `python_exec_file` | Run a Python script file from the project's Scripts folder |
| `python_check` | Check whether a Python expression evaluates without error |
| `python_set_var` | Store a named variable for use across python_exec calls |
| `python_get_var` | Read a stored variable |
| `python_list_scripts` | List available Python scripts in the Scripts folder |
| `python_register_tool` | Register a new MCP tool backed by a Python snippet |
| `python_unregister_tool` | Remove a dynamically registered tool |
| `python_list_dynamic_tools` | List all currently registered dynamic tools |
| `python_reload_tools_from_file` | Load a batch of tools from a JSON file |

**Example — register a tool at runtime:**
```json
{
  "name": "python_register_tool",
  "arguments": {
    "name": "count_actors",
    "description": "Count all actors in the current level.",
    "code": "import unreal\nactors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()\n_mcp_result = f'{len(actors)} actors'"
  }
}
```

Tools receive their arguments as `_mcp_params` (dict) and return a result by assigning `_mcp_result` (string).

**Batch load from a JSON file:**
```json
[
  { "name": "tool_a", "description": "...", "code": "..." },
  { "name": "tool_b", "description": "...", "code": "..." }
]
```

---

## Optional Plugin Dependencies

Some categories only activate when their plugin is enabled:

| Category | Required Plugin | Enable via |
|----------|----------------|------------|
| PCG | Procedural Content Generation | Plugins window |
| GAS | Gameplay Abilities | Plugins window |
| Niagara | Niagara | Enabled by default in UE5 |
| IK Retarget | IKRig | Plugins window |

All other categories load unconditionally.

---

## Architecture

- **Transport:** HTTP + SSE on `localhost:7777`
- **Protocol:** MCP 2024-11-05 / JSON-RPC 2.0
- **Engine version:** UE 5.6+ (5.7 compatible)
- **Module type:** Editor plugin, `PostEngineInit`
- **Threading:** HTTP handlers run on the game thread tick — all UE API calls are direct, no `AsyncTask` wrappers

### Module layout

| Module | Contents |
|--------|----------|
| `UECortex` | Core server, router, registry, level/blueprint/rendering/spline/volume/cpp/viewport/fab/meta tools |
| `UECortexAsset` | Asset management tools |
| `UECortexAnim` | Animation Blueprint, skeleton, and state machine tools |
| `UECortexIKRetarget` | IK Rig setup and batch retarget tools *(optional — requires IKRig)* |
| `UECortexUMG` | Widget Blueprint tools |
| `UECortexSequencer` | Level Sequence / cinematics tools |
| `UECortexPCG` | PCG graph tools *(optional — requires PCG plugin)* |
| `UECortexGAS` | Gameplay Ability System tools *(optional — requires GameplayAbilities)* |
| `UECortexNiagara` | Niagara VFX tools |
| `UECortexPython` | Python executor + dynamic tool registration |
