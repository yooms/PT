// Copyright yoomsGames. All Rights Reserved.

#include "Animation/PTMotionMatchingAnimInstance.h"
#include "Character/PTCharacter.h"
#include "Character/PTCharacterMovementComponent.h"
#include "PTLogChannels.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Math/SpringMath.h"
#include "PoseSearch/MotionMatchingAnimNodeLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTMotionMatchingAnimInstance)

namespace PTMotionMatchingCVars
{
	static bool bDrawTrajectory = false;
	static FAutoConsoleVariableRef CVarDrawTrajectory(
		TEXT("pt.MotionMatching.DrawTrajectory"),
		bDrawTrajectory,
		TEXT("Draw the motion matching trajectory of PT characters."),
		ECVF_Cheat);

	static bool bLogDatabaseChanges = false;
	static FAutoConsoleVariableRef CVarLogDatabaseChanges(
		TEXT("pt.MotionMatching.LogDatabaseChanges"),
		bLogDatabaseChanges,
		TEXT("Log when the motion matching search query changes."),
		ECVF_Cheat);
}

UPTMotionMatchingAnimInstance::UPTMotionMatchingAnimInstance()
{
	// GASP 기본값에 가까운 궤적 설정
	TrajectoryDataIdle.RotateTowardsMovementSpeed = 0.f;
	TrajectoryDataIdle.MaxControllerYawRate = 100.f;

	TrajectoryDataMoving.RotateTowardsMovementSpeed = 10.f;
	TrajectoryDataMoving.MaxControllerYawRate = 100.f;

	// 템플릿 마네킹의 착지 동작을 정지 슬라이드 포즈로 사용 (GASP 연결 후 Sprint Stop 애니메이션으로 교체 권장)
	SprintStopAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Land.MM_Land")));
}

void UPTMotionMatchingAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	PTOwnerCharacter = Cast<APTCharacter>(TryGetPawnOwner());
	PTMovementComponent = PTOwnerCharacter ? PTOwnerCharacter->GetPTMovementComponent() : nullptr;

	if (PTOwnerCharacter && !SprintStopAnimation.IsNull())
	{
		LoadedSprintStopAnimation = SprintStopAnimation.LoadSynchronous();
	}

	if (PTOwnerCharacter)
	{
		UPoseSearchTrajectoryLibrary::InitTrajectorySamples(Trajectory,
			PTOwnerCharacter->GetActorLocation(), PTOwnerCharacter->GetActorQuat(),
			FPoseSearchTrajectoryData::FSampling{ HistorySampleCount, HistorySamplingInterval, PredictionSampleCount, PredictionSamplingInterval },
			1.f / 60.f);
	}
}

void UPTMotionMatchingAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!PTOwnerCharacter || !PTMovementComponent)
	{
		PTOwnerCharacter = Cast<APTCharacter>(TryGetPawnOwner());
		PTMovementComponent = PTOwnerCharacter ? PTOwnerCharacter->GetPTMovementComponent() : nullptr;
		if (!PTOwnerCharacter || !PTMovementComponent)
		{
			return;
		}
	}

	// ---- Game thread: 상태 수집 ----
	LocomotionInput.ActorTransform = PTOwnerCharacter->GetActorTransform();
	LocomotionInput.Velocity = PTMovementComponent->Velocity;
	LocomotionInput.Acceleration = PTMovementComponent->GetCurrentAcceleration();
	LocomotionInput.MaxAcceleration = PTMovementComponent->GetMaxAcceleration();
	LocomotionInput.Gait = PTMovementComponent->GetGait();
	LocomotionInput.Stance = PTMovementComponent->GetStance();
	LocomotionInput.RotationMode = PTMovementComponent->GetRotationMode();
	LocomotionInput.bIsFalling = PTMovementComponent->IsFalling();
	LocomotionInput.bIsLocallyControlled = PTOwnerCharacter->IsLocallyControlled();
	LocomotionInput.ControlRotation = PTOwnerCharacter->GetControlRotation();
	LocomotionInput.AimRotation = PTOwnerCharacter->GetAimRotation();
	// 파쿠르(절차적 모드 포함)는 Flying 으로 진행
	LocomotionInput.bIsPlayingRootMotionMontage = (GetRootMotionMontageInstance() != nullptr) || PTMovementComponent->MovementMode == MOVE_Flying;
	LocomotionInput.bIsSprintStopSliding = PTMovementComponent->IsSprintStopSliding();

	UpdateSprintPresentation(DeltaSeconds);
	UpdateFootIK(DeltaSeconds);

	if (PTMotionMatchingCVars::bDrawTrajectory)
	{
		UPoseSearchTrajectoryLibrary::DrawTransformTrajectory(this, Trajectory, 2.f, 0.f);
	}
}

void UPTMotionMatchingAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (!PTOwnerCharacter || !PTMovementComponent || DeltaSeconds <= 0.f)
	{
		return;
	}

	UpdateTrajectory(DeltaSeconds);
	UpdateLocomotionState(DeltaSeconds);
	UpdateTurnInPlace(DeltaSeconds);
	UpdateAimOffset(DeltaSeconds);
	UpdateBodyLean(DeltaSeconds);
}

void UPTMotionMatchingAnimInstance::UpdateTurnInPlace(float DeltaSeconds)
{
	const float ActorYaw = LocomotionInput.ActorTransform.Rotator().Yaw;
	if (!bTurnInitialized)
	{
		TurnLastActorYaw = ActorYaw;
		bTurnInitialized = true;
	}
	const float ActorYawDelta = FMath::FindDeltaAngleDegrees(TurnLastActorYaw, ActorYaw);
	TurnLastActorYaw = ActorYaw;

	const bool bAllow = bEnableTurnInPlace
		&& !LocomotionInput.bIsPlayingRootMotionMontage
		&& Query.Situation == EPTMovementSituation::OnGround
		&& Query.MovementState == EPTMovementState::Idle;

	if (!bAllow)
	{
		// 움직이기 시작하면 메시를 캡슐 방향으로 부드럽게 복귀, 발 고정 해제
		bTurnFeetInitialized = false;
		TurnActiveFoot = 0;
		TurnPendingFollowFoot = 0;
		TurnStepFoot = 0;
		TurnStepAlpha = 0.f;
		TurnSettleTimer = 0.f;
		TurnIdleTime = 0.f;
		TurnRootYawOffset = FMath::FInterpTo(TurnRootYawOffset, 0.f, DeltaSeconds, TurnRecoverInterpSpeed);
		TurnStepEndYawOffset = TurnRootYawOffset;
		TurnFootLockAlpha = FMath::FInterpTo(TurnFootLockAlpha, 0.f, DeltaSeconds, 15.f);
		return;
	}

	// 1) 몸: 캡슐이 돈 만큼 반대로 남겨둔 뒤, 부드럽게 캡슐 방향을 따라감 (발은 고정된 채 다리가 살짝 꼬임)
	TurnRootYawOffset = FMath::Clamp(TurnRootYawOffset - ActorYawDelta, -TurnMaxRootYawOffset, TurnMaxRootYawOffset);
	TurnRootYawOffset = FMath::FInterpTo(TurnRootYawOffset, 0.f, DeltaSeconds, TurnBodyFollowSpeed);

	// 멈춤/착지 직후에는 애니메이션상 발이 아직 움직이는 중(보폭이 벌어져 있거나 공중) →
	// 그 상태로 고정하면 발이 엉뚱한 곳에 박히므로, 포즈가 안정될 때까지 고정을 미룸
	TurnIdleTime += DeltaSeconds;
	const bool bFeetReady = (TurnIdleTime >= TurnEngageDelay) && !bJustLanded;
	if (!bFeetReady)
	{
		bTurnFeetInitialized = false;
		TurnActiveFoot = 0;
		TurnPendingFollowFoot = 0;
		TurnStepFoot = 0;
		TurnStepAlpha = 0.f;
		TurnStepEndYawOffset = TurnRootYawOffset;
		TurnFootLockAlpha = 0.f; // 0 이면 노드가 발 상태를 초기화하고 애니메이션 포즈를 그대로 사용
		return;
	}

	// 고정 시작은 즉시(1) - 노드가 이 순간의 '안정된' 애니메이션 발 위치에 고정 (해제는 위에서 부드럽게)
	TurnFootLockAlpha = 1.f;

	const float BodyYaw = ActorYaw + TurnRootYawOffset;
	if (!bTurnFeetInitialized)
	{
		TurnFootYaw[0] = BodyYaw;
		TurnFootYaw[1] = BodyYaw;
		bTurnFeetInitialized = true;
	}

	// 스텝이 끝날 때쯤 몸이 있을 위치를 예측해 그 아래로 발을 옮김
	auto PredictOffset = [this](float TimeAhead)
	{
		return TurnRootYawOffset * FMath::Exp(-TurnBodyFollowSpeed * FMath::Max(TimeAhead, 0.f));
	};

	// 2) 발: 한 번에 한 발씩만 스텝
	if (TurnActiveFoot != 0)
	{
		TurnStepTime += DeltaSeconds;
		const float StepProgress = FMath::Clamp(TurnStepTime / FMath::Max(TurnStepDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

		TurnStepEndYawOffset = PredictOffset(TurnStepDuration - TurnStepTime);
		TurnStepFoot = TurnActiveFoot;
		TurnStepAlpha = StepProgress;

		if (StepProgress >= 1.f)
		{
			TurnFootYaw[TurnActiveFoot - 1] = ActorYaw + TurnStepEndYawOffset;
			TurnActiveFoot = 0;
			TurnStepFoot = 0;
			TurnStepAlpha = 0.f;
		}
		return;
	}

	const float LeftDiff = FMath::FindDeltaAngleDegrees(TurnFootYaw[0], BodyYaw);
	const float RightDiff = FMath::FindDeltaAngleDegrees(TurnFootYaw[1], BodyYaw);
	const float MaxDiff = FMath::Max(FMath::Abs(LeftDiff), FMath::Abs(RightDiff));

	const bool bBodyTurning = FMath::Abs(ActorYawDelta) > KINDA_SMALL_NUMBER || FMath::Abs(TurnRootYawOffset) > 2.f;
	TurnSettleTimer = bBodyTurning ? 0.f : TurnSettleTimer + DeltaSeconds;

	int32 FootToStep = 0;
	const float PendingDiff = (TurnPendingFollowFoot == 1) ? FMath::Abs(LeftDiff) : FMath::Abs(RightDiff);
	if (TurnPendingFollowFoot != 0 && PendingDiff <= TurnSettleAngle)
	{
		// 따라올 필요가 없을 만큼 이미 정렬됨 → 대기 해제 (다음 회전은 다시 회전 방향 쪽 발부터)
		TurnPendingFollowFoot = 0;
	}

	if (TurnPendingFollowFoot != 0 && PendingDiff > TurnSettleAngle)
	{
		// 앞장선 발이 디딘 직후 → 반대 발이 바로 따라옴
		FootToStep = TurnPendingFollowFoot;
		TurnPendingFollowFoot = 0;
	}
	else if (MaxDiff > TurnStepTriggerAngle)
	{
		// 항상 '돌아가는 방향 쪽 발'이 먼저 (+ = 오른쪽 회전 → 오른발, - = 왼쪽 회전 → 왼발)
		const float TurnDir = (FMath::Abs(LeftDiff) >= FMath::Abs(RightDiff)) ? LeftDiff : RightDiff;
		FootToStep = (TurnDir > 0.f) ? 2 : 1;
		TurnPendingFollowFoot = (FootToStep == 1) ? 2 : 1;
	}
	else if (TurnSettleTimer >= TurnSettleDelay && MaxDiff > TurnSettleAngle)
	{
		// 회전이 끝난 뒤 많이 틀어진 발부터 정리 스텝
		FootToStep = (FMath::Abs(LeftDiff) >= FMath::Abs(RightDiff)) ? 1 : 2;
	}

	if (FootToStep != 0)
	{
		TurnActiveFoot = FootToStep;
		TurnStepTime = 0.f;
		TurnStepFoot = FootToStep;
		TurnStepAlpha = 0.f;
		TurnStepEndYawOffset = PredictOffset(TurnStepDuration);
	}
	else
	{
		TurnStepFoot = 0;
		TurnStepAlpha = 0.f;
		TurnStepEndYawOffset = TurnRootYawOffset;
	}
}

void UPTMotionMatchingAnimInstance::UpdateAimOffset(float DeltaSeconds)
{
	// 액터 정면 기준 조준 각도
	const FRotator Delta = (LocomotionInput.AimRotation - LocomotionInput.ActorTransform.Rotator()).GetNormalized();
	// 제자리 회전으로 메시가 캡슐보다 뒤처져 있으면 그만큼 보정 (메시 정면 기준)
	const float RawYaw = FRotator::NormalizeAxis(Delta.Yaw - TurnRootYawOffset);
	const float RawPitch = Delta.Pitch;

	const bool bStrafe = (Query.RotationMode == EPTRotationMode::Strafe);
	// 0 = 정지, 1 = 달리기 속도 이상
	const float MoveBlend = FMath::Clamp(Speed2D / FMath::Max(LocomotionAnimReferenceSpeed, 1.f), 0.f, 1.f);

	float TargetWeight = 0.f;
	if (bEnableAimOffset && !LocomotionInput.bIsPlayingRootMotionMontage)
	{
		TargetWeight = bStrafe ? AimWeightStrafe : AimWeightOrientToMovement * FMath::Lerp(1.f, AimWeightMoving, MoveBlend);

		if (Query.Gait == EPTGait::Sprint && Query.MovementState == EPTMovementState::Moving)
		{
			TargetWeight *= AimWeightSprint;
		}
		if (Query.Situation == EPTMovementSituation::InAir)
		{
			TargetWeight *= AimWeightInAir;
		}
		if (LocomotionInput.bIsSprintStopSliding)
		{
			TargetWeight *= 0.3f;
		}

		// 카메라가 뒤쪽을 보면 서서히 끔
		const float AbsYaw = FMath::Abs(RawYaw);
		if (AimYawFadeEnd > AimYawFadeStart && AbsYaw > AimYawFadeStart)
		{
			TargetWeight *= 1.f - FMath::Clamp((AbsYaw - AimYawFadeStart) / (AimYawFadeEnd - AimYawFadeStart), 0.f, 1.f);
		}
	}

	const float MaxYaw = bStrafe ? AimMaxYaw : FMath::Lerp(AimMaxYaw, FMath::Min(AimMaxYawMoving, AimMaxYaw), MoveBlend);
	const float TargetYaw = FMath::Clamp(RawYaw, -MaxYaw, MaxYaw);
	const float TargetPitch = FMath::Clamp(RawPitch, -AimMaxPitchDown, AimMaxPitchUp);

	// 임계 감쇠 스프링: 출발/정지가 부드럽고 튀지 않음 (FInterpTo 보다 자연스러운 가감속)
	SpringMath::CriticalSpringDamper(AimYawRaw, AimYawRate, TargetYaw, AimSmoothingTime, DeltaSeconds);
	SpringMath::CriticalSpringDamper(AimPitchRaw, AimPitchRate, TargetPitch, AimSmoothingTime, DeltaSeconds);
	SpringMath::CriticalSpringDamper(AimWeight, AimWeightRate, TargetWeight, AimWeightSmoothingTime, DeltaSeconds);
	AimWeight = FMath::Clamp(AimWeight, 0.f, 1.f);

	// 가중치는 각도에 곱해 전달 (노드 Alpha 는 1 고정 → 같은 노드의 몸 기울기는 가중치와 무관)
	AimYaw = AimYawRaw * AimWeight;
	AimPitch = AimPitchRaw * AimWeight;
	AimAlpha = 1.f;
}

void UPTMotionMatchingAnimInstance::UpdateBodyLean(float DeltaSeconds)
{
	float TargetLean = 0.f;
	// 모션 매칭 DB 를 쓰면 GASP 애니메이션 자체에 질주 기울기 / 정지 동작이 들어 있으므로 추가하지 않음
	if (!DatabaseSet && !LocomotionInput.bIsPlayingRootMotionMontage && Query.Situation != EPTMovementSituation::InAir)
	{
		if (LocomotionInput.bIsSprintStopSliding)
		{
			TargetLean = -SlideLeanBackAngle;
		}
		else if (Query.Gait == EPTGait::Sprint && Query.MovementState == EPTMovementState::Moving)
		{
			// 질주 속도에 비례해 앞으로 숙임
			const float SprintBlend = FMath::Clamp((Speed2D - LocomotionAnimReferenceSpeed * 0.8f) / FMath::Max(LocomotionAnimReferenceSpeed * 0.6f, 1.f), 0.f, 1.f);
			TargetLean = SprintLeanAngle * SprintBlend;
		}
	}

	SpringMath::CriticalSpringDamper(BodyLeanPitch, BodyLeanRate, TargetLean, LeanSmoothingTime, DeltaSeconds);
}

void UPTMotionMatchingAnimInstance::UpdateFootIK(float DeltaSeconds)
{
	USkeletalMeshComponent* MeshComp = GetSkelMeshComponent();
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = PTOwnerCharacter ? PTOwnerCharacter->GetCapsuleComponent() : nullptr;
	if (!MeshComp || !World || !Capsule || !PTMovementComponent || DeltaSeconds <= 0.f)
	{
		return;
	}

	float TargetLeft = 0.f;
	float TargetRight = 0.f;
	FVector TargetLeftNormal = FVector::UpVector;
	FVector TargetRightNormal = FVector::UpVector;

	// 지상에서만 (공중 / 파쿠르 몽타주 중에는 0 으로 서서히 복귀)
	const bool bActive = bEnableFootIK && PTMovementComponent->IsMovingOnGround() && !LocomotionInput.bIsPlayingRootMotionMontage;
	if (bActive)
	{
		const float FloorZ = Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
		const float WalkableZ = PTMovementComponent->GetWalkableFloorZ();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PTFootIK), false, PTOwnerCharacter);

		auto TraceFoot = [&](FName BoneName, float& OutOffset, FVector& OutNormal)
		{
			if (BoneName.IsNone() || !MeshComp->DoesSocketExist(BoneName))
			{
				return;
			}
			// 발의 수평 위치에서 캡슐 바닥 위/아래로 바닥 찾기 (높이는 캡슐 기준이라 IK 결과가 다시 반영되지 않음)
			const FVector FootLocation = MeshComp->GetSocketLocation(BoneName);
			const FVector Start(FootLocation.X, FootLocation.Y, FloorZ + FootIKTraceUp);
			const FVector End(FootLocation.X, FootLocation.Y, FloorZ - FootIKTraceDown);
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Start, End, FootIKTraceChannel, Params) && !Hit.bStartPenetrating && Hit.ImpactNormal.Z >= WalkableZ)
			{
				OutOffset = FMath::Clamp(Hit.ImpactPoint.Z - FloorZ, -FootIKMaxPelvisDrop, FootIKMaxFootRaise);
				OutNormal = Hit.ImpactNormal;
			}
		};

		TraceFoot(FootIKLeftBone, TargetLeft, TargetLeftNormal);
		TraceFoot(FootIKRightBone, TargetRight, TargetRightNormal);
	}

	// 낮은 쪽 발이 닿도록 골반을 내림 (올리지는 않음)
	const float TargetPelvis = FMath::Max(FMath::Min3(TargetLeft, TargetRight, 0.f), -FootIKMaxPelvisDrop);

	SpringMath::CriticalSpringDamper(FootIKLeftOffset, FootIKLeftRate, TargetLeft, FootIKSmoothingTime, DeltaSeconds);
	SpringMath::CriticalSpringDamper(FootIKRightOffset, FootIKRightRate, TargetRight, FootIKSmoothingTime, DeltaSeconds);
	SpringMath::CriticalSpringDamper(FootIKPelvisOffset, FootIKPelvisRate, TargetPelvis, FootIKPelvisSmoothingTime, DeltaSeconds);

	const float NormalAlpha = (FootIKSmoothingTime > 0.f) ? 1.f - FMath::Exp(-DeltaSeconds / FootIKSmoothingTime) : 1.f;
	FootIKLeftNormal = FMath::Lerp(FootIKLeftNormal, TargetLeftNormal, NormalAlpha).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	FootIKRightNormal = FMath::Lerp(FootIKRightNormal, TargetRightNormal, NormalAlpha).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
}

