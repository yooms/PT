// Copyright yoomsGames. All Rights Reserved.

#include "Character/PTCharacterMovementComponent.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTCharacterMovementComponent)

UPTCharacterMovementComponent::UPTCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// GASP(Game Animation Sample) 기본값에 가까운 값. 애니메이션 데이터의 실제 속도에 맞춰 조정하세요.
	WalkSettings.Speed = { 200.f, 175.f, 150.f };
	WalkSettings.MaxAcceleration = 600.f;
	WalkSettings.BrakingDeceleration = 1500.f;
	WalkSettings.GroundFriction = 5.f;

	RunSettings.Speed = { 500.f, 450.f, 400.f };
	RunSettings.MaxAcceleration = 800.f;
	RunSettings.BrakingDeceleration = 2000.f;
	RunSettings.GroundFriction = 5.f;

	SprintSettings.Speed = { 700.f, 700.f, 700.f };
	SprintSettings.MaxAcceleration = 800.f;
	SprintSettings.BrakingDeceleration = 2200.f;
	SprintSettings.GroundFriction = 5.f;

	CrouchWalkSettings.Speed = { 200.f, 180.f, 150.f };
	CrouchWalkSettings.MaxAcceleration = 500.f;
	CrouchWalkSettings.BrakingDeceleration = 1500.f;

	CrouchRunSettings.Speed = { 300.f, 275.f, 250.f };
	CrouchRunSettings.MaxAcceleration = 600.f;
	CrouchRunSettings.BrakingDeceleration = 1800.f;

	MaxWalkSpeed = RunSettings.Speed.Forward;
	MaxWalkSpeedCrouched = CrouchRunSettings.Speed.Forward;
	MaxAcceleration = RunSettings.MaxAcceleration;
	BrakingDecelerationWalking = RunSettings.BrakingDeceleration;
	BrakingFrictionFactor = 1.f;
	bUseSeparateBrakingFriction = true;
	// 정지 시 감속: 일정 감속(BrakingDeceleration) + 속도 비례 마찰(BrakingFriction)
	// 달리다 멈출 때 약 0.15초 / 30cm 안에 정지 → 발 미끄러짐 최소화 (GASP 정지 애니메이션 사용 시 값을 낮춰 조정)
	BrakingFriction = RunSettings.BrakingFriction;

	JumpZVelocity = 500.f;
	AirControl = 0.25f;
	GravityScale = 1.f;

	bOrientRotationToMovement = true;
	bUseControllerDesiredRotation = false;
	RotationRate = OrientToMovementRotationRate;

	GetNavAgentPropertiesRef().bCanCrouch = true;
	bCanWalkOffLedgesWhenCrouching = true;
}

// ---------------------------------------------------------------------------------------------
// 상태
// ---------------------------------------------------------------------------------------------

EPTGait UPTCharacterMovementComponent::GetGait() const
{
	if (bHasReplicatedIntent && CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// 서버가 이미 확정한 Gait 를 그대로 사용
		return bWantsToSprint ? EPTGait::Sprint : (bWantsToWalk ? EPTGait::Walk : EPTGait::Run);
	}

	if (bWantsToSprint && CanSprintInCurrentState())
	{
		return EPTGait::Sprint;
	}

	const float Analog = GetAnalogInputModifier();
	if (bWantsToWalk || (Analog > KINDA_SMALL_NUMBER && Analog < AnalogWalkThreshold))
	{
		return EPTGait::Walk;
	}

	return EPTGait::Run;
}

EPTStance UPTCharacterMovementComponent::GetStance() const
{
	return IsCrouching() ? EPTStance::Crouch : EPTStance::Stand;
}

EPTRotationMode UPTCharacterMovementComponent::GetRotationMode() const
{
	return bWantsToStrafe ? EPTRotationMode::Strafe : EPTRotationMode::OrientToMovement;
}

uint8 UPTCharacterMovementComponent::GetIntentFlags() const
{
	uint8 Flags = 0;
	const EPTGait Gait = GetGait();
	if (Gait == EPTGait::Sprint) { Flags |= Intent_Sprint; }
	if (Gait == EPTGait::Walk)   { Flags |= Intent_Walk; }
	if (bWantsToStrafe)          { Flags |= Intent_Strafe; }
	if (bSprintStopSliding)      { Flags |= Intent_SprintStopSlide; }
	return Flags;
}

void UPTCharacterMovementComponent::SetReplicatedIntentFlags(uint8 InFlags)
{
	bHasReplicatedIntent = true;
	bWantsToSprint = (InFlags & Intent_Sprint) != 0;
	bWantsToWalk = (InFlags & Intent_Walk) != 0;
	bWantsToStrafe = (InFlags & Intent_Strafe) != 0;
	bSprintStopSliding = (InFlags & Intent_SprintStopSlide) != 0;
}

void UPTCharacterMovementComponent::SetReplicatedAcceleration(const FVector& InAcceleration)
{
	bHasReplicatedAcceleration = true;
	Acceleration = InAcceleration;
}

