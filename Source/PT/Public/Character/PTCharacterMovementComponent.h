// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "Character/PTLocomotionTypes.h"
#include "PTCharacterMovementComponent.generated.h"

/**
 * 모션 매칭용 CharacterMovementComponent.
 *  - Gait(Walk/Run/Sprint), Stance, RotationMode 를 SavedMove 플래그로 클라이언트 예측 + 서버 권한 처리
 *  - 시뮬레이티드 프록시에서도 가속도(Acceleration)를 복제값으로 유지 -> 원격 캐릭터 궤적 예측 품질 향상
 */
UCLASS(Config = Game)
class PT_API UPTCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend class FSavedMove_PT;

public:
	UPTCharacterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ---- 입력 의도 (로컬 컨트롤러에서 설정) ----
	UFUNCTION(BlueprintCallable, Category = "PT|Movement")
	void SetWantsToSprint(bool bInWants) { bWantsToSprint = bInWants; }

	UFUNCTION(BlueprintCallable, Category = "PT|Movement")
	void SetWantsToWalk(bool bInWants) { bWantsToWalk = bInWants; }

	UFUNCTION(BlueprintCallable, Category = "PT|Movement")
	void SetWantsToStrafe(bool bInWants) { bWantsToStrafe = bInWants; }

	UFUNCTION(BlueprintPure, Category = "PT|Movement")
	bool WantsToSprint() const { return bWantsToSprint; }

	UFUNCTION(BlueprintPure, Category = "PT|Movement")
	bool WantsToWalk() const { return bWantsToWalk; }

	UFUNCTION(BlueprintPure, Category = "PT|Movement")
	bool WantsToStrafe() const { return bWantsToStrafe; }

	// ---- 현재 상태 ----
	UFUNCTION(BlueprintPure, Category = "PT|Movement")
	EPTGait GetGait() const;

	UFUNCTION(BlueprintPure, Category = "PT|Movement")
	EPTStance GetStance() const;

	UFUNCTION(BlueprintPure, Category = "PT|Movement")
	EPTRotationMode GetRotationMode() const;

	/** 전력질주 중 입력을 놓아 미끄러지며 멈추는 중 (시뮬레이티드 프록시도 복제값) */
	UFUNCTION(BlueprintPure, Category = "PT|Movement")
	bool IsSprintStopSliding() const { return bSprintStopSliding; }

	/** 제자리 회전 중인가 (로컬/서버) */
	UFUNCTION(BlueprintPure, Category = "PT|Movement")
	bool IsTurningInPlace() const { return bIsTurningInPlace; }

	/** 시뮬레이티드 프록시용: 서버에서 복제된 의도 플래그 적용 */
	void SetReplicatedIntentFlags(uint8 InFlags);
	uint8 GetIntentFlags() const;

	/** 시뮬레이티드 프록시용: 복제된 가속도 적용 */
	void SetReplicatedAcceleration(const FVector& InAcceleration);

	// ---- UCharacterMovementComponent ----
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void SimulateMovement(float DeltaTime) override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;
	virtual void PhysicsRotation(float DeltaTime) override;

	enum EIntentFlags : uint8
	{
		Intent_Sprint = 1 << 0,
		Intent_Walk   = 1 << 1,
		Intent_Strafe = 1 << 2,
		Intent_SprintStopSlide = 1 << 3,
	};

protected:
	const FPTGaitSettings& GetCurrentGaitSettings() const;
	bool CanSprintInCurrentState() const;
	/** 가속(또는 속도) 방향과 캐릭터 정면 사이 각도 */
	float GetMovementDirectionAngle() const;
	void ApplyRotationMode();
	bool ShouldHandleTurnInPlace() const;
	void UpdateTurnInPlace(float DeltaTime);
	void UpdateSprintStopSlide();

