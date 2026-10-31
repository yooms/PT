// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PTMigrationTypes.generated.h"

/** 세션/호스트 인계에 사용하는 세션 설정 키 */
namespace PTSessionKeys
{
	/** 호스트가 바뀌어도 유지되는 '게임 계보' ID */
	inline const TCHAR* const Lineage = TEXT("PT_LINEAGE");
	/** 호스트 인계 횟수 (0 = 최초 호스트) */
	inline const TCHAR* const Epoch = TEXT("PT_EPOCH");
	/** 인계 후보 순위 (0 = 1순위) */
	inline const TCHAR* const HostRank = TEXT("PT_HOSTRANK");
	/** 빌드 호환성 확인용 */
	inline const TCHAR* const BuildId = TEXT("PT_BUILD");
	/** 맵 경로 */
	inline const TCHAR* const MapPath = TEXT("PT_MAP");
	/** 호스트 표시 이름 */
	inline const TCHAR* const HostName = TEXT("PT_HOSTNAME");
}

USTRUCT(BlueprintType)
struct FPTKeyValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Migration")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Migration")
	FString Value;
};

USTRUCT(BlueprintType)
struct FPTAttributeSnapshot
{
	GENERATED_BODY()

	/** "<AttributeSetClass>.<Property>" */
	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	FString AttributeName;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	float BaseValue = 0.f;
};

USTRUCT(BlueprintType)
struct FPTPlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	FString PlayerName;

	/** 접속 순서 (호스트 인계 우선순위) */
	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	int32 JoinOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	bool bHasPawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	FTransform PawnTransform;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	FRotator ControlRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	TArray<FPTAttributeSnapshot> Attributes;

	/** PlayerState 의 UPROPERTY(SaveGame) 값 */
	UPROPERTY()
	TArray<uint8> PlayerStateSaveData;

	/** Pawn 의 UPROPERTY(SaveGame) 값 */
	UPROPERTY()
	TArray<uint8> PawnSaveData;
};

USTRUCT(BlueprintType)
struct FPTActorSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	FString ActorName;

	/** 클래스 경로 (동적 스폰된 액터 재생성용) */
	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	FString ActorClassPath;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	FTransform Transform;

	/** 레벨에 배치된 액터인가 (true 면 이름으로 찾고, false 면 새로 스폰) */
	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	bool bLevelPlaced = false;

	/** UPROPERTY(SaveGame) 값 */
	UPROPERTY()
	TArray<uint8> SaveData;
};

/** 호스트 인계를 위해 호스트가 주기적으로 만들어 모든 클라이언트에 복제하는 월드 스냅샷 */
USTRUCT(BlueprintType)
struct FPTMigrationSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	FString SessionLineageId;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	FString HostPlayerId;

	/** 롱 패키지 이름 (예: /Game/PT/Maps/L_Stage01) */
	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	FString MapPackageName;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	int32 MaxPlayers = 4;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	bool bIsLAN = true;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	double ServerWorldTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	TArray<FPTPlayerSnapshot> Players;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	TArray<FPTActorSnapshot> Actors;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Migration")
	TArray<FPTKeyValue> GlobalData;

	/** GameState 의 UPROPERTY(SaveGame) 값 */
	UPROPERTY()
	TArray<uint8> GameStateSaveData;

	bool IsValid() const { return !SessionLineageId.IsEmpty() && !MapPackageName.IsEmpty(); }

	const FPTPlayerSnapshot* FindPlayer(const FString& PlayerId) const
	{
		return Players.FindByPredicate([&PlayerId](const FPTPlayerSnapshot& P) { return P.PlayerId == PlayerId; });
	}

	/** 호스트를 제외한 인계 후보 목록 (JoinOrder 오름차순) */
	TArray<FString> GetSuccessorOrder() const
	{
		TArray<FPTPlayerSnapshot> Sorted = Players;
		Sorted.Sort([](const FPTPlayerSnapshot& A, const FPTPlayerSnapshot& B) { return A.JoinOrder < B.JoinOrder; });
		TArray<FString> Result;
		for (const FPTPlayerSnapshot& P : Sorted)
		{
			if (P.PlayerId != HostPlayerId)
			{
				Result.Add(P.PlayerId);
			}
		}
		return Result;
	}
};

