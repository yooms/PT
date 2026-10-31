// Copyright NeedsGames. All Rights Reserved.

#include "Animation/AnimNode_PTAimOffset.h"
#include "Animation/AnimInstanceProxy.h"
#include "BonePose.h"
#include "Math/SpringMath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_PTAimOffset)

FAnimNode_PTAimOffset::FAnimNode_PTAimOffset()
{
	// UE5 Manny/Quinn, GASP(UEFN Mannequin) 공용 본 이름. Yaw/Pitch 각각 합계 1.0
	Bones.Add(FPTAimOffsetBone(TEXT("spine_01"), 0.10f, 0.10f));
	Bones.Add(FPTAimOffsetBone(TEXT("spine_02"), 0.10f, 0.10f));
	Bones.Add(FPTAimOffsetBone(TEXT("spine_03"), 0.15f, 0.15f));
	Bones.Add(FPTAimOffsetBone(TEXT("spine_04"), 0.15f, 0.15f));
	Bones.Add(FPTAimOffsetBone(TEXT("spine_05"), 0.20f, 0.15f));
	Bones.Add(FPTAimOffsetBone(TEXT("neck_01"), 0.15f, 0.15f));
	Bones.Add(FPTAimOffsetBone(TEXT("head"), 0.15f, 0.20f));
}

void FAnimNode_PTAimOffset::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.2f, Yaw: %.1f, Pitch: %.1f, Lean: %.1f, Bones: %d)"), ActualAlpha, AimYaw, AimPitch, LeanPitch, SortedValidBones.Num());
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_PTAimOffset::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	SortedValidBones.Reset();

	TArray<TPair<int32, int32>> Pairs; // (CompactIndex, BonesIndex)
	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		FPTAimOffsetBone& Entry = Bones[i];
		if (Entry.Bone.Initialize(RequiredBones) && Entry.Bone.IsValidToEvaluate(RequiredBones))
		{
			Pairs.Emplace(Entry.Bone.GetCompactPoseIndex(RequiredBones).GetInt(), i);
		}
	}

	Pairs.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B) { return A.Key < B.Key; });
	for (const TPair<int32, int32>& Pair : Pairs)
	{
		SortedValidBones.Add(Pair.Value);
	}

	// 기울기 분배: 'spine' 본들이 +LeanPitch 를 나눠 갖고, 나머지(목/머리)가 같은 양을 되돌림
	int32 NumSpine = 0;
	for (const int32 Index : SortedValidBones)
	{
		NumSpine += Bones[Index].Bone.BoneName.ToString().Contains(TEXT("spine")) ? 1 : 0;
	}
	const int32 NumOther = SortedValidBones.Num() - NumSpine;

	LeanWeights.Reset();
	for (const int32 Index : SortedValidBones)
	{
		const bool bSpine = Bones[Index].Bone.BoneName.ToString().Contains(TEXT("spine"));
		if (NumSpine == 0)
		{
			LeanWeights.Add(1.f / FMath::Max(SortedValidBones.Num(), 1));
		}
		else if (bSpine)
		{
			LeanWeights.Add(1.f / NumSpine);
		}
		else
		{
			LeanWeights.Add((bKeepHeadLevelOnLean && NumOther > 0) ? -1.f / NumOther : 0.f);
		}
	}

	BoneStates.SetNum(SortedValidBones.Num());
	bStatesInitialized = false;
}

void FAnimNode_PTAimOffset::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);

	const int32 Num = SortedValidBones.Num();
	if (BoneStates.Num() != Num || LeanWeights.Num() != Num)
	{
		return;
	}

	const float DeltaTime = Context.GetDeltaTime();
	for (int32 k = 0; k < Num; ++k)
	{
		const FPTAimOffsetBone& Entry = Bones[SortedValidBones[k]];
		FPTAimOffsetBoneState& State = BoneStates[k];

		const float TargetYaw = AimYaw * Entry.YawWeight;
		const float TargetPitch = AimPitch * Entry.PitchWeight;
		const float TargetLean = LeanPitch * LeanWeights[k];

		if (!bStatesInitialized)
		{
			State = FPTAimOffsetBoneState();
			State.Yaw = TargetYaw;
			State.Pitch = TargetPitch;
			State.Lean = TargetLean;
			continue;
		}

		// 허리 → 머리로 갈수록 빠르게 반응 (머리가 먼저 돌고 몸이 따라옴)
		const float ChainAlpha = (Num > 1) ? (float)k / (float)(Num - 1) : 1.f;
		const float SmoothingTime = FMath::Lerp(ChainRootSmoothingTime, ChainTipSmoothingTime, ChainAlpha);

		SpringMath::CriticalSpringDamper(State.Yaw, State.YawRate, TargetYaw, SmoothingTime, DeltaTime);
		SpringMath::CriticalSpringDamper(State.Pitch, State.PitchRate, TargetPitch, SmoothingTime, DeltaTime);
		SpringMath::CriticalSpringDamper(State.Lean, State.LeanRate, TargetLean, SmoothingTime, DeltaTime);
	}
	bStatesInitialized = true;
}