const FPTGaitSettings& UPTCharacterMovementComponent::GetCurrentGaitSettings() const
{
	const EPTGait Gait = GetGait();
	if (IsCrouching())
	{
		return (Gait == EPTGait::Walk) ? CrouchWalkSettings : CrouchRunSettings;
	}

	switch (Gait)
	{
	case EPTGait::Walk:   return WalkSettings;
	case EPTGait::Sprint: return SprintSettings;
	default:              return RunSettings;
	}
}

bool UPTCharacterMovementComponent::CanSprintInCurrentState() const
{
	if (IsCrouching())
	{
		return false;
	}

	if (GetRotationMode() == EPTRotationMode::Strafe)
	{
		return GetMovementDirectionAngle() <= SprintMaxAngle;
	}
	return true;
}

float UPTCharacterMovementComponent::GetMovementDirectionAngle() const
{
	if (!UpdatedComponent)
	{
		return 0.f;
	}

	FVector Direction = Acceleration.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Velocity.GetSafeNormal2D();
	}
	if (Direction.IsNearlyZero())
	{
		return 0.f;
	}

	const float ForwardYaw = UpdatedComponent->GetComponentRotation().Yaw;
	const float DirectionYaw = Direction.Rotation().Yaw;
	return FMath::Abs(FMath::FindDeltaAngleDegrees(ForwardYaw, DirectionYaw));
}

void UPTCharacterMovementComponent::ApplyRotationMode()
{
	const bool bStrafe = (GetRotationMode() == EPTRotationMode::Strafe);
	bOrientRotationToMovement = !bStrafe;
	bUseControllerDesiredRotation = bStrafe;
	RotationRate = bStrafe ? StrafeRotationRate : OrientToMovementRotationRate;
}

// ---------------------------------------------------------------------------------------------
// UCharacterMovementComponent
// ---------------------------------------------------------------------------------------------

float UPTCharacterMovementComponent::GetMaxSpeed() const
{
	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
	case MOVE_Falling:
	{
		const float Angle = (GetRotationMode() == EPTRotationMode::Strafe) ? GetMovementDirectionAngle() : 0.f;
		return GetCurrentGaitSettings().Speed.Evaluate(Angle);
	}
	default:
		return Super::GetMaxSpeed();
	}
}

float UPTCharacterMovementComponent::GetMaxAcceleration() const
{
	if (IsMovingOnGround())
	{
		return GetCurrentGaitSettings().MaxAcceleration;
	}
	return Super::GetMaxAcceleration();
}

float UPTCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		return bSprintStopSliding ? SprintStopSlideDeceleration : GetCurrentGaitSettings().BrakingDeceleration;
	}
	return Super::GetMaxBrakingDeceleration();
}

void UPTCharacterMovementComponent::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	// bUseSeparateBrakingFriction 이면 공중에서도 BrakingFriction 이 쓰이므로,
	// 지상 정지용 마찰은 걷기 모드에서만 적용 (점프 중 입력을 놓아도 관성 유지)
	if (MovementMode != MOVE_Walking && MovementMode != MOVE_NavWalking)
	{
		Friction = 0.f;
	}
	else if (bSprintStopSliding)
	{
		Friction = SprintStopSlideFriction;
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

void UPTCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToWalk   = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bWantsToStrafe = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
	// 이동 시작 시점의 슬라이드 상태 (클라이언트와 같은 상태에서 시작 → 같은 결과)
	bSprintStopSliding = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;
}

void UPTCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	GroundFriction = GetCurrentGaitSettings().GroundFriction;
	BrakingFriction = GetCurrentGaitSettings().BrakingFriction;
	UpdateSprintStopSlide();
	ApplyRotationMode();
}

void UPTCharacterMovementComponent::UpdateSprintStopSlide()
{
	const bool bNoInput = Acceleration.SizeSquared2D() < KINDA_SMALL_NUMBER;
	const float Speed2D = Velocity.Size2D();

	if (!bEnableSprintStopSlide || !IsMovingOnGround() || !bNoInput || IsCrouching() || HasAnimRootMotion() || CurrentRootMotion.HasActiveRootMotionSources())
	{
		bSprintStopSliding = false;
	}
	else if (bSprintStopSliding)
	{
		bSprintStopSliding = Speed2D > SprintStopSlideEndSpeed;
	}
	else
	{
		// 달리기로는 낼 수 없는 속도에서 입력을 놓았을 때만 = 전력질주 정지
		bSprintStopSliding = Speed2D >= SprintStopSlideMinSpeed;
	}
}

void UPTCharacterMovementComponent::SimulateMovement(float DeltaTime)
{
	if (bHasReplicatedAcceleration)
	{
		// SimulateMovement 가 Acceleration 을 덮어쓰지 않도록 보존
		const FVector OriginalAcceleration = Acceleration;
		Super::SimulateMovement(DeltaTime);
		Acceleration = OriginalAcceleration;
	}
	else
	{
		Super::SimulateMovement(DeltaTime);
	}
}

