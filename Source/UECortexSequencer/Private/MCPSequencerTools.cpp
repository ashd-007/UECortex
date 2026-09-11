#include "MCPSequencerTools.h"
#include "MCPToolRegistry.h"
#include "UECortexModule.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneObjectBindingID.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneVisibilityTrack.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "CineCameraActor.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "FileHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "MovieSceneTimeHelpers.h"

// ---------------------------------------------------------------------------
// NOTE: HTTP handlers are called from the game thread tick (FHttpServerModule::ProcessRequests).
// Do NOT use AsyncTask(GameThread) + FEvent — deadlocks. Call UE APIs directly.
// ---------------------------------------------------------------------------

// --------------- Helpers ---------------------------------------------------

static ULevelSequence* LoadSeq(const FString& Path)
{
	FString Full = Path;
	if (!Full.Contains(TEXT(".")))
		Full += TEXT(".") + FPackageName::GetShortName(Path);
	return LoadObject<ULevelSequence>(nullptr, *Full);
}

static FGuid FindBindingByName(UMovieScene* MS, const FString& Name)
{
	for (int32 i = 0; i < MS->GetPossessableCount(); i++)
	{
		const FMovieScenePossessable& P = MS->GetPossessable(i);
		if (P.GetName() == Name) return P.GetGuid();
	}
	for (int32 i = 0; i < MS->GetSpawnableCount(); i++)
	{
		const FMovieSceneSpawnable& S = MS->GetSpawnable(i);
		if (S.GetName() == Name) return S.GetGuid();
	}
	return FGuid();
}

static FFrameNumber ToTick(int32 DisplayFrame, UMovieScene* MS)
{
	return ConvertFrameTime(
		FFrameTime(DisplayFrame),
		MS->GetDisplayRate(),
		MS->GetTickResolution()
	).RoundToFrame();
}

static int32 ToDisplay(FFrameNumber Tick, UMovieScene* MS)
{
	return ConvertFrameTime(
		FFrameTime(Tick),
		MS->GetTickResolution(),
		MS->GetDisplayRate()
	).RoundToFrame().Value;
}

// --------------- RegisterTools --------------------------------------------

void FMCPSequencerTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	{
		FMCPToolDef T;
		T.Name = TEXT("seq_create");
		T.Description = TEXT("Create a new Level Sequence asset. Params: path (string, e.g. /Game/Cinematics/MySequence), fps (int, default 24).");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return SeqCreate(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("seq_list");
		T.Description = TEXT("List all Level Sequence assets in the project.");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return SeqList(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("seq_get_info");
		T.Description = TEXT("Get info about a sequence: bindings, tracks, playback range. Params: seq_path (string).");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return SeqGetInfo(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("seq_set_playback_range");
		T.Description = TEXT("Set sequence playback range and optionally FPS. Params: seq_path, start_frame (int), end_frame (int), fps (int, optional).");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return SeqSetPlaybackRange(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("seq_add_actor_binding");
		T.Description = TEXT("Bind an existing level actor as a possessable in the sequence. Params: seq_path, actor_name (string — actor label or name).");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return SeqAddActorBinding(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("seq_add_transform_track");
		T.Description = TEXT("Add a 3D transform track to a binding. Params: seq_path, binding_name (string).");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return SeqAddTransformTrack(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("seq_add_keyframe");
		T.Description = TEXT("Add a transform keyframe. Params: seq_path, binding_name, frame (int), position {x,y,z} (optional), rotation {pitch,yaw,roll} (optional).");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return SeqAddKeyframe(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("seq_add_camera_cut");
		T.Description = TEXT("Add a camera cut section. Params: seq_path, camera_binding_name (string), start_frame (int), end_frame (int).");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return SeqAddCameraCut(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("seq_add_spawnable_camera");
		T.Description = TEXT("Add a CineCamera spawnable to the sequence with a transform track. Params: seq_path, camera_name (string, default 'CineCamera').");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return SeqAddSpawnableCamera(P); };
		OutTools.Add(T);
	}
	{
		FMCPToolDef T;
		T.Name = TEXT("seq_add_visibility_track");
		T.Description = TEXT("Add a visibility (show/hide) track to a binding. Params: seq_path, binding_name.");
		T.Handler = [](const TSharedPtr<FJsonObject>& P) { return SeqAddVisibilityTrack(P); };
		OutTools.Add(T);
	}
}

// --------------- Tool implementations -------------------------------------

FMCPToolResult FMCPSequencerTools::SeqCreate(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	if (!Params->TryGetStringField(TEXT("path"), Path))
		return FMCPToolResult::Error(TEXT("Missing 'path'"));

	FString AssetName = FPackageName::GetShortName(Path);
	UPackage* Package = CreatePackage(*Path);
	if (!Package) return FMCPToolResult::Error(TEXT("Failed to create package"));

	ULevelSequence* Seq = NewObject<ULevelSequence>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
	if (!Seq) return FMCPToolResult::Error(TEXT("Failed to create LevelSequence"));

	Seq->Initialize();

	UMovieScene* MS = Seq->GetMovieScene();
	int32 FPS = Params->HasField(TEXT("fps")) ? (int32)Params->GetNumberField(TEXT("fps")) : 24;
	MS->SetDisplayRate(FFrameRate(FPS, 1));

	// Default 5 second range
	FFrameNumber Start = ToTick(0, MS);
	FFrameNumber End   = ToTick(FPS * 5, MS);
	MS->SetPlaybackRange(TRange<FFrameNumber>(Start, End));

	FAssetRegistryModule::AssetCreated(Seq);
	Package->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(TEXT("Created Level Sequence '%s' (%dfps, 0-%d frames)"), *Path, FPS, FPS * 5));
}

FMCPToolResult FMCPSequencerTools::SeqList(const TSharedPtr<FJsonObject>& Params)
{
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/LevelSequence"), TEXT("LevelSequence")));
	Filter.PackagePaths.Add(TEXT("/Game"));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	AR.GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> List;
	for (const FAssetData& AD : Assets)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), AD.AssetName.ToString());
		Obj->SetStringField(TEXT("path"), AD.PackageName.ToString());
		List.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("sequences"), List);
	Result->SetNumberField(TEXT("count"), Assets.Num());

	FString Json;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

	return FMCPToolResult::Success(FString::Printf(TEXT("Found %d level sequences\n%s"), Assets.Num(), *Json));
}

FMCPToolResult FMCPSequencerTools::SeqGetInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString SeqPath;
	if (!Params->TryGetStringField(TEXT("seq_path"), SeqPath))
		return FMCPToolResult::Error(TEXT("Missing 'seq_path'"));

	ULevelSequence* Seq = LoadSeq(SeqPath);
	if (!Seq) return FMCPToolResult::Error(FString::Printf(TEXT("Sequence not found: %s"), *SeqPath));

	UMovieScene* MS = Seq->GetMovieScene();
	FFrameRate DisplayRate = MS->GetDisplayRate();
	TRange<FFrameNumber> Range = MS->GetPlaybackRange();

	int32 StartFrame = ToDisplay(Range.GetLowerBoundValue(), MS);
	int32 EndFrame   = ToDisplay(Range.GetUpperBoundValue(), MS);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), SeqPath);
	Result->SetNumberField(TEXT("fps"), DisplayRate.AsDecimal());
	Result->SetNumberField(TEXT("start_frame"), StartFrame);
	Result->SetNumberField(TEXT("end_frame"), EndFrame);
	Result->SetNumberField(TEXT("duration_frames"), EndFrame - StartFrame);

	TArray<TSharedPtr<FJsonValue>> Bindings;

	for (int32 i = 0; i < MS->GetPossessableCount(); i++)
	{
		const FMovieScenePossessable& P = MS->GetPossessable(i);
		TSharedPtr<FJsonObject> B = MakeShared<FJsonObject>();
		B->SetStringField(TEXT("name"), P.GetName());
		B->SetStringField(TEXT("guid"), P.GetGuid().ToString());
		B->SetStringField(TEXT("type"), TEXT("possessable"));
		const FMovieSceneBinding* Binding = MS->FindBinding(P.GetGuid());
		B->SetNumberField(TEXT("track_count"), Binding ? Binding->GetTracks().Num() : 0);
		Bindings.Add(MakeShared<FJsonValueObject>(B));
	}

	for (int32 i = 0; i < MS->GetSpawnableCount(); i++)
	{
		const FMovieSceneSpawnable& S = MS->GetSpawnable(i);
		TSharedPtr<FJsonObject> B = MakeShared<FJsonObject>();
		B->SetStringField(TEXT("name"), S.GetName());
		B->SetStringField(TEXT("guid"), S.GetGuid().ToString());
		B->SetStringField(TEXT("type"), TEXT("spawnable"));
		const FMovieSceneBinding* Binding = MS->FindBinding(S.GetGuid());
		B->SetNumberField(TEXT("track_count"), Binding ? Binding->GetTracks().Num() : 0);
		Bindings.Add(MakeShared<FJsonValueObject>(B));
	}

	Result->SetArrayField(TEXT("bindings"), Bindings);
	Result->SetNumberField(TEXT("binding_count"), MS->GetPossessableCount() + MS->GetSpawnableCount());

	FString Json;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);

	return FMCPToolResult::Success(FString::Printf(TEXT("Sequence '%s': %d bindings, %d-%d frames @ %.0ffps\n%s"),
		*FPackageName::GetShortName(SeqPath),
		MS->GetPossessableCount() + MS->GetSpawnableCount(),
		StartFrame, EndFrame, DisplayRate.AsDecimal(), *Json));
}

