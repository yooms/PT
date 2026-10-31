// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Engine/EngineTypes.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Animation/TrajectoryTypes.h"
#include "Animation/PTLocomotionDatabaseSet.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "PTMotionMatchingAnimInstance.generated.h"

class APTCharacter;
class UPTCharacterMovementComponent;
class UPoseSearchDatabase;
class UAnimSequenceBase;
class UAnimMontage;

/** 게임 스레드에서 수집해 워커 스레드로 넘기는 캐릭터 상태 */
USTRUCT(BlueprintType)
struct FPTLocomotionInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	FTransform ActorTransform;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	float MaxAcceleration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	EPTGait Gait = EPTGait::Run;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	EPTStance Stance = EPTStance::Stand;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	EPTRotationMode RotationMode = EPTRotationMode::OrientToMovement;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	bool bIsLocallyControlled = false;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	FRotator ControlRotation = FRotator::ZeroRotator;

	/** 루트 모션 몽타주 재생 중 또는 파쿠르 중(Flying) - 제자리 회전/에임 오프셋 비활성 */
	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	bool bIsPlayingRootMotionMontage = false;

	/** 전력질주 정지 슬라이드 중 */
	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	bool bIsSprintStopSliding = false;

	/** 조준 회전 (원격 캐릭터도 복제값 사용) */
	UPROPERTY(BlueprintReadOnly, Category = "PT|Locomotion")
	FRotator AimRotation = FRotator::ZeroRotator;
};

/**
 * C++ 모션 매칭 AnimInstance.
 *
 * 매 프레임:
 *  [Game Thread]   캐릭터/무브먼트 상태 수집 (FPTLocomotionInput)
 *  [Worker Thread] 궤적(Trajectory) 생성 → 이동 상태 판정 → 검색 조건(FPTLocomotionQuery) 계산
 *  [Anim Node]     Motion Matching 노드의 OnUpdate 에서 UpdateMotionMatching() 호출
 *                   → DatabaseSet 규칙으로 검색할 Database 선택 + Interrupt 모드 결정
 *
 * AnimBP 구성 (부모 클래스 = PTMotionMatchingAnimInstance):
 *   Pose History (Trajectory 핀 = Trajectory 변수)  →  Motion Matching (On Update = UpdateMotionMatching,
 *   On Post Selection(옵션) = UpdateMotionMatchingPostSelection)  →  Output Pose
 *   자세한 내용은 Docs/MotionMatching.md
 */