void UPTMotionMatchingAnimInstance::UpdateSprintPresentation(float DeltaSeconds)
{
	USkeletalMeshComponent* MeshComp = GetSkelMeshComponent();
	if (!MeshComp || !PTMovementComponent)
	{
		return;
	}

	const float Speed = LocomotionInput.Velocity.Size2D();
	const bool bOnGround = PTMovementComponent->IsMovingOnGround();

	// 1) 전력질주 애니 재생 속도 (모션 매칭 DB 를 쓰면 실제 질주 애니메이션이 있으므로 하지 않음)
	float TargetRate = 1.f;
	if (bScaleAnimRateForSprint && !DatabaseSet && bOnGround && !LocomotionInput.bIsPlayingRootMotionMontage
		&& LocomotionInput.Gait == EPTGait::Sprint && !LocomotionInput.bIsSprintStopSliding)
	{
		TargetRate = FMath::Clamp(Speed / FMath::Max(LocomotionAnimReferenceSpeed, 1.f), 1.f, SprintMaxAnimRateScale);
	}
	CurrentAnimRateScale = FMath::FInterpTo(CurrentAnimRateScale, TargetRate, DeltaSeconds, 6.f);
	if (LocomotionInput.bIsSprintStopSliding || LocomotionInput.bIsPlayingRootMotionMontage
		|| (FMath::IsNearlyEqual(CurrentAnimRateScale, 1.f, 0.005f) && TargetRate == 1.f))
	{
		CurrentAnimRateScale = 1.f;
	}
	if (MeshComp->GlobalAnimRateScale != CurrentAnimRateScale)
	{
		MeshComp->GlobalAnimRateScale = CurrentAnimRateScale;
	}

	// 2) 정지 슬라이드 애니메이션 (상태는 CMC 가 예측/복제 → 모든 머신에서 로컬로 재생)
	// 모션 매칭 DB 를 쓰면 GASP 의 Sprint/Run Stops 애니메이션이 정지 동작을 담당
	const bool bSliding = LocomotionInput.bIsSprintStopSliding && !DatabaseSet;
	if (bSliding && !bWasSprintStopSliding)
	{
		if (!LoadedSprintStopAnimation && !SprintStopAnimation.IsNull())
		{
			LoadedSprintStopAnimation = SprintStopAnimation.LoadSynchronous();
		}
		if (LoadedSprintStopAnimation)
		{
			SprintStopMontage = PlaySlotAnimationAsDynamicMontage(LoadedSprintStopAnimation, SprintStopSlotName,
				0.12f, 0.3f, SprintStopAnimPlayRate, 1, -1.f, SprintStopAnimStartTime);
		}
	}
	else if (!bSliding && bWasSprintStopSliding)
	{
		// 슬라이드가 일찍 끝나면(다시 이동 입력) 바로 블렌드 아웃
		if (SprintStopMontage && Montage_IsPlaying(SprintStopMontage) && PTMovementComponent->GetCurrentAcceleration().SizeSquared2D() > KINDA_SMALL_NUMBER)
		{
			Montage_Stop(0.2f, SprintStopMontage);
		}
		SprintStopMontage = nullptr;
	}
	bWasSprintStopSliding = bSliding;
}