FMCPToolResult FMCPSequencerTools::SeqSetPlaybackRange(const TSharedPtr<FJsonObject>& Params)
{
	FString SeqPath;
	if (!Params->TryGetStringField(TEXT("seq_path"), SeqPath))
		return FMCPToolResult::Error(TEXT("Missing 'seq_path'"));

	ULevelSequence* Seq = LoadSeq(SeqPath);
	if (!Seq) return FMCPToolResult::Error(FString::Printf(TEXT("Sequence not found: %s"), *SeqPath));

	UMovieScene* MS = Seq->GetMovieScene();

	if (Params->HasField(TEXT("fps")))
		MS->SetDisplayRate(FFrameRate((int32)Params->GetNumberField(TEXT("fps")), 1));

	int32 StartFrame = Params->HasField(TEXT("start_frame")) ? (int32)Params->GetNumberField(TEXT("start_frame")) : 0;
	int32 EndFrame   = (int32)Params->GetNumberField(TEXT("end_frame"));

	MS->SetPlaybackRange(TRange<FFrameNumber>(ToTick(StartFrame, MS), ToTick(EndFrame, MS)));
	Seq->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(TEXT("Set playback range [%d, %d] at %.0ffps on '%s'"),
		StartFrame, EndFrame, MS->GetDisplayRate().AsDecimal(), *FPackageName::GetShortName(SeqPath)));
}

FMCPToolResult FMCPSequencerTools::SeqAddActorBinding(const TSharedPtr<FJsonObject>& Params)
{
	FString SeqPath, ActorName;
	if (!Params->TryGetStringField(TEXT("seq_path"), SeqPath))
		return FMCPToolResult::Error(TEXT("Missing 'seq_path'"));
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("Missing 'actor_name'"));

	ULevelSequence* Seq = LoadSeq(SeqPath);
	if (!Seq) return FMCPToolResult::Error(FString::Printf(TEXT("Sequence not found: %s"), *SeqPath));

	UWorld* World = FUECortexModule::GetActiveWorld();
	if (!World) return FMCPToolResult::Error(TEXT("No editor world"));

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}
	if (!FoundActor) return FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found in level"), *ActorName));

	UMovieScene* MS = Seq->GetMovieScene();
	FGuid Guid = MS->AddPossessable(ActorName, FoundActor->GetClass());
	Seq->BindPossessableObject(Guid, *FoundActor, World);
	Seq->MarkPackageDirty();

	return FMCPToolResult::Success(FString::Printf(TEXT("Bound actor '%s' to '%s' (GUID: %s)"),
		*ActorName, *FPackageName::GetShortName(SeqPath), *Guid.ToString()));
}

FMCPToolResult FMCPSequencerTools::SeqAddTransformTrack(const TSharedPtr<FJsonObject>& Params)
{
	FString SeqPath, BindingName;
	if (!Params->TryGetStringField(TEXT("seq_path"), SeqPath))
		return FMCPToolResult::Error(TEXT("Missing 'seq_path'"));
	if (!Params->TryGetStringField(TEXT("binding_name"), BindingName))
		return FMCPToolResult::Error(TEXT("Missing 'binding_name'"));

	ULevelSequence* Seq = LoadSeq(SeqPath);
	if (!Seq) return FMCPToolResult::Error(FString::Printf(TEXT("Sequence not found: %s"), *SeqPath));

	UMovieScene* MS = Seq->GetMovieScene();
	FGuid Guid = FindBindingByName(MS, BindingName);
	if (!Guid.IsValid()) return FMCPToolResult::Error(FString::Printf(TEXT("Binding '%s' not found"), *BindingName));

	if (MS->FindTrack<UMovieScene3DTransformTrack>(Guid))
		return FMCPToolResult::Error(TEXT("Transform track already exists on this binding"));

	UMovieScene3DTransformTrack* Track = MS->AddTrack<UMovieScene3DTransformTrack>(Guid);
	if (!Track) return FMCPToolResult::Error(TEXT("Failed to add transform track"));

	UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(Track->CreateNewSection());
	Section->SetRange(TRange<FFrameNumber>::All());
	Track->AddSection(*Section);

	Seq->MarkPackageDirty();
	return FMCPToolResult::Success(FString::Printf(TEXT("Added transform track to binding '%s'"), *BindingName));
}

