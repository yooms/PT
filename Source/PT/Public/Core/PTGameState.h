// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "GameFramework/GameStateBase.h"
#include "Online/PTMigrationTypes.h"
#include "PTGameState.generated.h"

/** 호스트 인계 스냅샷을 모든 클라이언트에 복제하는 GameState */
UCLASS()
class PT_API APTGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	APTGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 서버: 압축 스냅샷 갱신 */
	void SetMigrationPayload(TArray<uint8>&& Compressed, int32 UncompressedSize);

	/** 서버 -> 전체: 호스트가 곧 떠남 (bAllowMigration=false 면 세션 종료로 처리) */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastHostLeaving(bool bAllowMigration);

	const FPTReplicatedMigrationData& GetMigrationData() const { return MigrationData; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_MigrationData();

	UPROPERTY(ReplicatedUsing = OnRep_MigrationData)
	FPTReplicatedMigrationData MigrationData;
};
