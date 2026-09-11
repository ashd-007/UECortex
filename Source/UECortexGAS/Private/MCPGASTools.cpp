#include "MCPGASTools.h"
#include "MCPToolBase.h"
#include "UECortexModule.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Async/Async.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameplayTagsManager.h"
#include "GameplayTagContainer.h"

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

static UAbilitySystemComponent* GetASC(AActor* Actor)
{
	if (!Actor) return nullptr;
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor);
	if (ASI) return ASI->GetAbilitySystemComponent();
	return Actor->FindComponentByClass<UAbilitySystemComponent>();
}

// ─────────────────────────────────────────────
//  RegisterTools
// ─────────────────────────────────────────────

void FMCPGASTools::RegisterTools(TArray<FMCPToolDef>& OutTools)
{
	// --- gas_add_ability_component ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_add_ability_component");
		T.Description = TEXT("Add an AbilitySystemComponent to an actor Blueprint and configure it");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Actor label or name"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASAddAbilityComponent(A); };
		OutTools.Add(T);
	}

	// --- gas_grant_ability ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_grant_ability");
		T.Description = TEXT("Grant a GameplayAbility class to an actor's AbilitySystemComponent");
		T.Params = {
			{ TEXT("actor_name"),   TEXT("string"), TEXT("Actor label or name"), true },
			{ TEXT("ability_class"),TEXT("string"), TEXT("Full class path of the GameplayAbility asset"), true },
			{ TEXT("level"),        TEXT("number"), TEXT("Ability level (default 1)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASGrantAbility(A); };
		OutTools.Add(T);
	}

	// --- gas_revoke_ability ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_revoke_ability");
		T.Description = TEXT("Revoke a granted GameplayAbility from an actor");
		T.Params = {
			{ TEXT("actor_name"),   TEXT("string"), TEXT("Actor label or name"), true },
			{ TEXT("ability_class"),TEXT("string"), TEXT("Full class path of the GameplayAbility"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASRevokeAbility(A); };
		OutTools.Add(T);
	}

	// --- gas_get_abilities ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_get_abilities");
		T.Description = TEXT("List all granted abilities on an actor's AbilitySystemComponent");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Actor label or name"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASGetAbilities(A); };
		OutTools.Add(T);
	}

	// --- gas_add_gameplay_tag ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_add_gameplay_tag");
		T.Description = TEXT("Add a gameplay tag to an actor's AbilitySystemComponent");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Actor label or name"), true },
			{ TEXT("tag"),        TEXT("string"), TEXT("Tag string e.g. 'Character.State.Stunned'"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASAddGameplayTag(A); };
		OutTools.Add(T);
	}

	// --- gas_remove_gameplay_tag ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_remove_gameplay_tag");
		T.Description = TEXT("Remove a gameplay tag from an actor's AbilitySystemComponent");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Actor label or name"), true },
			{ TEXT("tag"),        TEXT("string"), TEXT("Tag string to remove"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASRemoveGameplayTag(A); };
		OutTools.Add(T);
	}

	// --- gas_get_gameplay_tags ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_get_gameplay_tags");
		T.Description = TEXT("Get all gameplay tags on an actor's AbilitySystemComponent");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Actor label or name"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASGetGameplayTags(A); };
		OutTools.Add(T);
	}

	// --- gas_list_registered_tags ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_list_registered_tags");
		T.Description = TEXT("List all registered gameplay tags in the project (optional prefix filter)");
		T.Params = {
			{ TEXT("prefix"), TEXT("string"), TEXT("Optional tag prefix filter e.g. 'Character'"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASListRegisteredTags(A); };
		OutTools.Add(T);
	}

	// --- gas_set_attribute ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_set_attribute");
		T.Description = TEXT("Set a numeric attribute value on an actor's AbilitySystemComponent");
		T.Params = {
			{ TEXT("actor_name"),     TEXT("string"), TEXT("Actor label or name"), true },
			{ TEXT("attribute_name"), TEXT("string"), TEXT("Attribute name e.g. 'Health', 'MaxHealth'"), true },
			{ TEXT("value"),          TEXT("number"), TEXT("New base value"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASSetAttribute(A); };
		OutTools.Add(T);
	}

	// --- gas_get_attributes ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_get_attributes");
		T.Description = TEXT("Get all attribute values on an actor's AbilitySystemComponent");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Actor label or name"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASGetAttributes(A); };
		OutTools.Add(T);
	}

	// --- gas_apply_effect ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_apply_effect");
		T.Description = TEXT("Apply a GameplayEffect to an actor's AbilitySystemComponent");
		T.Params = {
			{ TEXT("actor_name"),    TEXT("string"), TEXT("Target actor label or name"), true },
			{ TEXT("effect_class"),  TEXT("string"), TEXT("Full class path of the GameplayEffect asset"), true },
			{ TEXT("level"),         TEXT("number"), TEXT("Effect level (default 1)"), false },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASApplyEffect(A); };
		OutTools.Add(T);
	}

	// --- gas_get_active_effects ---
	{
		FMCPToolDef T;
		T.Name = TEXT("gas_get_active_effects");
		T.Description = TEXT("List all active gameplay effects on an actor's AbilitySystemComponent");
		T.Params = {
			{ TEXT("actor_name"), TEXT("string"), TEXT("Actor label or name"), true },
		};
		T.Handler = [](const TSharedPtr<FJsonObject>& A) { return GASGetActiveEffects(A); };
		OutTools.Add(T);
	}
}

