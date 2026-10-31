// Copyright yoomsGames. All Rights Reserved.

#include "Animation/AnimNode_PTFootIK.h"
#include "Animation/AnimInstanceProxy.h"
#include "BonePose.h"
#include "TwoBoneIK.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_PTFootIK)

FAnimNode_PTFootIK::FAnimNode_PTFootIK()
{
	// UE5 / UEFN 마네킹 공용 본 이름
	PelvisBone = FBoneReference(TEXT("pelvis"));
	LeftLeg = FPTTurnInPlaceLeg(TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"));
	RightLeg = FPTTurnInPlaceLeg(TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"));
}

void FAnimNode_PTFootIK::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.2f, L: %.1f, R: %.1f, Pelvis: %.1f)"), ActualAlpha, LeftFootOffset, RightFootOffset, PelvisOffset);
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_PTFootIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	PelvisBone.Initialize(RequiredBones);
	for (FPTTurnInPlaceLeg* Leg : { &LeftLeg, &RightLeg })
	{
		Leg->Thigh.Initialize(RequiredBones);
		Leg->Calf.Initialize(RequiredBones);
		Leg->Foot.Initialize(RequiredBones);
	}
}

bool FAnimNode_PTFootIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	auto LegValid = [&RequiredBones](const FPTTurnInPlaceLeg& Leg)
	{
		return Leg.Thigh.IsValidToEvaluate(RequiredBones) && Leg.Calf.IsValidToEvaluate(RequiredBones) && Leg.Foot.IsValidToEvaluate(RequiredBones);
	};
	return PelvisBone.IsValidToEvaluate(RequiredBones) && LegValid(LeftLeg) && LegValid(RightLeg);
}

void FAnimNode_PTFootIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	if (FMath::IsNearlyZero(LeftFootOffset, 0.05f) && FMath::IsNearlyZero(RightFootOffset, 0.05f) && FMath::IsNearlyZero(PelvisOffset, 0.05f)
		&& LeftFootNormal.Equals(FVector::UpVector, 0.01f) && RightFootNormal.Equals(FVector::UpVector, 0.01f))
	{
		return; // 평지
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FTransform& ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

	// 월드 위쪽을 컴포넌트 공간으로
	FVector UpCS = ComponentTransform.InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
	if (UpCS.IsNearlyZero())
	{
		UpCS = FVector::UpVector;
	}

	// 1) 골반 이동 (자식인 척추/다리 전체가 따라감)
	const FVector PelvisShiftCS = UpCS * PelvisOffset;
	const FCompactPoseBoneIndex PelvisIndex = PelvisBone.GetCompactPoseIndex(BoneContainer);
	FTransform Pelvis = Output.Pose.GetComponentSpaceTransform(PelvisIndex);
	Pelvis.AddToTranslation(PelvisShiftCS);
	OutBoneTransforms.Add(FBoneTransform(PelvisIndex, Pelvis));

	// 2) 다리
	SolveLeg(LeftLeg, Output, PelvisShiftCS, UpCS, LeftFootOffset, LeftFootNormal, OutBoneTransforms);
	SolveLeg(RightLeg, Output, PelvisShiftCS, UpCS, RightFootOffset, RightFootNormal, OutBoneTransforms);

	// LocalBlendCSBoneTransforms 는 본 인덱스(부모 먼저) 순서를 요구
	OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B) { return A.BoneIndex.GetInt() < B.BoneIndex.GetInt(); });
}

void FAnimNode_PTFootIK::SolveLeg(const FPTTurnInPlaceLeg& Leg, FComponentSpacePoseContext& Output, const FVector& PelvisShiftCS, const FVector& UpCS,
	float FootOffset, const FVector& GroundNormalWorld, TArray<FBoneTransform>& OutBoneTransforms) const
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FTransform& ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

	const FCompactPoseBoneIndex ThighIndex = Leg.Thigh.GetCompactPoseIndex(BoneContainer);
	const FCompactPoseBoneIndex CalfIndex = Leg.Calf.GetCompactPoseIndex(BoneContainer);
	const FCompactPoseBoneIndex FootIndex = Leg.Foot.GetCompactPoseIndex(BoneContainer);

	const FTransform FootOriginal = Output.Pose.GetComponentSpaceTransform(FootIndex);

	// 골반 이동을 반영한 다리 (자식이므로 같이 내려감)
	FTransform Thigh = Output.Pose.GetComponentSpaceTransform(ThighIndex);
	FTransform Calf = Output.Pose.GetComponentSpaceTransform(CalfIndex);
	FTransform Foot = FootOriginal;
	Thigh.AddToTranslation(PelvisShiftCS);
	Calf.AddToTranslation(PelvisShiftCS);
	Foot.AddToTranslation(PelvisShiftCS);

	// 목표: 원래 애니메이션 발 위치 + 발 아래 바닥 높이 (스윙 중 들린 높이는 그대로 유지)
	const FVector TargetCS = FootOriginal.GetLocation() + UpCS * FootOffset;

	// 발바닥을 바닥 경사에 맞춤
	FQuat TargetRotation = FootOriginal.GetRotation();
	const FVector NormalCS = ComponentTransform.InverseTransformVectorNoScale(GroundNormalWorld.GetSafeNormal());
	if (!NormalCS.IsNearlyZero())
	{
		FQuat Tilt = FQuat::FindBetweenNormals(UpCS, NormalCS);
		FVector Axis;
		float Angle;
		Tilt.ToAxisAndAngle(Axis, Angle);
		const float MaxAngle = FMath::DegreesToRadians(MaxFootRotation);
		if (Angle > MaxAngle)
		{
			Tilt = FQuat(Axis, MaxAngle);
		}
		TargetRotation = (Tilt * TargetRotation).GetNormalized();
	}

	// 무릎 방향: 애니메이션상 무릎이 튀어나온 방향 유지
	const FVector Mid = (Thigh.GetLocation() + Foot.GetLocation()) * 0.5;
	FVector KneeDirection = Calf.GetLocation() - Mid;
	if (KneeDirection.IsNearlyZero(0.1))
	{
		KneeDirection = FVector::CrossProduct(UpCS, FVector::RightVector);
	}
	const FVector JointTarget = Calf.GetLocation() + KneeDirection.GetSafeNormal() * KneePoleDistance;

	AnimationCore::SolveTwoBoneIK(Thigh, Calf, Foot, JointTarget, TargetCS, false, 1.0, 1.0);
	Foot.SetRotation(TargetRotation);

	OutBoneTransforms.Add(FBoneTransform(ThighIndex, Thigh));
	OutBoneTransforms.Add(FBoneTransform(CalfIndex, Calf));
	OutBoneTransforms.Add(FBoneTransform(FootIndex, Foot));
}
