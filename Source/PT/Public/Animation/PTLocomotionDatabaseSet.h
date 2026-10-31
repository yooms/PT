// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Character/PTLocomotionTypes.h"
#include "PTLocomotionDatabaseSet.generated.h"

class UPoseSearchDatabase;

/** 모션 매칭 검색 조건 (AnimInstance 가 매 프레임 계산) */
USTRUCT(BlueprintType)
struct FPTLocomotionQuery
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PT|MotionMatching")
	EPTMovementState MovementState = EPTMovementState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "PT|MotionMatching")
	EPTGait Gait = EPTGait::Run;

	UPROPERTY(BlueprintReadOnly, Category = "PT|MotionMatching")
	EPTStance Stance = EPTStance::Stand;

	UPROPERTY(BlueprintReadOnly, Category = "PT|MotionMatching")
	EPTMovementSituation Situation = EPTMovementSituation::OnGround;

	UPROPERTY(BlueprintReadOnly, Category = "PT|MotionMatching")
	EPTRotationMode RotationMode = EPTRotationMode::OrientToMovement;

	bool operator==(const FPTLocomotionQuery& Other) const
	{
		return MovementState == Other.MovementState && Gait == Other.Gait && Stance == Other.Stance
			&& Situation == Other.Situation && RotationMode == Other.RotationMode;
	}
	bool operator!=(const FPTLocomotionQuery& Other) const { return !(*this == Other); }
};

/**
 * 조건 -> PoseSearch Database 매핑 규칙. 필터 배열이 비어 있으면 '모두 허용'.
 * (GASP 의 Chooser Table 역할을 C++ 데이터로 대체)
 */
USTRUCT(BlueprintType)
struct FPTDatabaseRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	FName RuleName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter")
	TArray<EPTMovementState> MovementStates;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter")
	TArray<EPTGait> Gaits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter")
	TArray<EPTStance> Stances;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter")
	TArray<EPTMovementSituation> Situations;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter")
	TArray<EPTRotationMode> RotationModes;

	/** 조건이 맞을 때 검색할 데이터베이스들 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Databases")
	TArray<TObjectPtr<UPoseSearchDatabase>> Databases;

	/** true 면 이 규칙이 맞을 때 이후 규칙은 평가하지 않음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	bool bStopEvaluation = false;

	bool Matches(const FPTLocomotionQuery& Q) const
	{
		return (MovementStates.Num() == 0 || MovementStates.Contains(Q.MovementState))
			&& (Gaits.Num() == 0 || Gaits.Contains(Q.Gait))
			&& (Stances.Num() == 0 || Stances.Contains(Q.Stance))
			&& (Situations.Num() == 0 || Situations.Contains(Q.Situation))
			&& (RotationModes.Num() == 0 || RotationModes.Contains(Q.RotationMode));
	}
};

/**
 * 모션 매칭 데이터베이스 선택 규칙 세트.
 * 위에서부터 평가하며 맞는 규칙의 Databases 를 모두 합쳐 검색합니다.
 */
UCLASS(BlueprintType, Const)
class PT_API UPTLocomotionDatabaseSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 스레드 세이프 (워커 스레드에서 호출) */
	void SelectDatabases(const FPTLocomotionQuery& Query, TArray<UPoseSearchDatabase*>& OutDatabases) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (TitleProperty = "RuleName"))
	TArray<FPTDatabaseRule> Rules;

	/** 어떤 규칙도 맞지 않을 때 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	TArray<TObjectPtr<UPoseSearchDatabase>> FallbackDatabases;
};