void UPTMotionMatchingAnimInstance::UpdateTrajectory(float DeltaSeconds)
{
	Speed2D = LocomotionInput.Velocity.Size2D();
	bHasAcceleration = LocomotionInput.Acceleration.SizeSquared2D() > KINDA_SMALL_NUMBER;

	const FPoseSearchTrajectoryData& TrajectoryData = (Speed2D > MovingSpeedThreshold) ? TrajectoryDataMoving : TrajectoryDataIdle;

	// CharacterMovement 의 이동 공식을 시뮬레이션해 미래 궤적을 예측하고 과거 궤적을 누적
	UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(
		this, TrajectoryData, DeltaSeconds,
		Trajectory, DesiredControllerYawLastUpdate, Trajectory,
		HistorySamplingInterval, HistorySampleCount,
		PredictionSamplingInterval, PredictionSampleCount);

	// 약 0.4~0.5초 후 예상 속도
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(Trajectory, 0.4f, 0.5f, FutureVelocity, false);
}

void UPTMotionMatchingAnimInstance::UpdateLocomotionState(float DeltaSeconds)
{
	const bool bIsFalling = LocomotionInput.bIsFalling;

	// 착지 판정
	if (bWasFalling && !bIsFalling)
	{
		bJustLanded = true;
		LandedTimer = LandingSituationTime;
		LandVelocity = LastAirVelocity;
	}
	else if (LandedTimer > 0.f)
	{
		LandedTimer -= DeltaSeconds;
		if (LandedTimer <= 0.f)
		{
			bJustLanded = false;
		}
	}
	if (bIsFalling)
	{
		LastAirVelocity = LocomotionInput.Velocity;
	}
	bWasFalling = bIsFalling;

	// 파쿠르(루트 모션 몽타주 / Flying) 종료 감지
	const bool bTraversing = LocomotionInput.bIsPlayingRootMotionMontage;
	if (bWasTraversing && !bTraversing)
	{
		FromTraversalTimer = FromTraversalTime;
	}
	else if (FromTraversalTimer > 0.f)
	{
		FromTraversalTimer -= DeltaSeconds;
	}
	bWasTraversing = bTraversing;

	const bool bMoving = (bHasAcceleration && Speed2D > MovingSpeedThreshold) || FutureVelocity.Size2D() > FutureSpeedThreshold;

	Query.MovementState = bMoving ? EPTMovementState::Moving : EPTMovementState::Idle;
	Query.Gait = LocomotionInput.Gait;
	Query.Stance = LocomotionInput.Stance;
	if (bIsFalling)
	{
		Query.Situation = EPTMovementSituation::InAir;
	}
	else if (FromTraversalTimer > 0.f)
	{
		Query.Situation = EPTMovementSituation::FromTraversal;
	}
	else if (bJustLanded)
	{
		Query.Situation = (-LandVelocity.Z >= HeavyLandSpeed) ? EPTMovementSituation::LandHeavy : EPTMovementSituation::LandLight;
	}
	else
	{
		Query.Situation = EPTMovementSituation::OnGround;
	}
	Query.RotationMode = LocomotionInput.RotationMode;

	// MM 노드의 Blend Time 핀에 바인딩
	MotionMatchingBlendTime = GetMotionMatchingBlendTime();
}