// ---------------------------------------------------------------------------------------------
// Turn In Place
// ---------------------------------------------------------------------------------------------

bool UPTCharacterMovementComponent::ShouldHandleTurnInPlace() const
{
	if (!bEnableTurnInPlace || !CharacterOwner || !CharacterOwner->Controller || !UpdatedComponent)
	{
		return false;
	}
	if (!IsMovingOnGround() || HasAnimRootMotion() || CharacterOwner->IsPlayingNetworkedRootMotionMontage())
	{
		return false;
	}
	// 이동 입력이 없고 거의 멈춘 상태
	return Acceleration.SizeSquared2D() < KINDA_SMALL_NUMBER && Velocity.SizeSquared2D() < FMath::Square(TurnInPlaceMaxSpeed);
}

void UPTCharacterMovementComponent::UpdateTurnInPlace(float DeltaTime)
{
	const FRotator CurrentRotation = UpdatedComponent->GetComponentRotation();
	const float TargetYaw = CharacterOwner->Controller->GetDesiredRotation().Yaw;
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetYaw);

	if (!bIsTurningInPlace && FMath::Abs(DeltaYaw) > TurnInPlaceStartAngle)
	{
		bIsTurningInPlace = true;
	}

	if (!bIsTurningInPlace)
	{
		return;
	}

	if (FMath::Abs(DeltaYaw) <= TurnInPlaceStopAngle)
	{
		bIsTurningInPlace = false;
		return;
	}

	const float MaxStep = TurnInPlaceRate * DeltaTime;
	const float Step = FMath::Clamp(DeltaYaw, -MaxStep, MaxStep);
	const FRotator NewRotation(0.f, FRotator::NormalizeAxis(CurrentRotation.Yaw + Step), 0.f);
	MoveUpdatedComponent(FVector::ZeroVector, NewRotation, /*bSweep*/ false);
}

void UPTCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (ShouldHandleTurnInPlace())
	{
		// 정지 중에는 기본 회전(이동 방향 / 컨트롤러 방향 연속 회전) 대신 제자리 회전 규칙 사용
		TurnInPlaceIdleTime += DeltaTime;
		if (TurnInPlaceIdleTime >= TurnInPlaceStartDelay)
		{
			UpdateTurnInPlace(DeltaTime);
		}
		return;
	}

	TurnInPlaceIdleTime = 0.f;
	bIsTurningInPlace = false;
	Super::PhysicsRotation(DeltaTime);
}

bool UPTCharacterMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	// 리플레이 동안 SavedMove 값으로 덮어쓰이므로 현재 입력 의도를 보존 후 복원 (엔진의 bWantsToCrouch 처리와 동일)
	const bool bRealSprint = bWantsToSprint;
	const bool bRealWalk = bWantsToWalk;
	const bool bRealStrafe = bWantsToStrafe;

	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();

	bWantsToSprint = bRealSprint;
	bWantsToWalk = bRealWalk;
	bWantsToStrafe = bRealStrafe;
	return bResult;
}

FNetworkPredictionData_Client* UPTCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UPTCharacterMovementComponent* MutableThis = const_cast<UPTCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_PT(*this);
	}
	return ClientPredictionData;
}

// ---------------------------------------------------------------------------------------------
// Saved Move
// ---------------------------------------------------------------------------------------------

void FSavedMove_PT::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = 0;
	bSavedWantsToWalk = 0;
	bSavedWantsToStrafe = 0;
	bSavedSprintStopSliding = 0;
}

uint8 FSavedMove_PT::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSprint) { Result |= FLAG_Custom_0; }
	if (bSavedWantsToWalk)   { Result |= FLAG_Custom_1; }
	if (bSavedWantsToStrafe) { Result |= FLAG_Custom_2; }
	if (bSavedSprintStopSliding) { Result |= FLAG_Custom_3; }
	return Result;
}

bool FSavedMove_PT::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_PT* Other = static_cast<const FSavedMove_PT*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint
		|| bSavedWantsToWalk != Other->bSavedWantsToWalk
		|| bSavedWantsToStrafe != Other->bSavedWantsToStrafe
		|| bSavedSprintStopSliding != Other->bSavedSprintStopSliding)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_PT::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UPTCharacterMovementComponent* MoveComp = Cast<UPTCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = MoveComp->bWantsToSprint;
		bSavedWantsToWalk = MoveComp->bWantsToWalk;
		bSavedWantsToStrafe = MoveComp->bWantsToStrafe;
		bSavedSprintStopSliding = MoveComp->bSprintStopSliding;
	}
}

void FSavedMove_PT::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UPTCharacterMovementComponent* MoveComp = Cast<UPTCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		MoveComp->bWantsToSprint = bSavedWantsToSprint;
		MoveComp->bWantsToWalk = bSavedWantsToWalk;
		MoveComp->bWantsToStrafe = bSavedWantsToStrafe;
		MoveComp->bSprintStopSliding = bSavedSprintStopSliding;
	}
}

FNetworkPredictionData_Client_PT::FNetworkPredictionData_Client_PT(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_PT::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_PT());
}