/** 복제 배열 크기 제한(net.MaxRepArraySize, 기본 2048)을 피하기 위한 청크 */
USTRUCT()
struct FPTMigrationChunk
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<uint8> Bytes;
};

/** GameState 로 복제되는 압축 스냅샷 (1KB 청크로 분할) */
USTRUCT()
struct FPTReplicatedMigrationData
{
	GENERATED_BODY()

	static constexpr int32 ChunkSize = 1024;

	UPROPERTY()
	int32 Revision = 0;

	UPROPERTY()
	int32 UncompressedSize = 0;

	UPROPERTY()
	int32 CompressedSize = 0;

	UPROPERTY()
	TArray<FPTMigrationChunk> Chunks;

	void SetPayload(const TArray<uint8>& Compressed)
	{
		CompressedSize = Compressed.Num();
		Chunks.Reset();
		for (int32 Offset = 0; Offset < Compressed.Num(); Offset += ChunkSize)
		{
			const int32 Count = FMath::Min(ChunkSize, Compressed.Num() - Offset);
			Chunks.AddDefaulted_GetRef().Bytes.Append(Compressed.GetData() + Offset, Count);
		}
	}

	/** 모든 청크가 도착했으면 true */
	bool GetPayload(TArray<uint8>& OutCompressed) const
	{
		OutCompressed.Reset(CompressedSize);
		for (const FPTMigrationChunk& Chunk : Chunks)
		{
			OutCompressed.Append(Chunk.Bytes);
		}
		return CompressedSize > 0 && OutCompressed.Num() == CompressedSize;
	}
};

UENUM(BlueprintType)
enum class EPTMigrationState : uint8
{
	Idle,
	/** 연결 끊김 감지 후 기본 맵 로드 대기 */
	WaitingForDisconnectTravel,
	/** 내가 새 호스트가 되는 중 */
	Promoting,
	/** 새 호스트 세션 검색 중 */
	Searching,
	/** 새 호스트에 접속 중 */
	Joining,
	Completed,
	Failed
};

/** 호스트 인계 설정 (Project Settings > Game > PT Host Migration) */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "PT Host Migration"))
class PT_API UPTHostMigrationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category = "Host Migration")
	bool bEnableHostMigration = true;

	/** 호스트가 스냅샷을 갱신/복제하는 주기 (초) */
	UPROPERTY(Config, EditAnywhere, Category = "Host Migration", meta = (ClampMin = "0.5"))
	float SnapshotInterval = 3.f;

	/** N 순위 후보는 (N * 이 값) 초 동안 상위 후보의 세션을 찾지 못하면 스스로 호스트가 됨 */
	UPROPERTY(Config, EditAnywhere, Category = "Host Migration", meta = (ClampMin = "1.0"))
	float CandidatePromotionDelay = 8.f;

	UPROPERTY(Config, EditAnywhere, Category = "Host Migration", meta = (ClampMin = "0.5"))
	float SearchInterval = 2.f;

	/** 전체 인계 제한 시간 (초) - 초과 시 실패 처리 후 메뉴로 */
	UPROPERTY(Config, EditAnywhere, Category = "Host Migration", meta = (ClampMin = "5.0"))
	float MigrationTimeout = 60.f;

	/** 하위 순위 후보가 호스트가 된 뒤 아무도 접속하지 않으면 상위 순위 세션이 있는지 재확인하는 지연 (초) */
	UPROPERTY(Config, EditAnywhere, Category = "Host Migration", meta = (ClampMin = "1.0"))
	float RedundantHostCheckDelay = 10.f;
};
