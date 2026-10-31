// Copyright NeedsGames. All Rights Reserved.

#include "Animation/AnimNode_PTTurnInPlace.h"
#include "Animation/AnimInstanceProxy.h"
#include "BonePose.h"
#include "TwoBoneIK.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_PTTurnInPlace)

FAnimNode_PTTurnInPlace::FAnimNode_PTTurnInPlace()
	: RootBone(TEXT("root"))
	, PelvisBone(TEXT("pelvis"))
	, LeftLeg(TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"))
	, RightLeg(TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"))
{
}

void FAnimNode_PTTurnInPlace::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Offset: %.1f, StepFoot: %d, StepAlpha: %.2f, Lock: %.2f)"), RootYawOffset, StepFoot, StepAlpha, FootLockAlpha);
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_PTTurnInPlace::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	RootBone.Initialize(RequiredBones);
	PelvisBone.Initialize(RequiredBones);
	for (FPTTurnInPlaceLeg* Leg : { &LeftLeg, &RightLeg })
	{
		Leg->Thigh.Initialize(RequiredBones);
		Leg->Calf.Initialize(RequiredBones);
		Leg->Foot.Initialize(RequiredBones);
	}

	for (FLegState& State : Legs)
	{
		State = FLegState();
	}
}

void FAnimNode_PTTurnInPlace::UpdateInternal(const FAnimationUpdateContext& Context)
{
	Super::UpdateInternal(Context);

	if (RecoveryFoot != 0)
	{
		RecoveryAlpha = FMath::Min(1.f, RecoveryAlpha + Context.GetDeltaTime() / FMath::Max(RecoveryStepDuration, KINDA_SMALL_NUMBER));
	}
}

bool FAnimNode_PTTurnInPlace::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	auto LegValid = [&RequiredBones](const FPTTurnInPlaceLeg& Leg)
	{
		return Leg.Thigh.IsValidToEvaluate(RequiredBones) && Leg.Calf.IsValidToEvaluate(RequiredBones) && Leg.Foot.IsValidToEvaluate(RequiredBones);
	};
	return RootBone.IsValidToEvaluate(RequiredBones) && PelvisBone.IsValidToEvaluate(RequiredBones) && LegValid(LeftLeg) && LegValid(RightLeg);
}

void FAnimNode_PTTurnInPlace::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	if (FMath::IsNearlyZero(RootYawOffset, 0.01f) && FootLockAlpha <= KINDA_SMALL_NUMBER)
	{
		for (FLegState& State : Legs)
		{
			State = FLegState();
		}
		RecoveryFoot = 0;
		RecoveryAlpha = 0.f;
		return;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FAnimInstanceProxy* Proxy = Output.AnimInstanceProxy;
	const FTransform& ComponentTransform = Proxy->GetComponentTransform();
	const FTransform& ActorTransform = Proxy->GetActorTransform();

	FVector UpCS = ComponentTransform.InverseTransformVectorNoScale(ActorTransform.GetUnitAxis(EAxis::Z)).GetSafeNormal();
	if (UpCS.IsNearlyZero())
	{
		UpCS = FVector::UpVector;
	}

	// 월드 기준 '메시 정면' 각도 (상수 오프셋은 비교 시 상쇄됨)
	const float ActorYaw = ActorTransform.Rotator().Yaw;
	const float MeshYaw = ActorYaw + RootYawOffset;
	const float EndMeshYaw = ActorYaw + StepEndYawOffset;

	// 루트 본 위치를 중심으로 Up 축 회전
	const FCompactPoseBoneIndex RootIndex = RootBone.GetCompactPoseIndex(BoneContainer);
	const FTransform RootOriginal = Output.Pose.GetComponentSpaceTransform(RootIndex);
	const FVector RootPivot = RootOriginal.GetLocation();
	auto MakeYawAboutRoot = [&RootPivot, &UpCS](float YawDegrees)
	{
		return FTransform(-RootPivot) * FTransform(FQuat(UpCS, FMath::DegreesToRadians(YawDegrees))) * FTransform(RootPivot);
	};
	const FTransform RootRotation = MakeYawAboutRoot(RootYawOffset);
	const FTransform EndRootRotation = MakeYawAboutRoot(StepEndYawOffset);

	OutBoneTransforms.Add(FBoneTransform(RootIndex, RootOriginal * RootRotation));

	// 스텝 중 골반을 살짝 낮춰 무게 이동 표현
	float Drop = 0.f;
	if (StepFoot != 0)
	{
		Drop = PelvisDropOnStep * FMath::Sin(UE_PI * FMath::Clamp(StepAlpha, 0.f, 1.f)) * FMath::Clamp(FootLockAlpha, 0.f, 1.f);
	}
	const FVector PelvisShiftCS = -UpCS * Drop;

	const FCompactPoseBoneIndex PelvisIndex = PelvisBone.GetCompactPoseIndex(BoneContainer);
	FTransform Pelvis = Output.Pose.GetComponentSpaceTransform(PelvisIndex) * RootRotation;
	Pelvis.AddToTranslation(PelvisShiftCS);
	OutBoneTransforms.Add(FBoneTransform(PelvisIndex, Pelvis));

	EvaluateLeg(0, LeftLeg, Output, RootRotation, EndRootRotation, PelvisShiftCS, UpCS, MeshYaw, EndMeshYaw, OutBoneTransforms);
	EvaluateLeg(1, RightLeg, Output, RootRotation, EndRootRotation, PelvisShiftCS, UpCS, MeshYaw, EndMeshYaw, OutBoneTransforms);

	// LocalBlendCSBoneTransforms 는 본 인덱스 오름차순(부모 먼저)을 요구
	OutBoneTransforms.Sort([](const FBoneTransform& A, const FBoneTransform& B) { return A.BoneIndex.GetInt() < B.BoneIndex.GetInt(); });
}

