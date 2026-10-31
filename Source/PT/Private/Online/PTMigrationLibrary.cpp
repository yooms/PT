// Copyright NeedsGames. All Rights Reserved.

#include "Online/PTMigrationLibrary.h"
#include "Online/PTMigratableInterface.h"
#include "PTLogChannels.h"

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "Misc/Compression.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTMigrationLibrary)

namespace PTMigrationPrivate
{
	static constexpr uint32 SnapshotMagic = 0x50544D53; // 'PTMS'
	static constexpr int32 SnapshotVersion = 1;
	static constexpr int32 MaxUncompressedSize = 8 * 1024 * 1024;
}

bool UPTMigrationLibrary::SerializeSnapshot(const FPTMigrationSnapshot& Snapshot, TArray<uint8>& OutCompressed, int32& OutUncompressedSize)
{
	using namespace PTMigrationPrivate;

	TArray<uint8> Raw;
	{
		FMemoryWriter Writer(Raw, true);
		FObjectAndNameAsStringProxyArchive Ar(Writer, false);

		uint32 Magic = SnapshotMagic;
		int32 Version = SnapshotVersion;
		Ar << Magic;
		Ar << Version;
		FPTMigrationSnapshot::StaticStruct()->SerializeBin(Ar, const_cast<FPTMigrationSnapshot*>(&Snapshot));
	}

	OutUncompressedSize = Raw.Num();

	int32 CompressedSize = FCompression::CompressMemoryBound(NAME_Zlib, Raw.Num());
	OutCompressed.SetNumUninitialized(CompressedSize);
	if (!FCompression::CompressMemory(NAME_Zlib, OutCompressed.GetData(), CompressedSize, Raw.GetData(), Raw.Num()))
	{
		UE_LOG(LogPTMigration, Error, TEXT("SerializeSnapshot: compression failed"));
		OutCompressed.Reset();
		return false;
	}
	OutCompressed.SetNum(CompressedSize);
	return true;
}

bool UPTMigrationLibrary::DeserializeSnapshot(const TArray<uint8>& Compressed, int32 UncompressedSize, FPTMigrationSnapshot& OutSnapshot)
{
	using namespace PTMigrationPrivate;

	if (Compressed.Num() == 0 || UncompressedSize <= 0 || UncompressedSize > MaxUncompressedSize)
	{
		return false;
	}

	TArray<uint8> Raw;
	Raw.SetNumUninitialized(UncompressedSize);
	if (!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), UncompressedSize, Compressed.GetData(), Compressed.Num()))
	{
		UE_LOG(LogPTMigration, Error, TEXT("DeserializeSnapshot: decompression failed"));
		return false;
	}

	FMemoryReader Reader(Raw, true);
	FObjectAndNameAsStringProxyArchive Ar(Reader, true);

	uint32 Magic = 0;
	int32 Version = 0;
	Ar << Magic;
	Ar << Version;
	if (Magic != SnapshotMagic || Version != SnapshotVersion)
	{
		UE_LOG(LogPTMigration, Error, TEXT("DeserializeSnapshot: bad header (magic %08x, version %d)"), Magic, Version);
		return false;
	}

	FPTMigrationSnapshot Temp;
	FPTMigrationSnapshot::StaticStruct()->SerializeBin(Ar, &Temp);
	if (Ar.IsError())
	{
		return false;
	}
	OutSnapshot = MoveTemp(Temp);
	return true;
}

void UPTMigrationLibrary::SaveGameProperties(UObject* Object, TArray<uint8>& OutData)
{
	OutData.Reset();
	if (!Object)
	{
		return;
	}

	FMemoryWriter Writer(OutData, true);
	FObjectAndNameAsStringProxyArchive Ar(Writer, false);
	Ar.ArIsSaveGame = true;
	Ar.ArNoDelta = true;
	Object->Serialize(Ar);
}

void UPTMigrationLibrary::LoadGameProperties(UObject* Object, const TArray<uint8>& Data)
{
	if (!Object || Data.Num() == 0)
	{
		return;
	}

	FMemoryReader Reader(Data, true);
	FObjectAndNameAsStringProxyArchive Ar(Reader, true);
	Ar.ArIsSaveGame = true;
	Ar.ArNoDelta = true;
	Object->Serialize(Ar);
}

void UPTMigrationLibrary::CaptureAttributes(const UAbilitySystemComponent* ASC, TArray<FPTAttributeSnapshot>& OutAttributes)
{
	OutAttributes.Reset();
	if (!ASC)
	{
		return;
	}

	for (const UAttributeSet* Set : ASC->GetSpawnedAttributes())
	{
		if (!Set)
		{
			continue;
		}

		for (TFieldIterator<FProperty> It(Set->GetClass()); It; ++It)
		{
			FProperty* Property = *It;
			if (!FGameplayAttribute::IsGameplayAttributeDataProperty(Property))
			{
				continue;
			}

			const FGameplayAttribute Attribute(Property);
			FPTAttributeSnapshot& Entry = OutAttributes.AddDefaulted_GetRef();
			Entry.AttributeName = FString::Printf(TEXT("%s.%s"), *Set->GetClass()->GetName(), *Property->GetName());
			Entry.BaseValue = ASC->GetNumericAttributeBase(Attribute);
		}
	}

	// Max* 를 먼저 복원해야 클램프가 올바르게 동작
	OutAttributes.StableSort([](const FPTAttributeSnapshot& A, const FPTAttributeSnapshot& B)
	{
		const bool bAMax = A.AttributeName.Contains(TEXT(".Max"));
		const bool bBMax = B.AttributeName.Contains(TEXT(".Max"));
		return bAMax && !bBMax;
	});
}

