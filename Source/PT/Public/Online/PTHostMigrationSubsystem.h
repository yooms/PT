// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/TimerHandle.h"
#include "Online/PTMigrationTypes.h"
#include "UI/SPTStatusOverlay.h"
#include "PTHostMigrationSubsystem.generated.h"

class FTimerManager;
class UNetDriver;
class UPTSessionSubsystem;
class FOnlineSessionSearchResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPTOnMigrationStateChanged, EPTMigrationState, NewState);

/**
 * 리슨 서버 호스트 인계.
 *
 * [호스트]  GameMode 가 SnapshotInterval 마다 월드 스냅샷을 만들어 GameState 로 모든 클라이언트에 복제
 * [클라]    스냅샷을 로컬에 보관 → 호스트 연결이 끊기면(NetworkFailure) 인계 시작
 *           1) 후보 순위 계산 (JoinOrder 순, 기존 호스트 제외) - 모든 클라이언트가 같은 결과를 얻음
 *           2) 1순위: 같은 맵을 listen 으로 열고 세션 생성(같은 Lineage, Epoch+1) → GameMode 가 스냅샷 복원
 *           3) 나머지: 세션 검색으로 새 호스트(Lineage 일치, Epoch 증가)를 찾아 접속
 *              N 순위는 N*CandidatePromotionDelay 동안 못 찾으면 스스로 호스트가 됨 (상위 후보 동시 이탈 대비)
 */
UCLASS()
class PT_API UPTHostMigrationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- GameState / PlayerController 에서 호출 ----
	void OnSnapshotReplicated(const FPTReplicatedMigrationData& Data);
	void NotifyHostLeaving(bool bAllowMigration);
	void MarkIntentionalLeave();
	void NotifyKicked();

	// ---- GameMode 에서 호출 (새 호스트) ----
	bool HasPendingRestore() const { return bHasPendingRestore; }
	bool ConsumePendingRestore(FPTMigrationSnapshot& OutSnapshot);

	// ---- 조회 ----
	UFUNCTION(BlueprintPure, Category = "PT|Migration")
	EPTMigrationState GetMigrationState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "PT|Migration")
	bool HasValidSnapshot() const { return LastSnapshot.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "PT|Migration")
	int32 GetLocalCandidateRank() const { return LocalRank; }

	UFUNCTION(BlueprintPure, Category = "PT|Migration")
	const FPTMigrationSnapshot& GetLastSnapshot() const { return LastSnapshot; }

	UPROPERTY(BlueprintAssignable, Category = "PT|Migration")
	FPTOnMigrationStateChanged OnMigrationStateChanged;

	/** 디버그: 현재 스냅샷 기준으로 인계 흐름 강제 시작 (클라이언트) */
	void DebugForceMigration();

protected:
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	void BeginMigration(UWorld* FailedWorld);
	void CaptureLiveWorldState(UWorld* World);
	void StartMigrationFlow();
	void PromoteSelf();
	void StartSearching();
	void DoSearch();
	void HandleSearchResults(bool bSuccess, const TArray<FOnlineSessionSearchResult>& Results);
	void JoinNewHost(const FOnlineSessionSearchResult& Result);
	void CheckRedundantHost();
	void CompleteMigration();
	void FailMigration(const FString& Reason);

	void SetState(EPTMigrationState NewState);
	void ResetTransientFlags();
	void UpdateOverlay();

	bool GetMigrationSessionRank(const FOnlineSessionSearchResult& Result, int32 MinEpoch, int32& OutRank, int32& OutEpoch) const;
	double GetElapsed() const;
	UPTSessionSubsystem* GetSessionSubsystem() const;
	FTimerManager* GetTimerManager() const;

	FText GetOverlayTitle() const;
	FText GetOverlayDetail() const;

protected:
	EPTMigrationState State = EPTMigrationState::Idle;

	/** 마지막으로 받은 호스트 스냅샷 */
	FPTMigrationSnapshot LastSnapshot;
	int32 LastRevision = -1;

	/** 새 호스트가 복원할 스냅샷 */
	FPTMigrationSnapshot PendingRestore;
	bool bHasPendingRestore = false;

	/** 인계 진행 중 사용하는 스냅샷 사본 */
	FPTMigrationSnapshot ActiveSnapshot;
	int32 LocalRank = INDEX_NONE;
	int32 NumCandidates = 0;
	double MigrationStartTime = 0.0;
	FString FailureReason;

	bool bHostAnnouncedLeave = false;
	bool bHostAllowsMigration = true;
	bool bIntentionalLeave = false;
	bool bWasKicked = false;

	FTimerHandle SearchTimerHandle;
	FTimerHandle TimeoutTimerHandle;
	FTimerHandle TravelSafetyTimerHandle;
	FTimerHandle RedundantHostTimerHandle;
	FTimerHandle FailedOverlayTimerHandle;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoadMapHandle;

	FPTStatusOverlayHandle Overlay;
};
