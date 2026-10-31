// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"
#include "PTShaderWarmupSettings.generated.h"

/** 셰이더(PSO) 워밍업 설정 (Project Settings > Game > PT Shader Warmup) */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "PT Shader Warmup"))
class PT_API UPTShaderWarmupSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category = "Warmup")
	bool bEnableWarmup = true;

	/** 에디터(PIE/-game uncooked)에서도 실행할지 */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup")
	bool bRunInEditor = false;

	/** 진행 화면 최소 표시 시간 (초) */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup", meta = (ClampMin = "0"))
	float MinimumDisplayTime = 1.5f;

	/** 최대 대기 시간 (초). 초과 시 남은 작업은 백그라운드로 계속 */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup", meta = (ClampMin = "5"))
	float MaxWaitTime = 900.f;

	/** UE5 PSO Precache 요청(머티리얼/메시 로드 시 생성)도 완료까지 대기 */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup")
	bool bWaitForPSOPrecache = true;

	/** 런타임에 새 Pak(DLC/패치)이 마운트되면 즉시 다시 워밍업 */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup")
	bool bRewarmOnRuntimePakMount = true;

	/**
	 * 워밍업 중 미리 로드할 에셋 (머티리얼, 메시, 나이아가라 등).
	 * 로드 시 PSO Precache 가 발생하여 첫 등장 시 히치를 줄입니다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup")
	TArray<FSoftObjectPath> WarmupAssets;

	/** 이 값을 바꾸면 모든 유저가 다음 실행 시 다시 워밍업 (강제 재컴파일용) */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup")
	FString FingerprintSalt = TEXT("1");
};