float FAnimNode_PTTurnInPlace::EvaluateStepPitch(float Phase) const
{
	// 0 ~ 0.25 : 뒤꿈치 들기 (0 → -PushOff)
	// 0.25 ~ 0.6 : 발 들어 이동 (-PushOff → +SwingToe)
	// 0.6 ~ 1  : 착지 (+SwingToe → 0)
	const float P = FMath::Clamp(Phase, 0.f, 1.f);
	if (P < 0.25f)
	{
		return -PushOffPitch * FMath::SmoothStep(0.f, 1.f, P / 0.25f);
	}
	if (P < 0.6f)
	{
		return FMath::Lerp(-PushOffPitch, SwingToePitch, FMath::SmoothStep(0.f, 1.f, (P - 0.25f) / 0.35f));
	}
	return FMath::Lerp(SwingToePitch, 0.f, FMath::SmoothStep(0.f, 1.f, (P - 0.6f) / 0.4f));
}

bool FAnimNode_PTTurnInPlace::EvaluateLeg(int32 LegIndex, const FPTTurnInPlaceLeg& Leg, FComponentSpacePoseContext& Output, const FTransform& RootRotation,
	const FTransform& EndRootRotation, const FVector& PelvisShiftCS, const FVector& UpCS, float MeshYaw, float EndMeshYaw,
	TArray<FBoneTransform>& OutBoneTransforms)
{
	FLegState& State = Legs[LegIndex];

	if (FootLockAlpha <= KINDA_SMALL_NUMBER)
	{
		State = FLegState();
		RecoveryFoot = 0;
		RecoveryAlpha = 0.f;
		return false;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FTransform& ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

	const FCompactPoseBoneIndex ThighIndex = Leg.Thigh.GetCompactPoseIndex(BoneContainer);
	const FCompactPoseBoneIndex CalfIndex = Leg.Calf.GetCompactPoseIndex(BoneContainer);
	const FCompactPoseBoneIndex FootIndex = Leg.Foot.GetCompactPoseIndex(BoneContainer);

	const FTransform FootOriginal = Output.Pose.GetComponentSpaceTransform(FootIndex);
	const FTransform FootAnim = FootOriginal * RootRotation; // 골반 이동 전, 애니메이션상의 발

	FTransform Thigh = Output.Pose.GetComponentSpaceTransform(ThighIndex) * RootRotation;
	FTransform Calf = Output.Pose.GetComponentSpaceTransform(CalfIndex) * RootRotation;
	FTransform Foot = FootAnim;
	Thigh.AddToTranslation(PelvisShiftCS);
	Calf.AddToTranslation(PelvisShiftCS);
	Foot.AddToTranslation(PelvisShiftCS);

	const FVector AnimFootWorld = ComponentTransform.TransformPosition(FootAnim.GetLocation());
	if (!State.bPlanted)
	{
		State.bPlanted = true;
		State.PlantedLocation = AnimFootWorld;
		State.PlantedMeshYaw = MeshYaw;
		State.DisplayMeshYaw = MeshYaw;
	}

	const int32 FootId = LegIndex + 1;

	// 디딘 발이 있어야 할 위치에서 너무 멀어졌으면(보폭이 벌어진 채 고정 등) 복구 스텝 시작
	if (StepFoot == 0 && RecoveryFoot == 0 && !State.bStepping
		&& FVector::DistSquaredXY(State.PlantedLocation, AnimFootWorld) > FMath::Square(MaxPlantedDrift))
	{
		RecoveryFoot = FootId;
		RecoveryAlpha = 0.f;
	}

	const bool bDrivenStep = (StepFoot == FootId);
	const bool bRecoveryStep = !bDrivenStep && StepFoot == 0 && RecoveryFoot == FootId;

	FVector TargetWorld;
	float TargetMeshYaw;
	float StepPhase = -1.f; // 스텝 중이면 0~1

	if (bRecoveryStep)
	{
		if (!State.bStepping)
		{
			State.bStepping = true;
			State.StepStartLocation = State.PlantedLocation;
			State.StepStartMeshYaw = State.DisplayMeshYaw;
		}

		// 현재 몸 아래, 애니메이션상의 발 위치로 이동
		const FVector UpWorld = ComponentTransform.TransformVectorNoScale(UpCS);
		const float A = FMath::Clamp(RecoveryAlpha, 0.f, 1.f);
		StepPhase = A;
		const float Eased = FMath::SmoothStep(0.f, 1.f, A);

		TargetWorld = FMath::Lerp(State.StepStartLocation, AnimFootWorld, Eased) + UpWorld * (StepHeight * FMath::Sin(UE_PI * A));
		TargetMeshYaw = State.StepStartMeshYaw + FMath::FindDeltaAngleDegrees(State.StepStartMeshYaw, MeshYaw) * Eased;

		State.LastStepTarget = AnimFootWorld;
		State.LastStepTargetYaw = MeshYaw;

		if (A >= 1.f)
		{
			State.bStepping = false;
			State.PlantedLocation = AnimFootWorld;
			State.PlantedMeshYaw = MeshYaw;
			RecoveryFoot = 0;
			RecoveryAlpha = 0.f;
		}
	}
	else if (bDrivenStep)
	{
		if (RecoveryFoot == FootId)
		{
			// 애님 인스턴스가 지시한 스텝이 우선
			RecoveryFoot = 0;
			RecoveryAlpha = 0.f;
		}
		if (!State.bStepping)
		{
			State.bStepping = true;
			State.StepStartLocation = State.PlantedLocation;
			State.StepStartMeshYaw = State.DisplayMeshYaw;
		}

		// 스텝 종료 시 애니메이션상 발이 있어야 할 위치
		const FVector EndWorld = ComponentTransform.TransformPosition((FootOriginal * EndRootRotation).GetLocation());
		const FVector UpWorld = ComponentTransform.TransformVectorNoScale(UpCS);
		const float A = FMath::Clamp(StepAlpha, 0.f, 1.f);
		StepPhase = A;
		const float Eased = FMath::SmoothStep(0.f, 1.f, A);

		TargetWorld = FMath::Lerp(State.StepStartLocation, EndWorld, Eased) + UpWorld * (StepHeight * FMath::Sin(UE_PI * A));
		TargetMeshYaw = State.StepStartMeshYaw + FMath::FindDeltaAngleDegrees(State.StepStartMeshYaw, EndMeshYaw) * Eased;

		State.LastStepTarget = EndWorld;
		State.LastStepTargetYaw = EndMeshYaw;
	}
	else
	{
		if (State.bStepping)
		{
			// 스텝 완료(또는 다른 발 스텝으로 복구 스텝 중단) → 새 위치에 고정
			if (RecoveryFoot == FootId)
			{
				RecoveryFoot = 0;
				RecoveryAlpha = 0.f;
			}
			State.bStepping = false;
			State.PlantedLocation = State.LastStepTarget;
			State.PlantedMeshYaw = State.LastStepTargetYaw;
		}
		TargetWorld = State.PlantedLocation;
		// 고정된 발도 몸 회전을 따라 아주 조금 비틀어 발목이 억지로 꺾여 보이지 않게
		TargetMeshYaw = State.PlantedMeshYaw + FMath::FindDeltaAngleDegrees(State.PlantedMeshYaw, MeshYaw) * PlantedFootSwivel;
	}

	// 고정된 발은 월드 방향 유지 (몸이 돌아도 발끝이 같이 돌지 않게)
	State.DisplayMeshYaw = TargetMeshYaw;

	const float YawFix = FMath::FindDeltaAngleDegrees(MeshYaw, TargetMeshYaw);
	FQuat TargetFootRotation = FQuat(UpCS, FMath::DegreesToRadians(YawFix)) * FootAnim.GetRotation();
	FVector TargetCS = ComponentTransform.InverseTransformPosition(TargetWorld);

	// 발 디딤: 떼는 순간 발볼을 축으로 뒤꿈치를 들고(toe-off), 공중에서 발끝을 살짝 들었다가, 평평하게 착지
	if (StepPhase >= 0.f)
	{
		const float FootPitch = EvaluateStepPitch(StepPhase);
		if (!FMath::IsNearlyZero(FootPitch, 0.01f))
		{
			const FTransform& ActorTransform = Output.AnimInstanceProxy->GetActorTransform();
			const FVector ActorForwardCS = ComponentTransform.InverseTransformVectorNoScale(ActorTransform.GetUnitAxis(EAxis::X)).GetSafeNormal();
			const float ActorYaw = ActorTransform.Rotator().Yaw;
			const FVector FootForwardCS = FQuat(UpCS, FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(ActorYaw, TargetMeshYaw))).RotateVector(ActorForwardCS);
			const FVector PitchAxis = FVector::CrossProduct(FootForwardCS, UpCS).GetSafeNormal(); // +각도 = 발끝 위로
			if (!PitchAxis.IsNearlyZero())
			{
				const FQuat PitchRotation(PitchAxis, FMath::DegreesToRadians(FootPitch));
				if (FootPitch < 0.f)
				{
					// 뒤꿈치 들기: 발볼을 중심으로 회전 → 발목이 자연스럽게 올라감
					const FVector Ball = TargetCS + FootForwardCS * BallForwardOffset;
					TargetCS = Ball + PitchRotation.RotateVector(TargetCS - Ball);
				}
				TargetFootRotation = PitchRotation * TargetFootRotation;
			}
		}
	}

	const float Lock = FMath::Clamp(FootLockAlpha, 0.f, 1.f);
	TargetCS = FMath::Lerp(FootAnim.GetLocation(), TargetCS, Lock);
	TargetFootRotation = FQuat::Slerp(FootAnim.GetRotation(), TargetFootRotation, Lock).GetNormalized();

	// 무릎 방향: 애니메이션상 무릎이 튀어나온 방향 유지
	const FVector Mid = (Thigh.GetLocation() + Foot.GetLocation()) * 0.5;
	FVector KneeDirection = Calf.GetLocation() - Mid;
	if (KneeDirection.IsNearlyZero(0.1))
	{
		KneeDirection = FQuat(UpCS, FMath::DegreesToRadians(RootYawOffset)).RotateVector(FVector::ForwardVector);
	}
	// 무릎이 몸과 발 방향의 중간을 향하게 해 정강이-발목 비틀림 완화
	KneeDirection = FQuat(UpCS, FMath::DegreesToRadians(YawFix * KneeFollowFoot * Lock)).RotateVector(KneeDirection);
	const FVector JointTarget = Calf.GetLocation() + KneeDirection.GetSafeNormal() * KneePoleDistance;

	AnimationCore::SolveTwoBoneIK(Thigh, Calf, Foot, JointTarget, TargetCS, false, 1.0, 1.0);
	Foot.SetRotation(TargetFootRotation);

	OutBoneTransforms.Add(FBoneTransform(ThighIndex, Thigh));
	OutBoneTransforms.Add(FBoneTransform(CalfIndex, Calf));
	OutBoneTransforms.Add(FBoneTransform(FootIndex, Foot));
	return true;
}
