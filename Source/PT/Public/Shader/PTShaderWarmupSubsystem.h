// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "UI/SPTStatusOverlay.h"
#include "PTShaderWarmupSubsystem.generated.h"

struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPTOnShaderWarmupComplete);

/**
 * 셰이더 / PSO 워밍업.
 *
 *  최초 실행 또는 콘텐츠 변경(빌드 버전, Pak 목록, GPU/드라이버, Salt)이 감지되면
 *  진행 화면을 띄우고 다음을 완료까지 기다립니다.
 *    1) 번들 PSO 캐시(.upipelinecache) 프리컴파일   - FShaderPipelineCache
 *    2) UE5 PSO Precache 요청                       - PipelineStateCache::NumActivePrecacheRequests
 *    3) (uncooked 실행 시) 셰이더 컴파일 작업        - GShaderCompilingManager
 *    4) 설정된 WarmupAssets 비동기 로드
 *
 *  완료 후 Fingerprint 를 GameUserSettings.ini 에 저장 -> 다음 실행부터는 백그라운드 모드로만 동작.
 *  런타임에 Pak 이 추가 마운트되면(DLC/패치) 다시 워밍업합니다.
 */
UCLASS()
class PT_API UPTShaderWarmupSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 워밍업 시작. bForce = true 면 Fingerprint 와 무관하게 실행 */
	UFUNCTION(BlueprintCallable, Category = "PT|Shader")
	void StartWarmup(bool bForce = false);

	UFUNCTION(BlueprintPure, Category = "PT|Shader")
	bool IsWarmupRunning() const { return bRunning; }

	/** 최초 판단이 끝났고 워밍업이 진행 중이 아님 (메뉴에서 Host/Join 버튼 활성화 조건으로 사용) */
	UFUNCTION(BlueprintPure, Category = "PT|Shader")
	bool IsWarmupComplete() const { return bInitialCheckDone && !bRunning; }

	/** 0~1 */
	UFUNCTION(BlueprintPure, Category = "PT|Shader")
	float GetProgress() const;

	UFUNCTION(BlueprintPure, Category = "PT|Shader")
	int32 GetRemainingWork() const { return LastRemaining; }

	/** 현재 환경의 Fingerprint (디버그) */
	UFUNCTION(BlueprintPure, Category = "PT|Shader")
	FString ComputeFingerprint() const;

	UPROPERTY(BlueprintAssignable, Category = "PT|Shader")
	FPTOnShaderWarmupComplete OnWarmupComplete;

protected:
	bool Tick(float DeltaTime);
	void RunInitialCheck();
	void FinishWarmup(bool bTimedOut);
	void HandlePakMounted();
	void HandlePostLoadMap(UWorld* LoadedWorld);
	int32 QueryRemainingWork() const;
	bool CanShowOverlay() const;

	FText GetOverlayTitle() const;
	FText GetOverlayDetail() const;
	TOptional<float> GetOverlayProgress() const;

	FString LoadSavedFingerprint() const;
	void SaveFingerprint(const FString& Fingerprint) const;

protected:
	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle PakMountedHandle;
	FDelegateHandle PostLoadMapHandle;

	TSharedPtr<FStreamableHandle> WarmupAssetsHandle;
	FPTStatusOverlayHandle Overlay;

	bool bInitialCheckDone = false;
	bool bRunning = false;
	bool bPendingRewarm = false;
	double StartTime = 0.0;
	double ZeroRemainingSince = -1.0;
	int32 MaxRemainingSeen = 0;
	int32 LastRemaining = 0;
};