void UPTMotionMatchingAnimInstance::UpdateMotionMatching(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	EAnimNodeReferenceConversionResult Result = EAnimNodeReferenceConversionResult::Failed;
	const FMotionMatchingAnimNodeReference MMNode = UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNode(Node, Result);
	if (Result != EAnimNodeReferenceConversionResult::Succeeded || !DatabaseSet)
	{
		return;
	}

	TArray<UPoseSearchDatabase*> Databases;
	DatabaseSet->SelectDatabases(Query, Databases);
	if (Databases.Num() == 0)
	{
		return;
	}

	// 상태가 바뀌면 현재 재생 중인 데이터베이스가 목록에 없을 때 즉시 재검색
	const bool bQueryChanged = !bHasSearchedOnce || Query != LastSearchQuery;
	const EPoseSearchInterruptMode InterruptMode = bQueryChanged ? EPoseSearchInterruptMode::InterruptOnDatabaseChange : EPoseSearchInterruptMode::DoNotInterrupt;

	UMotionMatchingAnimNodeLibrary::SetDatabasesToSearch(MMNode, Databases, InterruptMode);

	if (bQueryChanged && PTMotionMatchingCVars::bLogDatabaseChanges)
	{
		UE_LOG(LogPTMotion, Log, TEXT("[%s] MM query: state=%d gait=%d stance=%d air=%d rot=%d -> %d database(s)"),
			*GetNameSafe(PTOwnerCharacter), (int32)Query.MovementState, (int32)Query.Gait, (int32)Query.Stance,
			(int32)Query.Situation, (int32)Query.RotationMode, Databases.Num());
	}

	LastSearchQuery = Query;
	bHasSearchedOnce = true;
}

void UPTMotionMatchingAnimInstance::UpdateMotionMatchingPostSelection(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	EAnimNodeReferenceConversionResult Result = EAnimNodeReferenceConversionResult::Failed;
	const FMotionMatchingAnimNodeReference MMNode = UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNode(Node, Result);
	if (Result != EAnimNodeReferenceConversionResult::Succeeded)
	{
		return;
	}

	FPoseSearchBlueprintResult SearchResult;
	bool bValid = false;
	UMotionMatchingAnimNodeLibrary::GetMotionMatchingSearchResult(MMNode, SearchResult, bValid);
	if (bValid && SearchResult.SelectedDatabase != CurrentDatabase)
	{
		CurrentDatabase = SearchResult.SelectedDatabase;
		UPoseSearchLibrary::GetDatabaseTags(CurrentDatabase, CurrentDatabaseTags);
	}
}

float UPTMotionMatchingAnimInstance::GetMotionMatchingBlendTime() const
{
	if (Query.Situation == EPTMovementSituation::InAir)
	{
		return InAirBlendTime;
	}
	if (bJustLanded)
	{
		return LandingBlendTime;
	}
	return GroundBlendTime;
}