bool FAnimNode_PTAimOffset::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	for (const FPTAimOffsetBone& Entry : Bones)
	{
		if (Entry.Bone.IsValidToEvaluate(RequiredBones))
		{
			return true;
		}
	}
	return false;
}

void FAnimNode_PTAimOffset::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	const int32 NumStates = BoneStates.Num();
	if (NumStates != SortedValidBones.Num() || !bStatesInitialized)
	{
		return;
	}

	bool bAnyRotation = false;
	for (const FPTAimOffsetBoneState& State : BoneStates)
	{
		if (!FMath::IsNearlyZero(State.Yaw, 0.01f) || !FMath::IsNearlyZero(State.Pitch, 0.01f) || !FMath::IsNearlyZero(State.Lean, 0.01f))
		{
			bAnyRotation = true;
			break;
		}
	}
	if (!bAnyRotation)
	{
		return;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FAnimInstanceProxy* Proxy = Output.AnimInstanceProxy;

	// 액터 기준 축을 컴포넌트 공간으로 변환 (메시가 -90 Yaw 로 붙어 있어도 정확)
	const FTransform& ComponentTransform = Proxy->GetComponentTransform();
	const FTransform& ActorTransform = Proxy->GetActorTransform();

	FVector UpCS = ComponentTransform.InverseTransformVectorNoScale(ActorTransform.GetUnitAxis(EAxis::Z)).GetSafeNormal();
	FVector ForwardCS = ComponentTransform.InverseTransformVectorNoScale(ActorTransform.GetUnitAxis(EAxis::X)).GetSafeNormal();
	if (UpCS.IsNearlyZero())
	{
		UpCS = FVector::UpVector;
	}
	if (ForwardCS.IsNearlyZero())
	{
		ForwardCS = FVector::ForwardVector;
	}

	// 전체 Yaw 를 적용한 조준 방향의 오른쪽 축을 Pitch 축으로 사용
	// (UE 축: Forward × Up = -Right, 이 축으로 +각도 회전 시 위로 들림)
	const FVector AimForwardCS = FQuat(UpCS, FMath::DegreesToRadians(ForwardYawOffset + AimYaw)).RotateVector(ForwardCS);
	FVector PitchAxisCS = FVector::CrossProduct(AimForwardCS, UpCS).GetSafeNormal();
	if (PitchAxisCS.IsNearlyZero())
	{
		return;
	}

	// 기울기는 몸(메시) 정면 기준 좌우 축
	const FVector BodyForwardCS = FQuat(UpCS, FMath::DegreesToRadians(ForwardYawOffset)).RotateVector(ForwardCS);
	FVector LeanAxisCS = FVector::CrossProduct(BodyForwardCS, UpCS).GetSafeNormal();
	if (LeanAxisCS.IsNearlyZero())
	{
		LeanAxisCS = PitchAxisCS;
	}

	// 부모 본 회전이 자식 본 위치/회전에 누적되도록 체인을 순서대로 처리
	FTransform Accumulated = FTransform::Identity;
	for (int32 k = 0; k < SortedValidBones.Num(); ++k)
	{
		const FPTAimOffsetBone& Entry = Bones[SortedValidBones[k]];
		const FPTAimOffsetBoneState& State = BoneStates[k];
		const FCompactPoseBoneIndex CompactIndex = Entry.Bone.GetCompactPoseIndex(BoneContainer);
		if (!CompactIndex.IsValid())
		{
			continue;
		}

		const FTransform Original = Output.Pose.GetComponentSpaceTransform(CompactIndex);
		const FTransform Moved = Original * Accumulated;

		const FQuat YawDelta(UpCS, FMath::DegreesToRadians(State.Yaw));
		const FQuat PitchDelta(PitchAxisCS, FMath::DegreesToRadians(State.Pitch));
		// +축 회전 = 위로 젖힘 → 앞으로 숙이려면 음수
		const FQuat LeanDelta(LeanAxisCS, FMath::DegreesToRadians(-State.Lean));
		const FQuat Delta = LeanDelta * PitchDelta * YawDelta; // Yaw → Pitch → Lean 순

		FTransform NewTransform = Moved;
		NewTransform.SetRotation((Delta * Moved.GetRotation()).GetNormalized());
		OutBoneTransforms.Add(FBoneTransform(CompactIndex, NewTransform));

		// 이 본의 위치를 중심으로 한 회전을 이후(자식) 본에 누적
		const FVector Pivot = Moved.GetLocation();
		const FTransform RotateAboutPivot = FTransform(-Pivot) * FTransform(Delta) * FTransform(Pivot);
		Accumulated = Accumulated * RotateAboutPivot;
	}
}