// ─────────────────────────────────────────────
//  Tool implementations
// ─────────────────────────────────────────────

FMCPToolResult FMCPGASTools::GASAddAbilityComponent(const TSharedPtr<FJsonObject>& Args)
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

		UAbilitySystemComponent* ExistingASC = Actor->FindComponentByClass<UAbilitySystemComponent>();
		if (ExistingASC)
		{
			Result = FMCPToolResult::Success(FString::Printf(TEXT("Actor '%s' already has AbilitySystemComponent"), *ActorName));
			return;
		}

		UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Actor, TEXT("AbilitySystemComponent"));
		ASC->RegisterComponent();
		Actor->AddInstanceComponent(ASC);
		Actor->Modify();

		Result = FMCPToolResult::Success(FString::Printf(TEXT("Added AbilitySystemComponent to '%s'"), *ActorName));
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPGASTools::GASGrantAbility(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName, AbilityClassPath;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("actor_name required"));
	if (!Args->TryGetStringField(TEXT("ability_class"), AbilityClassPath))
		return FMCPToolResult::Error(TEXT("ability_class required"));

	int32 Level = 1;
	Args->TryGetNumberField(TEXT("level"), Level);

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, AbilityClassPath, Level, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByName(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no AbilitySystemComponent"), *ActorName)); return; }

		UClass* AbilityClass = LoadClass<UGameplayAbility>(nullptr, *AbilityClassPath);
		if (!AbilityClass) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Ability class '%s' not found"), *AbilityClassPath)); return; }

		FGameplayAbilitySpec Spec(AbilityClass, Level);
		FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);

		auto Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("handle"), Handle.ToString());
		Data->SetStringField(TEXT("ability_class"), AbilityClassPath);
		Data->SetNumberField(TEXT("level"), Level);
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Granted ability '%s' to '%s'"), *AbilityClassPath, *ActorName), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPGASTools::GASRevokeAbility(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName, AbilityClassPath;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("actor_name required"));
	if (!Args->TryGetStringField(TEXT("ability_class"), AbilityClassPath))
		return FMCPToolResult::Error(TEXT("ability_class required"));

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, AbilityClassPath, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByName(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no AbilitySystemComponent"), *ActorName)); return; }

		UClass* AbilityClass = LoadClass<UGameplayAbility>(nullptr, *AbilityClassPath);
		if (!AbilityClass) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Ability class '%s' not found"), *AbilityClassPath)); return; }

		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass)
			{
				ASC->ClearAbility(Spec.Handle);
				Result = FMCPToolResult::Success(FString::Printf(TEXT("Revoked ability '%s' from '%s'"), *AbilityClassPath, *ActorName));
				return;
			}
		}
		Result = FMCPToolResult::Error(FString::Printf(TEXT("Ability '%s' not found on '%s'"), *AbilityClassPath, *ActorName));
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPGASTools::GASGetAbilities(const TSharedPtr<FJsonObject>& Args)
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

		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no AbilitySystemComponent"), *ActorName)); return; }

		TArray<TSharedPtr<FJsonValue>> AbilityArr;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			auto Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("handle"), Spec.Handle.ToString());
			Obj->SetStringField(TEXT("class"), Spec.Ability ? Spec.Ability->GetClass()->GetName() : TEXT("None"));
			Obj->SetNumberField(TEXT("level"), Spec.Level);
			Obj->SetBoolField(TEXT("active"), Spec.IsActive());
			AbilityArr.Add(MakeShared<FJsonValueObject>(Obj));
		}

		auto Data = MakeShared<FJsonObject>();
		Data->SetArrayField(TEXT("abilities"), AbilityArr);
		Data->SetNumberField(TEXT("count"), AbilityArr.Num());
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Found %d abilities on '%s'"), AbilityArr.Num(), *ActorName), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPGASTools::GASAddGameplayTag(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName, TagStr;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("actor_name required"));
	if (!Args->TryGetStringField(TEXT("tag"), TagStr))
		return FMCPToolResult::Error(TEXT("tag required"));

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, TagStr, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByName(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no AbilitySystemComponent"), *ActorName)); return; }

		FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
		if (!Tag.IsValid()) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Tag '%s' is not registered"), *TagStr)); return; }

		ASC->AddLooseGameplayTag(Tag);
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Added tag '%s' to '%s'"), *TagStr, *ActorName));
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPGASTools::GASRemoveGameplayTag(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName, TagStr;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("actor_name required"));
	if (!Args->TryGetStringField(TEXT("tag"), TagStr))
		return FMCPToolResult::Error(TEXT("tag required"));

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, TagStr, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByName(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no AbilitySystemComponent"), *ActorName)); return; }

		FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
		if (!Tag.IsValid()) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Tag '%s' is not registered"), *TagStr)); return; }

		ASC->RemoveLooseGameplayTag(Tag);
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Removed tag '%s' from '%s'"), *TagStr, *ActorName));
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPGASTools::GASGetGameplayTags(const TSharedPtr<FJsonObject>& Args)
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

		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no AbilitySystemComponent"), *ActorName)); return; }

		FGameplayTagContainer Tags;
		ASC->GetOwnedGameplayTags(Tags);

		TArray<TSharedPtr<FJsonValue>> TagArr;
		for (const FGameplayTag& Tag : Tags)
		{
			TagArr.Add(MakeShared<FJsonValueString>(Tag.ToString()));
		}

		auto Data = MakeShared<FJsonObject>();
		Data->SetArrayField(TEXT("tags"), TagArr);
		Data->SetNumberField(TEXT("count"), TagArr.Num());
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Found %d tags on '%s'"), TagArr.Num(), *ActorName), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPGASTools::GASListRegisteredTags(const TSharedPtr<FJsonObject>& Args)
{
	FString Prefix;
	Args->TryGetStringField(TEXT("prefix"), Prefix);

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [Prefix, &Result]()
	{
		UGameplayTagsManager& TM = UGameplayTagsManager::Get();
		FGameplayTagContainer AllTags;
		TM.RequestAllGameplayTags(AllTags, true);

		TArray<TSharedPtr<FJsonValue>> TagArr;
		for (const FGameplayTag& Tag : AllTags)
		{
			FString TagStr = Tag.ToString();
			if (Prefix.IsEmpty() || TagStr.StartsWith(Prefix))
			{
				TagArr.Add(MakeShared<FJsonValueString>(TagStr));
			}
		}

		auto Data = MakeShared<FJsonObject>();
		Data->SetArrayField(TEXT("tags"), TagArr);
		Data->SetNumberField(TEXT("count"), TagArr.Num());
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Found %d registered tags"), TagArr.Num()), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPGASTools::GASSetAttribute(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName, AttributeName;
	double Value = 0.0;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("actor_name required"));
	if (!Args->TryGetStringField(TEXT("attribute_name"), AttributeName))
		return FMCPToolResult::Error(TEXT("attribute_name required"));
	if (!Args->TryGetNumberField(TEXT("value"), Value))
		return FMCPToolResult::Error(TEXT("value required"));

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, AttributeName, Value, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByName(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no AbilitySystemComponent"), *ActorName)); return; }

		for (const UAttributeSet* AttribSet : ASC->GetSpawnedAttributes())
		{
			if (!AttribSet) continue;
			for (TFieldIterator<FProperty> PropIt(AttribSet->GetClass()); PropIt; ++PropIt)
			{
				FProperty* Prop = *PropIt;
				if (!Prop->GetName().Equals(AttributeName, ESearchCase::IgnoreCase)) continue;

				FStructProperty* StructProp = CastField<FStructProperty>(Prop);
				if (StructProp && StructProp->Struct->GetName() == TEXT("GameplayAttributeData"))
				{
					FGameplayAttributeData* AttrData = StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(const_cast<UAttributeSet*>(AttribSet));
					if (AttrData)
					{
						AttrData->SetBaseValue((float)Value);
						AttrData->SetCurrentValue((float)Value);
						Result = FMCPToolResult::Success(FString::Printf(TEXT("Set attribute '%s' = %f on '%s'"), *AttributeName, (float)Value, *ActorName));
						return;
					}
				}
			}
		}
		Result = FMCPToolResult::Error(FString::Printf(TEXT("Attribute '%s' not found on '%s'"), *AttributeName, *ActorName));
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPGASTools::GASGetAttributes(const TSharedPtr<FJsonObject>& Args)
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

		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no AbilitySystemComponent"), *ActorName)); return; }

		TArray<TSharedPtr<FJsonValue>> AttrArr;
		for (const UAttributeSet* AttribSet : ASC->GetSpawnedAttributes())
		{
			if (!AttribSet) continue;
			for (TFieldIterator<FProperty> PropIt(AttribSet->GetClass()); PropIt; ++PropIt)
			{
				FStructProperty* StructProp = CastField<FStructProperty>(*PropIt);
				if (!StructProp || StructProp->Struct->GetName() != TEXT("GameplayAttributeData")) continue;

				const FGameplayAttributeData* AttrData = StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttribSet);
				if (!AttrData) continue;

				auto Obj = MakeShared<FJsonObject>();
				Obj->SetStringField(TEXT("name"), PropIt->GetName());
				Obj->SetStringField(TEXT("attribute_set"), AttribSet->GetClass()->GetName());
				Obj->SetNumberField(TEXT("base_value"), AttrData->GetBaseValue());
				Obj->SetNumberField(TEXT("current_value"), AttrData->GetCurrentValue());
				AttrArr.Add(MakeShared<FJsonValueObject>(Obj));
			}
		}

		auto Data = MakeShared<FJsonObject>();
		Data->SetArrayField(TEXT("attributes"), AttrArr);
		Data->SetNumberField(TEXT("count"), AttrArr.Num());
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Found %d attributes on '%s'"), AttrArr.Num(), *ActorName), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPGASTools::GASApplyEffect(const TSharedPtr<FJsonObject>& Args)
{
	FString ActorName, EffectClassPath;
	double Level = 1.0;
	if (!Args->TryGetStringField(TEXT("actor_name"), ActorName))
		return FMCPToolResult::Error(TEXT("actor_name required"));
	if (!Args->TryGetStringField(TEXT("effect_class"), EffectClassPath))
		return FMCPToolResult::Error(TEXT("effect_class required"));
	Args->TryGetNumberField(TEXT("level"), Level);

	FMCPToolResult Result;
	AsyncTask(ENamedThreads::GameThread, [ActorName, EffectClassPath, Level, &Result]()
	{
		UWorld* World = GetEditorWorld();
		if (!World) { Result = FMCPToolResult::Error(TEXT("No editor world")); return; }

		AActor* Actor = FindActorByName(World, ActorName);
		if (!Actor) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' not found"), *ActorName)); return; }

		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no AbilitySystemComponent"), *ActorName)); return; }

		UClass* EffectClass = LoadClass<UGameplayEffect>(nullptr, *EffectClassPath);
		if (!EffectClass) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Effect class '%s' not found"), *EffectClassPath)); return; }

		UGameplayEffect* EffectCDO = EffectClass->GetDefaultObject<UGameplayEffect>();
		FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(EffectCDO, (float)Level, ASC->MakeEffectContext());

		auto Data = MakeShared<FJsonObject>();
		Data->SetBoolField(TEXT("applied"), Handle.IsValid());
		Data->SetStringField(TEXT("handle"), Handle.ToString());
		Result = FMCPToolResult::Success(
			FString::Printf(TEXT("Applied effect '%s' to '%s'"), *EffectClassPath, *ActorName), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}

FMCPToolResult FMCPGASTools::GASGetActiveEffects(const TSharedPtr<FJsonObject>& Args)
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

		UAbilitySystemComponent* ASC = GetASC(Actor);
		if (!ASC) { Result = FMCPToolResult::Error(FString::Printf(TEXT("Actor '%s' has no AbilitySystemComponent"), *ActorName)); return; }

		TArray<FActiveGameplayEffectHandle> ActiveHandles = ASC->GetActiveEffects(FGameplayEffectQuery());
		TArray<TSharedPtr<FJsonValue>> EffectArr;
		for (const FActiveGameplayEffectHandle& Handle : ActiveHandles)
		{
			const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(Handle);
			if (!ActiveEffect) continue;

			auto Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("handle"), Handle.ToString());
			Obj->SetStringField(TEXT("effect_class"),
				ActiveEffect->Spec.Def ? ActiveEffect->Spec.Def->GetClass()->GetName() : TEXT("None"));
			Obj->SetNumberField(TEXT("level"), ActiveEffect->Spec.GetLevel());
			Obj->SetNumberField(TEXT("duration"), ActiveEffect->GetDuration());
			EffectArr.Add(MakeShared<FJsonValueObject>(Obj));
		}

		auto Data = MakeShared<FJsonObject>();
		Data->SetArrayField(TEXT("effects"), EffectArr);
		Data->SetNumberField(TEXT("count"), EffectArr.Num());
		Result = FMCPToolResult::Success(FString::Printf(TEXT("Found %d active effects on '%s'"), EffectArr.Num(), *ActorName), Data);
	});
	FPlatformProcess::Sleep(0.05f);
	return Result;
}
