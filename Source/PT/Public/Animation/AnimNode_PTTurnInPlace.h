// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Animation/BoneReference.h"
#include "AnimNode_PTTurnInPlace.generated.h"

/** 다리 본 체인 (허벅지 - 종아리 - 발) */
USTRUCT(BlueprintType)
struct PT_API FPTTurnInPlaceLeg
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Leg")
	FBoneReference Thigh;

	UPROPERTY(EditAnywhere, Category = "Leg")
	FBoneReference Calf;

	UPROPERTY(EditAnywhere, Category = "Leg")
	FBoneReference Foot;

	FPTTurnInPlaceLeg() = default;
	FPTTurnInPlaceLeg(FName InThigh, FName InCalf, FName InFoot) : Thigh(InThigh), Calf(InCalf), Foot(InFoot) {}
};

/**
 * 절차적 제자리 회전 (Turn In Place) 노드.
 *
 *  - 캡슐이 돌아도 메시는 RootYawOffset 만큼 반대로 회전시켜 월드 기준 제자리에 머물게 하고
 *  - 오프셋을 따라잡는 '스텝' 동안 발을 하나씩 호(arc)를 그리며 새 위치로 옮김
 *  - 디디고 있는 발은 월드 위치/방향에 고정 (Two Bone IK)
 *
 * 회전 판단과 스텝 타이밍은 UPTMotionMatchingAnimInstance::UpdateTurnInPlace 가 계산하고
 * 이 노드는 그 값(핀)으로 포즈만 만듭니다 → 모든 머신에서 동일하게 동작.
 *
 * AnimGraph: [Local To Component] → [PT Turn In Place] → [PT Aim Offset] → [Component To Local]
 */
USTRUCT(BlueprintInternalUseOnly)
struct PT_API FAnimNode_PTTurnInPlace : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** 메시가 캡슐 대비 회전된 양 (도) ← TurnRootYawOffset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurnInPlace", meta = (PinShownByDefault))
	float RootYawOffset = 0.f;

	/** 스텝 중인 발 (0 없음, 1 왼발, 2 오른발) ← TurnStepFoot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurnInPlace", meta = (PinShownByDefault))
	int32 StepFoot = 0;

	/** 스텝 진행도 0~1 ← TurnStepAlpha */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurnInPlace", meta = (PinShownByDefault))
	float StepAlpha = 0.f;

	/** 이번 스텝 종료 시 RootYawOffset ← TurnStepEndYawOffset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurnInPlace", meta = (PinShownByDefault))
	float StepEndYawOffset = 0.f;

	/** 발 고정(IK) 강도 ← TurnFootLockAlpha */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurnInPlace", meta = (PinShownByDefault))
	float FootLockAlpha = 0.f;

	// ---- 설정 ----

	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference RootBone;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference PelvisBone;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FPTTurnInPlaceLeg LeftLeg;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FPTTurnInPlaceLeg RightLeg;

	/** 스텝할 때 발을 드는 높이 (cm) */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "0"))
	float StepHeight = 5.f;

	/** 스텝할 때 골반이 내려가는 양 (cm) - 무게 이동 느낌 */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "0"))
	float PelvisDropOnStep = 1.f;

	/**
	 * 디딘 발이 '애니메이션상 있어야 할 위치'에서 이 거리(cm) 이상 벗어나면 스스로 복구 스텝.
	 * 멈춤/착지 직후 보폭이 벌어진 상태로 고정되어 다리가 꼬이는 것을 방지
	 */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "1"))
	float MaxPlantedDrift = 20.f;

	/** 복구 스텝 시간 (초) */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "0.05"))
	float RecoveryStepDuration = 0.25f;

	// ---- 발목 / 발 디딤 표현 ----

	/** 발을 떼는 순간 뒤꿈치를 드는 각도 (도, 발볼 기준 회전) */
	UPROPERTY(EditAnywhere, Category = "Foot", meta = (ClampMin = "0", ClampMax = "60"))
	float PushOffPitch = 18.f;

	/** 발이 공중에 있을 때 발끝을 드는 각도 (도) */
	UPROPERTY(EditAnywhere, Category = "Foot", meta = (ClampMin = "0", ClampMax = "45"))
	float SwingToePitch = 8.f;

	/** 발목에서 발볼(회전 중심)까지 앞쪽 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "Foot", meta = (ClampMin = "0"))
	float BallForwardOffset = 12.f;

	/** 디딘 발이 몸 회전을 따라 살짝 비트는 비율 (0 = 완전 고정, 1 = 몸과 같이 회전). 발목 꺾임 완화 */
	UPROPERTY(EditAnywhere, Category = "Foot", meta = (ClampMin = "0", ClampMax = "1"))
	float PlantedFootSwivel = 0.2f;

	/** 무릎 방향이 발 방향을 따라가는 비율 (0 = 몸 방향, 1 = 발 방향). 정강이-발목 비틀림 완화 */
	UPROPERTY(EditAnywhere, Category = "Foot", meta = (ClampMin = "0", ClampMax = "1"))
	float KneeFollowFoot = 0.5f;

	/** 무릎이 향할 방향을 정하는 보조 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "1"))
	float KneePoleDistance = 40.f;

	FAnimNode_PTTurnInPlace();

	// FAnimNode_Base
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	// FAnimNode_SkeletalControlBase
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

	/** 다리별 고정 상태 (월드 공간) */
	struct FLegState
	{
		bool bPlanted = false;
		bool bStepping = false;
		FVector PlantedLocation = FVector::ZeroVector;
		float PlantedMeshYaw = 0.f;
		FVector StepStartLocation = FVector::ZeroVector;
		float StepStartMeshYaw = 0.f;
		FVector LastStepTarget = FVector::ZeroVector;
		float LastStepTargetYaw = 0.f;
		/** 실제로 표시 중인 발 Yaw (고정 중 살짝 비튼 값 포함) */
		float DisplayMeshYaw = 0.f;
	};

	/** 스텝 진행도에 따른 발 Pitch (도, + = 발끝 위) */
	float EvaluateStepPitch(float Phase) const;

	bool EvaluateLeg(int32 LegIndex, const FPTTurnInPlaceLeg& Leg, FComponentSpacePoseContext& Output, const FTransform& RootRotation,
		const FTransform& EndRootRotation, const FVector& PelvisShiftCS, const FVector& UpCS, float MeshYaw, float EndMeshYaw,
		TArray<FBoneTransform>& OutBoneTransforms);

	FLegState Legs[2];

	/** 노드 자체 복구 스텝 (0 = 없음, 1 = 왼발, 2 = 오른발) */
	int32 RecoveryFoot = 0;
	float RecoveryAlpha = 0.f;
};