FMCPToolResult FMCPSequencerTools::SeqAddKeyframe(const TSharedPtr<FJsonObject>& Params)
{
	FString SeqPath, BindingName;
	if (!Params->TryGetStringField(TEXT("seq_path"), SeqPath))
		return FMCPToolResult::Error(TEXT("Missing 'seq_path'"));
	if (!Params->TryGetStringField(TEXT("binding_name"), BindingName))
		return FMCPToolResult::Error(TEXT("Missing 'binding_name'"));
	if (!Params->HasField(TEXT("frame")))
		return FMCPToolResult::Error(TEXT("Missing 'frame'"));

	int32 Frame = (int32)Params->GetNumberField(TEXT("frame"));

	ULevelSequence* Seq = LoadSeq(SeqPath);
	if (!Seq) return FMCPToolResult::Error(FString::Printf(TEXT("Sequence not found: %s"), *SeqPath));

	UMovieScene* MS = Seq->GetMovieScene();
	FGuid Guid = FindBindingByName(MS, BindingName);
	if (!Guid.IsValid()) return FMCPToolResult::Error(FString::Printf(TEXT("Binding '%s' not found"), *BindingName));

	UMovieScene3DTransformTrack* Track = MS->FindTrack<UMovieScene3DTransformTrack>(Guid);
	if (!Track) return FMCPToolResult::Error(TEXT("No transform track — call seq_add_transform_track first"));

	if (Track->GetAllSections().Num() == 0)
		return FMCPToolResult::Error(TEXT("Transform track has no sections"));

	UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(Track->GetAllSections()[0]);
	if (!Section) return FMCPToolResult::Error(TEXT("No transform section"));

	FFrameNumber TickFrame = ToTick(Frame, MS);
	TArrayView<FMovieSceneDoubleChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();

	// Channels: [0-2] Translation XYZ, [3-5] Rotation XYZ (pitch/yaw/roll), [6-8] Scale XYZ
	bool bSetAny = false;

	const TSharedPtr<FJsonObject>* PosObj;
	if (Params->TryGetObjectField(TEXT("position"), PosObj))
	{
		Channels[0]->AddLinearKey(TickFrame, (*PosObj)->GetNumberField(TEXT("x")));
		Channels[1]->AddLinearKey(TickFrame, (*PosObj)->GetNumberField(TEXT("y")));
		Channels[2]->AddLinearKey(TickFrame, (*PosObj)->GetNumberField(TEXT("z")));
		bSetAny = true;
	}

	const TSharedPtr<FJsonObject>* RotObj;
	if (Params->TryGetObjectField(TEXT("rotation"), RotObj))
	{
		Channels[3]->AddLinearKey(TickFrame, (*RotObj)->GetNumberField(TEXT("pitch")));
		Channels[4]->AddLinearKey(TickFrame, (*RotObj)->GetNumberField(TEXT("yaw")));
		Channels[5]->AddLinearKey(TickFrame, (*RotObj)->GetNumberField(TEXT("roll")));
		bSetAny = true;
	}

	if (!bSetAny)
		return FMCPToolResult::Error(TEXT("Provide at least 'position' or 'rotation'"));

	Seq->MarkPackageDirty();
	return FMCPToolResult::Success(FString::Printf(TEXT("Added keyframe at frame %d for binding '%s'"), Frame, *BindingName));
}

FMCPToolResult FMCPSequencerTools::SeqAddCameraCut(const TSharedPtr<FJsonObject>& Params)
{
	FString SeqPath, CameraBindingName;
	if (!Params->TryGetStringField(TEXT("seq_path"), SeqPath))
		return FMCPToolResult::Error(TEXT("Missing 'seq_path'"));
	if (!Params->TryGetStringField(TEXT("camera_binding_name"), CameraBindingName))
		return FMCPToolResult::Error(TEXT("Missing 'camera_binding_name'"));
	if (!Params->HasField(TEXT("start_frame")) || !Params->HasField(TEXT("end_frame")))
		return FMCPToolResult::Error(TEXT("Missing 'start_frame' or 'end_frame'"));

	int32 StartFrame = (int32)Params->GetNumberField(TEXT("start_frame"));
	int32 EndFrame   = (int32)Params->GetNumberField(TEXT("end_frame"));

	ULevelSequence* Seq = LoadSeq(SeqPath);
	if (!Seq) return FMCPToolResult::Error(FString::Printf(TEXT("Sequence not found: %s"), *SeqPath));

	UMovieScene* MS = Seq->GetMovieScene();
	FGuid CameraGuid = FindBindingByName(MS, CameraBindingName);
	if (!CameraGuid.IsValid())
		return FMCPToolResult::Error(FString::Printf(TEXT("Camera binding '%s' not found"), *CameraBindingName));

	UMovieSceneCameraCutTrack* CutTrack = Cast<UMovieSceneCameraCutTrack>(MS->GetCameraCutTrack());
	if (!CutTrack)
		CutTrack = Cast<UMovieSceneCameraCutTrack>(MS->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));

	UMovieSceneCameraCutSection* Section = Cast<UMovieSceneCameraCutSection>(CutTrack->CreateNewSection());
	Section->SetRange(TRange<FFrameNumber>(ToTick(StartFrame, MS), ToTick(EndFrame, MS)));
	Section->SetCameraBindingID(UE::MovieScene::FFixedObjectBindingID(CameraGuid, MovieSceneSequenceID::Root));
	CutTrack->AddSection(*Section);

	Seq->MarkPackageDirty();
	return FMCPToolResult::Success(FString::Printf(TEXT("Added camera cut [%d-%d] using camera '%s'"),
		StartFrame, EndFrame, *CameraBindingName));
}