UCLASS(Transient, Blueprintable)
class PT_API UPTMotionMatchingAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPTMotionMatchingAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	/** Motion Matching 노드의 "On Update" 에 바인딩 */
	UFUNCTION(BlueprintCallable, Category = "PT|MotionMatching", meta = (BlueprintThreadSafe))
	void UpdateMotionMatching(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	/** Motion Matching 노드의 "On Post Selection"(또는 On Update 이후)에 바인딩 - 선택 결과 기록 */
	UFUNCTION(BlueprintCallable, Category = "PT|MotionMatching", meta = (BlueprintThreadSafe))
	void UpdateMotionMatchingPostSelection(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	UFUNCTION(BlueprintPure, Category = "PT|MotionMatching", meta = (BlueprintThreadSafe))
	float GetMotionMatchingBlendTime() const;

	UFUNCTION(BlueprintPure, Category = "PT|MotionMatching", meta = (BlueprintThreadSafe))
	bool IsMoving() const { return Query.MovementState == EPTMovementState::Moving; }

	UFUNCTION(BlueprintPure, Category = "PT|MotionMatching", meta = (BlueprintThreadSafe))
	bool IsInAir() const { return Query.Situation == EPTMovementSituation::InAir; }

	/** 현재 선택된 데이터베이스에 태그가 있는지 (GASP 의 "Pivots", "Starts" 등 태그 기반 로직용) */
	UFUNCTION(BlueprintPure, Category = "PT|MotionMatching", meta = (BlueprintThreadSafe))
	bool CurrentDatabaseHasTag(FName Tag) const { return CurrentDatabaseTags.Contains(Tag); }

protected:
	void UpdateTrajectory(float DeltaSeconds);
	void UpdateLocomotionState(float DeltaSeconds);
	void UpdateAimOffset(float DeltaSeconds);
	void UpdateTurnInPlace(float DeltaSeconds);
	void UpdateBodyLean(float DeltaSeconds);
	/** 게임 스레드: 전력질주 애니 속도 / 정지 슬라이드 애니메이션 */
	void UpdateSprintPresentation(float DeltaSeconds);
	/** 게임 스레드: 발 아래 바닥 트레이스 → PT Foot IK 노드 입력 */
	void UpdateFootIK(float DeltaSeconds);

public:
	// ---------------- 설정 ----------------

	/** 상태 -> 데이터베이스 선택 규칙 (필수) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|MotionMatching")
	TObjectPtr<UPTLocomotionDatabaseSet> DatabaseSet;

	/** 정지 상태용 궤적 생성 파라미터 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Trajectory")
	FPoseSearchTrajectoryData TrajectoryDataIdle;

	/** 이동 상태용 궤적 생성 파라미터 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Trajectory")
	FPoseSearchTrajectoryData TrajectoryDataMoving;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Trajectory", meta = (ClampMin = "0.001"))
	float HistorySamplingInterval = 0.04f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Trajectory", meta = (ClampMin = "2"))
	int32 HistorySampleCount = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Trajectory", meta = (ClampMin = "0.001"))
	float PredictionSamplingInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Trajectory", meta = (ClampMin = "2"))
	int32 PredictionSampleCount = 8;

	/** 이 속도(cm/s) 이상 + 가속 입력이 있으면 Moving */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|MotionMatching")
	float MovingSpeedThreshold = 5.f;

	/** 미래 궤적 속도가 이 값 이상이면 Moving (정지 직전 감속 구간 처리) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|MotionMatching")
	float FutureSpeedThreshold = 30.f;

	/** 상황별 블렌드 시간 -> MotionMatchingBlendTime 변수로 계산됨 (MM 노드 Blend Time 핀에 바인딩) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|MotionMatching")
	float GroundBlendTime = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|MotionMatching")
	float InAirBlendTime = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|MotionMatching")
	float LandingBlendTime = 0.2f;

	/** 착지 시 하강 속도가 이 값(cm/s) 이상이면 LandHeavy, 아니면 LandLight */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|MotionMatching", meta = (ClampMin = "0"))
	float HeavyLandSpeed = 700.f;

	/** 착지 상황(LandLight/LandHeavy)을 유지하는 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|MotionMatching", meta = (ClampMin = "0"))
	float LandingSituationTime = 0.3f;

	/** 파쿠르가 끝난 뒤 FromTraversal 상황을 유지하는 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|MotionMatching", meta = (ClampMin = "0"))
	float FromTraversalTime = 0.5f;

	// ---------------- 제자리 회전 (PT Turn In Place 노드) ----------------

	/** 정지 상태에서 캡슐이 돌 때 메시가 바로 따라 돌지 않고, 발 스텝으로 따라가게 함 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|TurnInPlace")
	bool bEnableTurnInPlace = true;

	/** 몸(메시)이 캡슐 방향을 따라가는 속도. 낮을수록 천천히 돌고 다리가 더 많이 꼬였다가 스텝 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|TurnInPlace", meta = (ClampMin = "0.5"))
	float TurnBodyFollowSpeed = 6.f;

	/** 발 방향과 몸 방향 차이가 이 각도를 넘으면 그 발을 스텝 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|TurnInPlace", meta = (ClampMin = "5", ClampMax = "90"))
	float TurnStepTriggerAngle = 35.f;

	/** 회전이 멈춘 뒤 발이 이 각도 이상 틀어져 있으면 정리 스텝 (자연스러운 자세 복귀) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|TurnInPlace", meta = (ClampMin = "0", ClampMax = "45"))
	float TurnSettleAngle = 12.f;

	/** 회전이 멈추고 정리 스텝까지 기다리는 시간 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|TurnInPlace", meta = (ClampMin = "0"))
	float TurnSettleDelay = 0.2f;

	/** 발 하나가 스텝하는 시간 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|TurnInPlace", meta = (ClampMin = "0.05"))
	float TurnStepDuration = 0.28f;

	/**
	 * 정지(또는 착지) 후 발 고정을 시작하기까지 기다리는 시간 (초).
	 * 멈춤/착지 애니메이션이 끝나 발이 바닥에 안정된 뒤에 고정해야 이상한 위치에 발이 박히지 않음
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|TurnInPlace", meta = (ClampMin = "0"))
	float TurnEngageDelay = 0.35f;

	/** 몸이 캡슐보다 이 각도 이상 뒤처지지 않게 제한 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|TurnInPlace", meta = (ClampMin = "10", ClampMax = "180"))
	float TurnMaxRootYawOffset = 90.f;

	/** 움직이기 시작하면 메시 회전 오프셋을 0 으로 되돌리는 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|TurnInPlace", meta = (ClampMin = "0"))
	float TurnRecoverInterpSpeed = 10.f;

	/** 노드 핀: 메시가 캡슐 대비 회전된 양 (도, + = 메시가 오른쪽) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|TurnInPlace")
	float TurnRootYawOffset = 0.f;

	/** 노드 핀: 현재 스텝 중인 발 (0 = 없음, 1 = 왼발, 2 = 오른발) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|TurnInPlace")
	int32 TurnStepFoot = 0;

	/** 노드 핀: 현재 발의 스텝 진행도 0~1 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|TurnInPlace")
	float TurnStepAlpha = 0.f;

	/** 노드 핀: 이번 스텝이 끝났을 때의 TurnRootYawOffset */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|TurnInPlace")
	float TurnStepEndYawOffset = 0.f;

	/** 노드 핀: 발 고정(IK) 강도 0~1 (정지 시 1, 이동 시 0) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|TurnInPlace")
	float TurnFootLockAlpha = 0.f;

	// ---------------- 에임 오프셋 (PT Aim Offset 노드) ----------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset")
	bool bEnableAimOffset = true;

	/** 좌우 조준 한계 (도). 넘어가면 이 값으로 고정 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0", ClampMax = "180"))
	float AimMaxYaw = 90.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0", ClampMax = "90"))
	float AimMaxPitchUp = 70.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0", ClampMax = "90"))
	float AimMaxPitchDown = 60.f;

	/** 카메라가 캐릭터 뒤쪽을 볼 때 조준을 서서히 끄는 구간 (도) - 몸이 과하게 꺾이는 것 방지 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0", ClampMax = "180"))
	float AimYawFadeStart = 110.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0", ClampMax = "180"))
	float AimYawFadeEnd = 160.f;

	/** 상황별 가중치 */
	/** 정지 상태 가중치 (OrientToMovement). 이동 중에는 AimWeightMoving 을 곱함 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0", ClampMax = "1"))
	float AimWeightOrientToMovement = 1.f;

	/** OrientToMovement 로 달리는 중 가중치 배율 (걷기는 중간값) - 달리면서 고개만 과하게 돌아가지 않게 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0", ClampMax = "1"))
	float AimWeightMoving = 0.65f;

	/** 이동 중 최대 Yaw (정지 시 AimMaxYaw) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0", ClampMax = "180"))
	float AimMaxYawMoving = 65.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0", ClampMax = "1"))
	float AimWeightStrafe = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0", ClampMax = "1"))
	float AimWeightSprint = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0", ClampMax = "1"))
	float AimWeightInAir = 0.5f;

	/** 조준 각도 스무딩 시간 (초, 임계 감쇠 스프링). 본별 추가 지연은 PT Aim Offset 노드의 Smoothing Time */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0"))
	float AimSmoothingTime = 0.08f;

	/** 가중치 변화 스무딩 시간 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|AimOffset", meta = (ClampMin = "0"))
	float AimWeightSmoothingTime = 0.25f;

	// ---------------- 몸 기울기 (PT Aim Offset 노드의 Lean Pitch 핀) ----------------

	/** 전력질주 시 앞으로 숙이는 각도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Lean", meta = (ClampMin = "0", ClampMax = "45"))
	float SprintLeanAngle = 12.f;

	/** 정지 슬라이드 시 뒤로 젖히는 각도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Lean", meta = (ClampMin = "0", ClampMax = "45"))
	float SlideLeanBackAngle = 14.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Lean", meta = (ClampMin = "0"))
	float LeanSmoothingTime = 0.2f;

	/** PT Aim Offset 노드의 Lean Pitch 핀에 연결 (+앞으로 숙임) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|Lean")
	float BodyLeanPitch = 0.f;

	// ---------------- 발 IK (PT Foot IK 노드) ----------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|FootIK")
	bool bEnableFootIK = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|FootIK")
	TEnumAsByte<ECollisionChannel> FootIKTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|FootIK")
	FName FootIKLeftBone = TEXT("foot_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|FootIK")
	FName FootIKRightBone = TEXT("foot_r");

	/** 캡슐 바닥 기준 위/아래로 바닥을 찾는 거리 (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|FootIK", meta = (ClampMin = "0"))
	float FootIKTraceUp = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|FootIK", meta = (ClampMin = "0"))
	float FootIKTraceDown = 60.f;

	/** 골반을 내릴 수 있는 최대 거리 / 발을 올릴 수 있는 최대 높이 (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|FootIK", meta = (ClampMin = "0"))
	float FootIKMaxPelvisDrop = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|FootIK", meta = (ClampMin = "0"))
	float FootIKMaxFootRaise = 45.f;

	/** 발 / 골반 높이 변화 스무딩 시간 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|FootIK", meta = (ClampMin = "0"))
	float FootIKSmoothingTime = 0.06f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|FootIK", meta = (ClampMin = "0"))
	float FootIKPelvisSmoothingTime = 0.1f;

	/** PT Foot IK 노드 핀에 연결 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|FootIK")
	float FootIKLeftOffset = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|FootIK")
	float FootIKRightOffset = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|FootIK")
	float FootIKPelvisOffset = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|FootIK")
	FVector FootIKLeftNormal = FVector::UpVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|FootIK")
	FVector FootIKRightNormal = FVector::UpVector;

	// ---------------- 전력질주 / 정지 슬라이드 표현 ----------------

	/**
	 * 모션 매칭 DB(DatabaseSet)가 없을 때(템플릿 블렌드스페이스 사용 중) 전력질주 속도에 맞춰 메시 애니 재생 속도를 올림.
	 * 블렌드스페이스가 달리기 속도까지만 있어서 생기는 발 미끄러짐을 줄이고 질주 느낌을 줌
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Sprint")
	bool bScaleAnimRateForSprint = true;

	/** 로코모션 애니메이션의 기준 최고 속도 (템플릿 달리기 = 500) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Sprint", meta = (ClampMin = "1"))
	float LocomotionAnimReferenceSpeed = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Sprint", meta = (ClampMin = "1"))
	float SprintMaxAnimRateScale = 1.35f;

	/** 정지 슬라이드 때 슬롯에 재생할 애니메이션 (기본: 템플릿 MM_Land. GASP 의 Sprint Stop 애니메이션으로 교체 가능) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Sprint")
	TSoftObjectPtr<UAnimSequenceBase> SprintStopAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Sprint")
	FName SprintStopSlotName = TEXT("DefaultSlot");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Sprint", meta = (ClampMin = "0.1"))
	float SprintStopAnimPlayRate = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Sprint", meta = (ClampMin = "0"))
	float SprintStopAnimStartTime = 0.f;

	/** PT Aim Offset 노드의 Aim Yaw 핀에 연결 (액터 정면 기준, +오른쪽) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|AimOffset")
	float AimYaw = 0.f;

	/** PT Aim Offset 노드의 Aim Pitch 핀에 연결 (+위) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|AimOffset")
	float AimPitch = 0.f;

	/** PT Aim Offset 노드의 Alpha 핀에 연결 (가중치는 각도에 이미 곱해져 있으므로 1 고정 - 몸 기울기도 같은 노드에서 처리) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|AimOffset")
	float AimAlpha = 1.f;

	// ---------------- 런타임 상태 (AnimBP 에서 바인딩) ----------------

	/** Pose History 노드의 Trajectory 핀에 연결 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|Trajectory")
	FTransformTrajectory Trajectory;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|Locomotion")
	FPTLocomotionInput LocomotionInput;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|Locomotion")
	FPTLocomotionQuery Query;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|Locomotion")
	float Speed2D = 0.f;

	/** Motion Matching 노드의 Blend Time 핀에 연결 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|MotionMatching")
	float MotionMatchingBlendTime = 0.4f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|Locomotion")
	bool bHasAcceleration = false;

	/** 궤적 기준 약 0.5초 후 속도 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|Trajectory")
	FVector FutureVelocity = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|Locomotion")
	bool bJustLanded = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|Locomotion")
	FVector LandVelocity = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|MotionMatching")
	TObjectPtr<const UPoseSearchDatabase> CurrentDatabase;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PT|MotionMatching")
	TArray<FName> CurrentDatabaseTags;

protected:
	UPROPERTY(Transient)
	TObjectPtr<APTCharacter> PTOwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UPTCharacterMovementComponent> PTMovementComponent;

	float DesiredControllerYawLastUpdate = 0.f;
	FPTLocomotionQuery LastSearchQuery;
	bool bHasSearchedOnce = false;
	bool bWasFalling = false;
	float LandedTimer = 0.f;
	float FromTraversalTimer = 0.f;
	bool bWasTraversing = false;
	FVector LastAirVelocity = FVector::ZeroVector;

	// 발 IK 스무딩 상태
	float FootIKLeftRate = 0.f;
	float FootIKRightRate = 0.f;
	float FootIKPelvisRate = 0.f;

	// 에임 / 기울기 스무딩 상태
	float AimYawRaw = 0.f;
	float AimYawRate = 0.f;
	float AimPitchRaw = 0.f;
	float AimPitchRate = 0.f;
	float AimWeight = 0.f;
	float AimWeightRate = 0.f;
	float BodyLeanRate = 0.f;

	// 전력질주 / 슬라이드 (게임 스레드)
	float CurrentAnimRateScale = 1.f;
	bool bWasSprintStopSliding = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> SprintStopMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> LoadedSprintStopAnimation;

	// Turn In Place 내부 상태
	bool bTurnInitialized = false;
	bool bTurnFeetInitialized = false;
	float TurnLastActorYaw = 0.f;
	float TurnStepTime = 0.f;
	float TurnSettleTimer = 0.f;
	float TurnIdleTime = 0.f;
	int32 TurnActiveFoot = 0;
	/** 앞장선 발이 디딘 뒤 따라와야 할 발 (0 = 없음) */
	int32 TurnPendingFollowFoot = 0;
	/** 각 발이 마지막으로 디딘 월드 Yaw (0 = 왼발, 1 = 오른발) */
	float TurnFootYaw[2] = { 0.f, 0.f };
};
