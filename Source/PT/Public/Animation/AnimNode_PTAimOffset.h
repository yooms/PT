// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Animation/BoneReference.h"
#include "AnimNode_PTAimOffset.generated.h"

/** 조준 회전을 나눠 받을 본과 가중치 */
USTRUCT(BlueprintType)
struct PT_API FPTAimOffsetBone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Aim")
	FBoneReference Bone;

	/** 전체 Yaw 중 이 본이 담당하는 비율 (체인 전체 합 = 1 권장) */
	UPROPERTY(EditAnywhere, Category = "Aim", meta = (ClampMin = "0", ClampMax = "1"))
	float YawWeight = 0.15f;

	/** 전체 Pitch 중 이 본이 담당하는 비율 (체인 전체 합 = 1 권장) */
	UPROPERTY(EditAnywhere, Category = "Aim", meta = (ClampMin = "0", ClampMax = "1"))
	float PitchWeight = 0.15f;

	FPTAimOffsetBone() = default;
	FPTAimOffsetBone(FName InBone, float InYaw, float InPitch) : Bone(InBone), YawWeight(InYaw), PitchWeight(InPitch) {}
};

/** 본별 스무딩 상태 (런타임 전용) */
struct FPTAimOffsetBoneState
{
	float Yaw = 0.f;
	float YawRate = 0.f;
	float Pitch = 0.f;
	float PitchRate = 0.f;
	float Lean = 0.f;
	float LeanRate = 0.f;
};

/**
 * 절차적(Procedural) 에임 오프셋 노드.
 *
 * 에임 오프셋 에셋 없이, 조준 Yaw/Pitch 를 척추~머리 본 체인에 가중치대로 나눠 회전시킵니다.
 * 회전축은 액터 기준(위쪽 = 액터 Up, Pitch 축 = 조준 방향의 오른쪽)이므로
 * 메시 회전 오프셋(-90 등)이나 재생 중인 애니메이션 포즈와 무관하게 동작 → 모션 매칭 위에 얹기 적합.
 *
 * AnimGraph:  [Local To Component] → [PT Aim Offset] → [Component To Local]
 * 핀 바인딩:  Aim Yaw ← AimYaw,  Aim Pitch ← AimPitch,  Alpha ← AimAlpha  (UPTMotionMatchingAnimInstance)
 */
USTRUCT(BlueprintInternalUseOnly)
struct PT_API FAnimNode_PTAimOffset : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** 액터 정면 기준 조준 Yaw (도, +오른쪽) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (PinShownByDefault))
	float AimYaw = 0.f;

	/** 조준 Pitch (도, +위쪽) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (PinShownByDefault))
	float AimPitch = 0.f;

	/** 메시 정면이 액터 정면에서 돌아간 양 (도). PT Turn In Place 사용 시 TurnRootYawOffset 연결 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (PinShownByDefault))
	float ForwardYawOffset = 0.f;

	/** 몸 앞뒤 기울기 (도, +앞으로 숙임). 척추가 나눠 숙이고 목/머리가 반대로 돌려 시선 유지 (BodyLeanPitch 연결) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (PinShownByDefault))
	float LeanPitch = 0.f;

	/**
	 * 본별 지연 (임계 감쇠 스프링 시간, 초). 체인 시작(허리) → 끝(머리)으로 보간.
	 * 머리가 먼저 돌고 허리가 늦게 따라와 사람처럼 보이게 함. 0 이면 지연 없음
	 */
	UPROPERTY(EditAnywhere, Category = "Smoothing", meta = (ClampMin = "0"))
	float ChainRootSmoothingTime = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Smoothing", meta = (ClampMin = "0"))
	float ChainTipSmoothingTime = 0.05f;

	/** LeanPitch 를 목/머리로 되돌려 시선을 수평으로 유지 */
	UPROPERTY(EditAnywhere, Category = "Lean")
	bool bKeepHeadLevelOnLean = true;

	/** 루트 → 끝 순서로 처리됩니다 (부모 본의 회전이 자식에게 누적). 기본값: UE5 마네킹 / GASP 스켈레톤 */
	UPROPERTY(EditAnywhere, Category = "Bones", meta = (TitleProperty = "Bone"))
	TArray<FPTAimOffsetBone> Bones;

	FAnimNode_PTAimOffset();

	// FAnimNode_Base
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	// FAnimNode_SkeletalControlBase
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

	/** 유효한 본의 Bones 인덱스 (Compact 인덱스 오름차순 = 부모 먼저) */
	TArray<int32> SortedValidBones;

	/** 본별 기울기 가중치 (SortedValidBones 순서) - 척추는 +, 목/머리는 - */
	TArray<float> LeanWeights;

	/** 본별 스무딩된 각도/속도 (SortedValidBones 순서) */
	TArray<FPTAimOffsetBoneState> BoneStates;
	bool bStatesInitialized = false;
};
