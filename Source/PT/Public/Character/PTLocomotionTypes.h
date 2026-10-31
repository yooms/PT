// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PTLocomotionTypes.generated.h"

UENUM(BlueprintType)
enum class EPTGait : uint8
{
	Walk,
	Run,
	Sprint
};

UENUM(BlueprintType)
enum class EPTStance : uint8
{
	Stand,
	Crouch
};

UENUM(BlueprintType)
enum class EPTRotationMode : uint8
{
	/** 이동 방향으로 회전 (기본 3인칭) */
	OrientToMovement,
	/** 컨트롤러(카메라) 방향 고정 - 스트레이프 */
	Strafe
};

UENUM(BlueprintType)
enum class EPTMovementState : uint8
{
	Idle,
	Moving
};

UENUM(BlueprintType)
enum class EPTMovementSituation : uint8
{
	OnGround,
	InAir,
	/** 착지 직후 (가벼운 착지) */
	LandLight,
	/** 착지 직후 (높은 곳에서 떨어진 무거운 착지) */
	LandHeavy,
	/** 파쿠르 몽타주가 끝난 직후 - GASP 의 *_FromTraversal 데이터베이스로 자연스럽게 이어짐 */
	FromTraversal
};

/** 방향별 최대 속도 (스트레이프 모드에서 이동 방향에 따라 보간) */
USTRUCT(BlueprintType)
struct FPTDirectionalSpeed
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float Forward = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float Strafe = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float Backward = 400.f;

	/** AngleDegrees: 0 = 정면, 90 = 측면, 180 = 후방 */
	float Evaluate(float AngleDegrees) const
	{
		const float A = FMath::Clamp(FMath::Abs(AngleDegrees), 0.f, 180.f);
		return (A <= 90.f) ? FMath::Lerp(Forward, Strafe, A / 90.f) : FMath::Lerp(Strafe, Backward, (A - 90.f) / 90.f);
	}
};

/** 보행(Gait) 별 이동 파라미터 - 모션 매칭 애니메이션의 실제 속도와 맞춰 튜닝하세요 (GASP 기준값) */
USTRUCT(BlueprintType)
struct FPTGaitSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	FPTDirectionalSpeed Speed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait", meta = (ClampMin = "0"))
	float MaxAcceleration = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait", meta = (ClampMin = "0"))
	float BrakingDeceleration = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait", meta = (ClampMin = "0"))
	float GroundFriction = 5.f;

	/** 입력을 놓았을 때(정지 중) 속도에 비례해 추가로 감속하는 마찰. 클수록 빨리 멈춤 (미끄러짐 감소) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait", meta = (ClampMin = "0"))
	float BrakingFriction = 8.f;
};
