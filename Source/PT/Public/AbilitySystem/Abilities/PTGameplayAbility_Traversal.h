// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "AbilitySystem/PTGameplayAbility.h"
#include "Traversal/PTTraversalTypes.h"
#include "PTGameplayAbility_Traversal.generated.h"

class UPrimitiveComponent;

/**
 * 파쿠르 어빌리티 (Hurdle / Vault / Mantle).
 *
 *  - Event.Traversal 게임플레이 이벤트로 활성화 (UPTTraversalComponent::TryTraversal 이 판정 결과를 TargetData 로 전달)
 *  - LocalPredicted: 클라이언트가 즉시 실행, 서버는 같은 TargetData 를 검증 후 실행 → 양쪽 모션 워핑 타겟 동일
 *  - 실행 중: 이동 모드 Flying (루트 모션으로 수직 이동), 장애물과 캡슐 충돌 무시, 모션 워핑 타겟 설정, 몽타주 재생
 *  - 절차적 모드(몽타주 없음): 구간별 ApplyRootMotionMoveToForce 로 경로 이동 + 점프 애니메이션을 슬롯에 재생
 */
UCLASS()
class PT_API UPTGameplayAbility_Traversal : public UPTGameplayAbility
{
	GENERATED_BODY()

public:
	UPTGameplayAbility_Traversal(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();

	/** 절차적 모드: 다음 경로 구간 시작 (마지막이면 종료) */
	UFUNCTION()
	void StartNextProceduralSegment();

	UFUNCTION()
	void OnProceduralSafetyTimeout();

	bool StartProcedural(APTCharacter* Character);

	void ApplyWarpTargets(const FPTTraversalCheckResult& Result);
	void RestoreMovement();

	UPROPERTY(Transient)
	FPTTraversalCheckResult ActiveResult;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> IgnoredComponent;

	bool bMovementModified = false;

	TArray<FPTTraversalPathSegment> ProceduralSegments;
	FVector ProceduralExitVelocity = FVector::ZeroVector;
	int32 ProceduralSegmentIndex = INDEX_NONE;
};
