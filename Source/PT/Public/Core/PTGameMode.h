// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "GameFramework/GameModeBase.h"
#include "Online/PTMigrationTypes.h"
#include "PTGameMode.generated.h"

/**
 * 협동 리슨 서버 GameMode.
 *  - 주기적 호스트 인계 스냅샷 생성 (리슨 서버일 때)
 *  - 새 호스트로 시작된 경우 스냅샷 복원 (월드 액터 / 플레이어 위치 / 어트리뷰트 / SaveGame 프로퍼티)
 */
UCLASS(Config = Game)
class PT_API APTGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APTGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;

	/** 스냅샷을 즉시 만들어 GameState 로 복제 */
	UFUNCTION(BlueprintCallable, Category = "PT|Migration")
	void CaptureAndPublishSnapshot();

	UFUNCTION(BlueprintPure, Category = "PT|Migration")
	bool IsRestoringFromMigration() const { return bRestoringFromMigration; }

	UFUNCTION(BlueprintPure, Category = "PT|Migration")
	const FString& GetSessionLineageId() const { return SessionLineageId; }

	UFUNCTION(BlueprintPure, Category = "PT|Migration")
	int32 GetSessionEpoch() const { return SessionEpoch; }

protected:
	/** 스냅샷에 넣을 게임 전역 데이터 (목표 진행도 등). BP 에서 확장 */
	UFUNCTION(BlueprintNativeEvent, Category = "PT|Migration")
	void CaptureMigrationGlobalData(TArray<FPTKeyValue>& OutData);

	/** 새 호스트에서 전역 데이터 복원 */
	UFUNCTION(BlueprintNativeEvent, Category = "PT|Migration")
	void RestoreMigrationGlobalData(const TArray<FPTKeyValue>& Data);

	/** 플레이어 복원 직후 호출 (서버) */
	UFUNCTION(BlueprintImplementableEvent, Category = "PT|Migration")
	void OnPlayerRestoredFromMigration(AController* Controller);

	void BuildSnapshot(FPTMigrationSnapshot& OutSnapshot);
	void RestorePlayerFromSnapshot(AController* Controller);
	void RestoreWorldFromSnapshot();
	void ScheduleSnapshotSoon();

protected:
	/** 리슨 서버에서 스냅샷 생성 여부 */
	UPROPERTY(EditDefaultsOnly, Category = "PT|Migration")
	bool bEnableMigrationSnapshots = true;

	FString SessionLineageId;
	int32 SessionEpoch = 0;
	int32 NextJoinOrder = 0;

	bool bRestoringFromMigration = false;
	FPTMigrationSnapshot RestoreSnapshot;
	TSet<FString> RestoredPlayerIds;

	FTimerHandle SnapshotTimerHandle;
	FTimerHandle SnapshotSoonTimerHandle;
};