void UPTMigrationLibrary::RestoreAttributes(UAbilitySystemComponent* ASC, const TArray<FPTAttributeSnapshot>& Attributes)
{
	if (!ASC || Attributes.Num() == 0)
	{
		return;
	}

	TMap<FString, FGameplayAttribute> Lookup;
	for (const UAttributeSet* Set : ASC->GetSpawnedAttributes())
	{
		if (!Set)
		{
			continue;
		}
		for (TFieldIterator<FProperty> It(Set->GetClass()); It; ++It)
		{
			if (FGameplayAttribute::IsGameplayAttributeDataProperty(*It))
			{
				Lookup.Add(FString::Printf(TEXT("%s.%s"), *Set->GetClass()->GetName(), *It->GetName()), FGameplayAttribute(*It));
			}
		}
	}

	for (const FPTAttributeSnapshot& Entry : Attributes)
	{
		if (const FGameplayAttribute* Attribute = Lookup.Find(Entry.AttributeName))
		{
			ASC->SetNumericAttributeBase(*Attribute, Entry.BaseValue);
		}
		else
		{
			UE_LOG(LogPTMigration, Warning, TEXT("RestoreAttributes: attribute [%s] not found on ASC"), *Entry.AttributeName);
		}
	}
}

void UPTMigrationLibrary::CaptureMigratableActors(UWorld* World, TArray<FPTActorSnapshot>& OutActors)
{
	OutActors.Reset();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || !Actor->Implements<UPTMigratableInterface>())
		{
			continue;
		}
		if (!IPTMigratableInterface::Execute_ShouldCaptureForMigration(Actor))
		{
			continue;
		}

		FPTActorSnapshot& Entry = OutActors.AddDefaulted_GetRef();
		Entry.ActorName = Actor->GetFName().ToString();
		Entry.ActorClassPath = Actor->GetClass()->GetPathName();
		Entry.Transform = Actor->GetActorTransform();
		Entry.bLevelPlaced = Actor->IsNetStartupActor();
		SaveGameProperties(Actor, Entry.SaveData);
	}
}

void UPTMigrationLibrary::RestoreMigratableActors(UWorld* World, const TArray<FPTActorSnapshot>& Actors)
{
	if (!World)
	{
		return;
	}

	TSet<FString> CapturedLevelActorNames;
	for (const FPTActorSnapshot& Entry : Actors)
	{
		if (Entry.bLevelPlaced)
		{
			CapturedLevelActorNames.Add(Entry.ActorName);
		}
	}

	// 1) 레벨 배치 액터 수집, 인계 이전에 이미 파괴되었던 액터는 제거
	TMap<FString, AActor*> LevelActors;
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || !Actor->Implements<UPTMigratableInterface>() || !Actor->IsNetStartupActor())
		{
			continue;
		}

		const FString Name = Actor->GetFName().ToString();
		if (CapturedLevelActorNames.Contains(Name))
		{
			LevelActors.Add(Name, Actor);
		}
		else if (IPTMigratableInterface::Execute_ShouldCaptureForMigration(Actor))
		{
			ToDestroy.Add(Actor);
		}
	}
	for (AActor* Actor : ToDestroy)
	{
		Actor->Destroy();
	}

	// 2) 복원 / 재생성
	for (const FPTActorSnapshot& Entry : Actors)
	{
		AActor* Actor = nullptr;
		if (Entry.bLevelPlaced)
		{
			Actor = LevelActors.FindRef(Entry.ActorName);
		}
		else
		{
			UClass* ActorClass = LoadClass<AActor>(nullptr, *Entry.ActorClassPath);
			if (ActorClass)
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Actor = World->SpawnActor<AActor>(ActorClass, Entry.Transform, Params);
			}
		}

		if (!Actor)
		{
			UE_LOG(LogPTMigration, Warning, TEXT("RestoreMigratableActors: could not restore [%s] (%s)"), *Entry.ActorName, *Entry.ActorClassPath);
			continue;
		}

		Actor->SetActorTransform(Entry.Transform, false, nullptr, ETeleportType::TeleportPhysics);
		LoadGameProperties(Actor, Entry.SaveData);
		IPTMigratableInterface::Execute_OnMigrationRestored(Actor);
	}
}

FString UPTMigrationLibrary::GetCurrentMapPackageName(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FString();
	}
	return UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
}

FString UPTMigrationLibrary::GetBuildId()
{
	FString ProjectVersion;
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), ProjectVersion, GGameIni);
	return FString::Printf(TEXT("%s-%u"), *ProjectVersion, FEngineVersion::Current().GetChangelist());
}
