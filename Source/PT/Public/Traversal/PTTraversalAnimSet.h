// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Character/PTLocomotionTypes.h"
#include "Traversal/PTTraversalTypes.h"
#include "PTTraversalAnimSet.generated.h"

class UAnimInstance;
class UAnimMontage;

/** 파쿠르 몽타주 후보 (Game Animation Sample 의 CHT_TraversalMontages 역할) */
USTRUCT(BlueprintType)
struct FPTTraversalAnimEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	EPTTraversalAction Action = EPTTraversalAction::Vault;

	/** 모션 워핑 노티파이(FrontLedge / BackLedge / BackFloor)가 들어 있는 루트 모션 몽타주 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** 이 몽타주를 쓸 장애물 높이 범위 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float MinHeight = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float MaxHeight = 275.f;

	/** 비어 있으면 모든 Gait */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	TArray<EPTGait> Gaits;

	/** 판정 당시 수평 속도 범위 (cm/s) - GASP 의 stand / walk / run 몽타주 구분 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal", meta = (ClampMin = "0"))
	float MinSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal", meta = (ClampMin = "0"))
	float MaxSpeed = 10000.f;

	/** true 면 공중(낙하 중)에서만, false 면 지상에서만 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	bool bInAir = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal", meta = (ClampMin = "0.1"))
	float PlayRate = 1.f;
};

/**
 * 파쿠르 몽타주 세트.
 * 조건(동작/높이/Gait/공중)에 맞는 후보들 중에서, 애님 BP 에 Pose History 노드가 있으면
 * 모션 매칭으로 현재 포즈와 가장 잘 이어지는 몽타주 + 시작 시간을 고르고, 없으면 높이가 가장 가까운 후보를 고릅니다.
 */
UCLASS(BlueprintType, Const)
class PT_API UPTTraversalAnimSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 결과를 OutResult.Montage / MontageStartTime / MontagePlayRate 에 기록 */
	bool SelectMontage(UAnimInstance* AnimInstance, EPTGait Gait, bool bInAir, FPTTraversalCheckResult& InOutResult) const;

	bool ContainsMontage(const UAnimMontage* Montage) const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal", meta = (TitleProperty = "Montage"))
	TArray<FPTTraversalAnimEntry> Entries;

	/** 모션 매칭으로 시작 포즈 선택 (애님 BP 의 Pose History 노드 태그) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching")
	bool bUseMotionMatching = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Matching", meta = (EditCondition = "bUseMotionMatching"))
	FName PoseHistoryName = TEXT("PoseHistory");

	/** 몽타주 안의 모션 워핑 타겟 이름 (GASP 기본값) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
	FName FrontLedgeWarpName = TEXT("FrontLedge");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
	FName BackLedgeWarpName = TEXT("BackLedge");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
	FName BackFloorWarpName = TEXT("BackFloor");
};
