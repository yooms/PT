// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Traversal/PTTraversalTypes.h"
#include "PTTraversalComponent.generated.h"

class APTCharacter;
class UAnimSequenceBase;
class UPTTraversalAnimSet;

/**
 * 파쿠르(Traversal) 판정 컴포넌트.
 *
 * 점프 입력 시(로컬) 앞의 장애물을 트레이스로 분석해 Hurdle / Vault / Mantle 을 결정하고,
 * 몽타주를 고른 뒤 결과를 TargetData 로 담아 Event.Traversal 게임플레이 이벤트를 보냅니다.
 * → UPTGameplayAbility_Traversal 이 LocalPredicted 로 실행되며 서버에도 같은 데이터가 전달됩니다.
 *
 * 장애물 판정은 전용 액터 없이 일반 지오메트리(스태틱 메시 등)에 대해 동작합니다.
 *
 * AnimSet(GASP 몽타주)이 없거나 맞는 몽타주가 없으면 '절차적 모드'로 동작합니다:
 * 루트 모션 경로(구간별 MoveTo)로 캡슐을 장애물 위/너머로 옮기고 점프 애니메이션을 슬롯에 재생.
 */
UCLASS(ClassGroup = (PT), meta = (BlueprintSpawnableComponent))
class PT_API UPTTraversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPTTraversalComponent();

	/** 로컬: 판정 후 가능하면 파쿠르 어빌리티 실행. 실행했으면 true (일반 점프 대신) */
	UFUNCTION(BlueprintCallable, Category = "PT|Traversal")
	bool TryTraversal();

	/** 장애물 판정 (몽타주 선택 제외) */
	UFUNCTION(BlueprintCallable, Category = "PT|Traversal")
	bool PerformTraversalCheck(FPTTraversalCheckResult& OutResult) const;

	/** 서버: 클라이언트가 보낸 결과가 현재 위치에서 그럴듯한지 확인 */
	bool ValidateResult(const FPTTraversalCheckResult& Result) const;

	UPTTraversalAnimSet* GetAnimSet() const { return AnimSet; }

	/** 절차적 모드: 현재 위치에서 결과까지의 캡슐 이동 경로 + 끝났을 때의 속도 */
	bool BuildProceduralPath(const FPTTraversalCheckResult& Result, TArray<FPTTraversalPathSegment>& OutSegments, FVector& OutExitVelocity) const;

	/** 절차적 모드에서 슬롯에 재생할 애니메이션 (로드되어 있지 않으면 nullptr) */
	UAnimSequenceBase* GetProceduralAnimation(EPTTraversalAction Action) const;

	// ---- 설정 ----

	/** 파쿠르 몽타주 세트 (GASP 몽타주 연결) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal")
	TObjectPtr<UPTTraversalAnimSet> AnimSet;

	/** AnimSet 이 비어 있으면 BeginPlay 에서 로드 (pt.Editor.SetupGASP 가 만드는 에셋) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal")
	TSoftObjectPtr<UPTTraversalAnimSet> DefaultAnimSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	// ---- 절차적 모드 (몽타주 없음) ----

	/** AnimSet 에 맞는 몽타주가 없으면 절차적 경로로 실행 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Procedural")
	bool bUseProceduralFallback = true;

	/** Hurdle / Vault 때 재생할 애니메이션 (기본: 템플릿 마네킹 MM_Jump) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Procedural")
	TSoftObjectPtr<UAnimSequenceBase> ProceduralVaultAnimation;

	/** Mantle 때 재생할 애니메이션 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Procedural")
	TSoftObjectPtr<UAnimSequenceBase> ProceduralMantleAnimation;

	/** 애니메이션을 재생할 AnimBP 슬롯 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Procedural")
	FName ProceduralSlotName = TEXT("DefaultSlot");

	/** 장애물 위를 지날 때 발이 윗면보다 내려가도 되는 양 (다리를 접는 만큼, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Procedural", meta = (ClampMin = "0"))
	float ProceduralFeetTuck = 20.f;

	/** 이동 속도 하한 (cm/s). 실제로는 판정 당시 속도와 이 값 중 큰 값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Procedural", meta = (ClampMin = "50"))
	float ProceduralHurdleSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Procedural", meta = (ClampMin = "50"))
	float ProceduralVaultSpeed = 350.f;

	/** Mantle 상승 속도 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Procedural", meta = (ClampMin = "50"))
	float ProceduralMantleSpeed = 320.f;

	/** 구간 최소 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Procedural", meta = (ClampMin = "0.05"))
	float ProceduralMinSegmentTime = 0.12f;

	/** 끝난 뒤 앞으로 유지할 속도 비율 (Hurdle / Vault). Mantle 은 정지 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Procedural", meta = (ClampMin = "0", ClampMax = "1"))
	float ProceduralHurdleExitSpeedRatio = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Procedural", meta = (ClampMin = "0", ClampMax = "1"))
	float ProceduralVaultExitSpeedRatio = 0.6f;

	/** 정지 시 / 최고 속도 시 앞쪽 탐색 거리 (cm) - 빠를수록 멀리서 반응 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Detection")
	float MinTraceDistance = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Detection")
	float MaxTraceDistance = 350.f;

	/** MaxTraceDistance 에 도달하는 속도 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Detection")
	float TraceDistanceSpeed = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Detection")
	float TraceRadius = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Detection")
	float TraceHalfHeight = 60.f;

	/** 넘을 수 있는 장애물 높이 (발 기준, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Rules")
	float MinObstacleHeight = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Rules")
	float MaxMantleHeight = 275.f;

	/** Vault / Hurdle 가 가능한 최대 높이 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Rules")
	float MaxVaultHeight = 125.f;

	/** 이 깊이 이하이면 '얇은' 장애물 (넘기 가능), 초과면 올라타기 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Rules")
	float MaxVaultDepth = 60.f;

	/** 윗면을 따라 뒤쪽 끝을 찾는 최대 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Rules")
	float MaxDepthCheck = 250.f;

	/** 이 속도 이상이고 뒤쪽 바닥이 크게 낮지 않으면 Vault 대신 Hurdle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Rules")
	float MinHurdleSpeed = 300.f;

	/** 뒤쪽 바닥이 이보다 더 낮으면 Hurdle 대신 Vault (cm, 음수) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Rules")
	float MaxHurdleDrop = -50.f;

	/** 뒤쪽 바닥을 찾는 최대 낙차 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Rules")
	float MaxBackFloorDrop = 200.f;

	/** 서버 검증: 앞 모서리까지 허용 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PT|Traversal|Rules")
	float ServerValidateDistance = 450.f;

protected:
	virtual void BeginPlay() override;

private:
	APTCharacter* GetPTCharacter() const;
	bool TraceDown(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	bool CapsuleFits(const FVector& Center, float Radius, float HalfHeight) const;

	/** 소프트 참조 애니메이션을 로드해 붙잡아 둠 (GC 방지) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequenceBase>> LoadedProceduralAnimations;
};