FMCPToolResult FMCPSequencerTools::SeqAddSpawnableCamera(const TSharedPtr<FJsonObject>& Params)
{
	FString SeqPath;
	if (!Params->TryGetStringField(TEXT("seq_path"), SeqPath))
		return FMCPToolResult::Error(TEXT("Missing 'seq_path'"));

	FString CameraName = TEXT("CineCamera");
	Params->TryGetStringField(TEXT("camera_name"), CameraName);

	ULevelSequence* Seq = LoadSeq(SeqPath);
	if (!Seq) return FMCPToolResult::Error(FString::Printf(TEXT("Sequence not found: %s"), *SeqPath));

	UMovieScene* MS = Seq->GetMovieScene();

	UObject* CameraTemplate = ACineCameraActor::StaticClass()->GetDefaultObject();
	FGuid CameraGuid = MS->AddSpawnable(*CameraName, *CameraTemplate);
	if (!CameraGuid.IsValid()) return FMCPToolResult::Error(TEXT("Failed to add spawnable camera"));

	// Add a transform track so it can be keyframed immediately
	UMovieScene3DTransformTrack* Track = MS->AddTrack<UMovieScene3DTransformTrack>(CameraGuid);
	if (Track)
	{
		UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(Track->CreateNewSection());
		Section->SetRange(TRange<FFrameNumber>::All());
		Track->AddSection(*Section);
	}

	Seq->MarkPackageDirty();
	return FMCPToolResult::Success(FString::Printf(
		TEXT("Added spawnable CineCamera '%s' (GUID: %s). Use seq_add_keyframe with binding_name='%s' to animate."),
		*CameraName, *CameraGuid.ToString(), *CameraName));
}

FMCPToolResult FMCPSequencerTools::SeqAddVisibilityTrack(const TSharedPtr<FJsonObject>& Params)
{
	FString SeqPath, BindingName;
	if (!Params->TryGetStringField(TEXT("seq_path"), SeqPath))
		return FMCPToolResult::Error(TEXT("Missing 'seq_path'"));
	if (!Params->TryGetStringField(TEXT("binding_name"), BindingName))
		return FMCPToolResult::Error(TEXT("Missing 'binding_name'"));

	ULevelSequence* Seq = LoadSeq(SeqPath);
	if (!Seq) return FMCPToolResult::Error(FString::Printf(TEXT("Sequence not found: %s"), *SeqPath));

	UMovieScene* MS = Seq->GetMovieScene();
	FGuid Guid = FindBindingByName(MS, BindingName);
	if (!Guid.IsValid()) return FMCPToolResult::Error(FString::Printf(TEXT("Binding '%s' not found"), *BindingName));

	if (MS->FindTrack<UMovieSceneVisibilityTrack>(Guid))
		return FMCPToolResult::Error(TEXT("Visibility track already exists on this binding"));

	UMovieSceneVisibilityTrack* Track = MS->AddTrack<UMovieSceneVisibilityTrack>(Guid);
	if (!Track) return FMCPToolResult::Error(TEXT("Failed to add visibility track"));

	UMovieSceneBoolSection* Section = Cast<UMovieSceneBoolSection>(Track->CreateNewSection());
	Section->SetRange(TRange<FFrameNumber>::All());
	Section->GetChannel().SetDefault(true);
	Track->AddSection(*Section);

	Seq->MarkPackageDirty();
	return FMCPToolResult::Success(FString::Printf(TEXT("Added visibility track to binding '%s'"), *BindingName));
}
