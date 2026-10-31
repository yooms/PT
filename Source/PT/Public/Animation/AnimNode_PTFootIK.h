// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Animation/BoneReference.h"
#include "Animation/AnimNode_PTTurnInPlace.h"
#include "AnimNode_PTFootIK.generated.h"

/**
 * 경사면 / 계단용 발 IK 노드.
 *
 *  - 발 아래 바닥 높이(캡슐 바닥 기준 cm)와 바닥 법선은 UPTMotionMatchingAnimInstance 가 게임 스레드에서 트레이스해 전달
 *  - 낮은 쪽 발이 닿도록 골반을 내리고, 각 발을 바닥 높이만큼 올리거나 내린 뒤 Two Bone IK
 *  - 발바닥을 바닥 경사에 맞춰 회전 (최대 MaxFootRotation)
 *
 * AnimGraph: ... → [PT Turn In Place] → [PT Foot IK] → [PT Aim Offset] → ...
 */
USTRUCT(BlueprintInternalUseOnly)
struct PT_API FAnimNode_PTFootIK : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** 왼발 아래 바닥 높이 (캡슐 바닥 기준, cm) ← FootIKLeftOffset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootIK", meta = (PinShownByDefault))
	float LeftFootOffset = 0.f;

	/** 오른발 아래 바닥 높이 ← FootIKRightOffset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootIK", meta = (PinShownByDefault))
	float RightFootOffset = 0.f;

	/** 골반 이동량 (cm, 음수 = 내림) ← FootIKPelvisOffset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootIK", meta = (PinShownByDefault))
	float PelvisOffset = 0.f;

	/** 왼발 아래 바닥 법선 (월드) ← FootIKLeftNormal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootIK", meta = (PinShownByDefault))
	FVector LeftFootNormal = FVector::UpVector;

	/** 오른발 아래 바닥 법선 (월드) ← FootIKRightNormal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootIK", meta = (PinShownByDefault))
	FVector RightFootNormal = FVector::UpVector;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FBoneReference PelvisBone;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FPTTurnInPlaceLeg LeftLeg;

	UPROPERTY(EditAnywhere, Category = "Bones")
	FPTTurnInPlaceLeg RightLeg;

	/** 발바닥을 바닥 경사에 맞추는 최대 각도 */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "0", ClampMax = "60"))
	float MaxFootRotation = 35.f;

	/** 무릎 방향 보조 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "1"))
	float KneePoleDistance = 40.f;

	FAnimNode_PTFootIK();

	// FAnimNode_Base
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	// FAnimNode_SkeletalControlBase
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

	void SolveLeg(const FPTTurnInPlaceLeg& Leg, FComponentSpacePoseContext& Output, const FVector& PelvisShiftCS, const FVector& UpCS,
		float FootOffset, const FVector& GroundNormalWorld, TArray<FBoneTransform>& OutBoneTransforms) const;
};