public:
	// ---- 튜닝 ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Gait")
	FPTGaitSettings WalkSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Gait")
	FPTGaitSettings RunSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Gait")
	FPTGaitSettings SprintSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Gait")
	FPTGaitSettings CrouchWalkSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Gait")
	FPTGaitSettings CrouchRunSettings;

	/** 스프린트가 허용되는 정면 기준 최대 각도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Gait", meta = (ClampMin = "0", ClampMax = "180"))
	float SprintMaxAngle = 50.f;

	/** 아날로그 입력 크기가 이 값보다 작으면 자동으로 Walk */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Gait", meta = (ClampMin = "0", ClampMax = "1"))
	float AnalogWalkThreshold = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Rotation")
	FRotator OrientToMovementRotationRate = FRotator(0.f, 500.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Rotation")
	FRotator StrafeRotationRate = FRotator(0.f, 360.f, 0.f);

	// ---- 전력질주 정지 슬라이드 ----

	/** 전력질주 중 이동 입력을 놓으면 짧게 미끄러지며 멈춤 (일반 달리기 정지는 즉시 멈춤) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|SprintStop")
	bool bEnableSprintStopSlide = true;

	/** 이 속도(cm/s) 이상에서 입력을 놓으면 슬라이드 시작 (달리기 최고속도보다 높게 → 전력질주에서만) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|SprintStop", meta = (ClampMin = "0"))
	float SprintStopSlideMinSpeed = 600.f;

	/** 슬라이드 중 감속 / 속도 비례 마찰 - 기본값으로 700cm/s 에서 약 0.4초, 1.3m 미끄러짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|SprintStop", meta = (ClampMin = "0"))
	float SprintStopSlideDeceleration = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|SprintStop", meta = (ClampMin = "0"))
	float SprintStopSlideFriction = 1.f;

	/** 이 속도 아래로 떨어지면 슬라이드 종료 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|SprintStop", meta = (ClampMin = "0"))
	float SprintStopSlideEndSpeed = 40.f;

	// ---- 제자리 회전 (Turn In Place) ----

	/** 정지 상태에서 카메라 방향과 몸 방향 차이가 TurnInPlaceStartAngle 을 넘으면 캡슐을 카메라 방향으로 회전 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|TurnInPlace")
	bool bEnableTurnInPlace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|TurnInPlace", meta = (ClampMin = "0", ClampMax = "180"))
	float TurnInPlaceStartAngle = 70.f;

	/** 이 각도 안으로 들어오면 회전 종료 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|TurnInPlace", meta = (ClampMin = "0", ClampMax = "90"))
	float TurnInPlaceStopAngle = 3.f;

	/** 캡슐 회전 속도 (도/초). 메시는 AnimNode_PTTurnInPlace 가 스텝으로 따라감 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|TurnInPlace", meta = (ClampMin = "1"))
	float TurnInPlaceRate = 300.f;

	/** 멈춘(또는 착지한) 뒤 이 시간이 지나야 제자리 회전 시작 - 멈춤/착지 동작과 겹치지 않게 (애님의 TurnEngageDelay 와 맞추기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|TurnInPlace", meta = (ClampMin = "0"))
	float TurnInPlaceStartDelay = 0.35f;

	/** 이 속도(cm/s) 미만이고 가속 입력이 없으면 '정지'로 판단 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|TurnInPlace", meta = (ClampMin = "0"))
	float TurnInPlaceMaxSpeed = 10.f;

protected:
	UPROPERTY(Transient)
	bool bWantsToSprint = false;

	UPROPERTY(Transient)
	bool bWantsToWalk = false;

	UPROPERTY(Transient)
	bool bWantsToStrafe = false;

	/** 슬라이드 상태 (SavedMove FLAG_Custom_3 로 예측/전송) */
	UPROPERTY(Transient)
	bool bSprintStopSliding = false;

	bool bHasReplicatedAcceleration = false;
	bool bIsTurningInPlace = false;
	float TurnInPlaceIdleTime = 0.f;
	bool bHasReplicatedIntent = false;
};

/** 커스텀 SavedMove */
class PT_API FSavedMove_PT : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;

	uint8 bSavedWantsToSprint : 1;
	uint8 bSavedWantsToWalk : 1;
	uint8 bSavedWantsToStrafe : 1;
	uint8 bSavedSprintStopSliding : 1;

	FSavedMove_PT() : bSavedWantsToSprint(0), bSavedWantsToWalk(0), bSavedWantsToStrafe(0), bSavedSprintStopSliding(0) {}
};

class PT_API FNetworkPredictionData_Client_PT : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	FNetworkPredictionData_Client_PT(const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};
