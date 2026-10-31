// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "PTTraversalTypes.generated.h"

class UAnimMontage;
class UPrimitiveComponent;

/** 파쿠르 동작 (Game Animation Sample 의 Traversal 과 동일한 3종) */
UENUM(BlueprintType)
enum class EPTTraversalAction : uint8
{
	None,
	/** 달리면서 낮고 얇은 장애물을 뛰어넘기 */
	Hurdle,
	/** 낮고 얇은 장애물을 손으로 짚고 넘기 */
	Vault,
	/** 높은 곳 / 깊은 장애물 위로 올라타기 */
	Mantle
};

/** 장애물 판정 결과 + 선택된 몽타주 (클라이언트가 계산해 서버로 그대로 전달 → 양쪽이 같은 워프 타겟 사용) */
USTRUCT(BlueprintType)
struct PT_API FPTTraversalCheckResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	EPTTraversalAction Action = EPTTraversalAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	bool bHasFrontLedge = false;

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	FVector FrontLedgeLocation = FVector::ZeroVector;

	/** 장애물 앞면 바깥쪽 법선 (수평) */
	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	FVector FrontLedgeNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	bool bHasBackLedge = false;

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	FVector BackLedgeLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	bool bHasBackFloor = false;

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	FVector BackFloorLocation = FVector::ZeroVector;

	/** 발 높이 기준 장애물 윗면 높이 (cm) */
	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	float ObstacleHeight = 0.f;

	/** 장애물 앞~뒤 깊이 (cm). 뒤쪽 끝이 없으면 0 */
	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	float ObstacleDepth = 0.f;

	/** 발 높이 기준 뒤쪽 바닥 높이 (cm, 아래면 음수) */
	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	float BackFloorHeight = 0.f;

	/** 판정 당시 수평 속도 */
	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	float Speed = 0.f;

	/** 통과하는 동안 캡슐 충돌을 무시할 장애물 */
	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	TObjectPtr<UPrimitiveComponent> HitComponent = nullptr;

	/** 재생할 몽타주와 시작 시간 */
	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	float MontageStartTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	float MontagePlayRate = 1.f;

	/** 몽타주 없이 루트 모션 경로(절차적)로 실행 - GASP 몽타주가 아직 없을 때의 대체 동작 */
	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	bool bProcedural = false;

	bool IsValid() const { return Action != EPTTraversalAction::None && bHasFrontLedge && (Montage != nullptr || bProcedural); }

	FRotator GetWarpRotation() const { return (-FrontLedgeNormal).Rotation(); }

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FPTTraversalCheckResult> : public TStructOpsTypeTraitsBase2<FPTTraversalCheckResult>
{
	enum { WithNetSerializer = true };
};

/** 절차적 파쿠르 경로의 한 구간 (캡슐 중심 목표 위치 + 소요 시간) */
struct FPTTraversalPathSegment
{
	FVector Target = FVector::ZeroVector;
	float Duration = 0.f;
};

/** 파쿠르 어빌리티 활성화 이벤트에 실어 보내는 TargetData */
USTRUCT(BlueprintType)
struct PT_API FPTGameplayAbilityTargetData_Traversal : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	FPTTraversalCheckResult Result;

	virtual UScriptStruct* GetScriptStruct() const override { return FPTGameplayAbilityTargetData_Traversal::StaticStruct(); }
	virtual FString ToString() const override { return TEXT("FPTGameplayAbilityTargetData_Traversal"); }

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		return Result.NetSerialize(Ar, Map, bOutSuccess);
	}
};

template<>
struct TStructOpsTypeTraits<FPTGameplayAbilityTargetData_Traversal> : public TStructOpsTypeTraitsBase2<FPTGameplayAbilityTargetData_Traversal>
{
	enum { WithNetSerializer = true };
};
