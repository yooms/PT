// Copyright yoomsGames. All Rights Reserved.

#include "Traversal/PTTraversalComponent.h"
#include "Traversal/PTTraversalAnimSet.h"
#include "Character/PTCharacter.h"
#include "Character/PTCharacterMovementComponent.h"
#include "AbilitySystem/PTAbilitySystemComponent.h"
#include "PTGameplayTags.h"
#include "PTLogChannels.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTTraversalComponent)

namespace PTTraversalCVars
{
	static bool bDebugDraw = false;
	static FAutoConsoleVariableRef CVarDebug(TEXT("pt.Traversal.Debug"), bDebugDraw, TEXT("Draw traversal (parkour) checks."), ECVF_Cheat);
}

UPTTraversalComponent::UPTTraversalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);

	// 템플릿 마네킹 팩의 점프 애니메이션 (GASP 몽타주 연결 전 임시)
	ProceduralVaultAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump.MM_Jump")));
	ProceduralMantleAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump.MM_Jump")));

	DefaultAnimSet = TSoftObjectPtr<UPTTraversalAnimSet>(FSoftObjectPath(TEXT("/Game/PT/Traversal/DA_PTTraversalAnimSet.DA_PTTraversalAnimSet")));
}

void UPTTraversalComponent::BeginPlay()
{
	Super::BeginPlay();

	// GASP 몽타주 세트 (서버/클라이언트 모두 같은 에셋을 로드해야 서버 검증이 통과)
	if (!AnimSet && !DefaultAnimSet.IsNull() && FPackageName::DoesPackageExist(DefaultAnimSet.ToSoftObjectPath().GetLongPackageName()))
	{
		AnimSet = DefaultAnimSet.LoadSynchronous();
	}

	// 실행 중 히치가 없도록 미리 로드 (서버/클라이언트 모두 재생하므로 모든 머신에서)
	if (bUseProceduralFallback)
	{
		for (const TSoftObjectPtr<UAnimSequenceBase>& Soft : { ProceduralVaultAnimation, ProceduralMantleAnimation })
		{
			if (!Soft.IsNull())
			{
				if (UAnimSequenceBase* Loaded = Soft.LoadSynchronous())
				{
					LoadedProceduralAnimations.AddUnique(Loaded);
				}
				else
				{
					UE_LOG(LogPT, Warning, TEXT("Traversal: procedural animation not found: %s"), *Soft.ToString());
				}
			}
		}
	}
}

UAnimSequenceBase* UPTTraversalComponent::GetProceduralAnimation(EPTTraversalAction Action) const
{
	return (Action == EPTTraversalAction::Mantle) ? ProceduralMantleAnimation.Get() : ProceduralVaultAnimation.Get();
}

bool UPTTraversalComponent::BuildProceduralPath(const FPTTraversalCheckResult& Result, TArray<FPTTraversalPathSegment>& OutSegments, FVector& OutExitVelocity) const
{
	OutSegments.Reset();
	OutExitVelocity = FVector::ZeroVector;

	const APTCharacter* Character = GetPTCharacter();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule || !Result.bHasFrontLedge)
	{
		return false;
	}

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Normal = Result.FrontLedgeNormal.GetSafeNormal2D();   // 장애물 → 캐릭터 쪽
	const FVector Fwd = -Normal;                                          // 진행 방향
	const float TopZ = Result.FrontLedgeLocation.Z;
	FVector Current = Character->GetActorLocation();

	auto AddSegment = [&](const FVector& Target, float Speed)
	{
		FPTTraversalPathSegment& Seg = OutSegments.AddDefaulted_GetRef();
		Seg.Target = Target;
		Seg.Duration = FMath::Max(FVector::Dist(Current, Target) / FMath::Max(Speed, 50.f), ProceduralMinSegmentTime);
		Current = Target;
	};

	switch (Result.Action)
	{
	case EPTTraversalAction::Hurdle:
	{
		if (!Result.bHasBackLedge || !Result.bHasBackFloor) { return false; }
		const float Speed = FMath::Max(Result.Speed, ProceduralHurdleSpeed);
		// 윗면 가운데 위로 (다리를 접은 높이) → 뒤쪽 바닥
		const FVector Mid = (Result.FrontLedgeLocation + Result.BackLedgeLocation) * 0.5f;
		AddSegment(FVector(Mid.X, Mid.Y, TopZ + HalfHeight - ProceduralFeetTuck), Speed);
		const FVector Land = Result.BackFloorLocation + Fwd * 30.f;
		AddSegment(FVector(Land.X, Land.Y, Result.BackFloorLocation.Z + HalfHeight + 2.f), Speed);
		OutExitVelocity = Fwd * Result.Speed * ProceduralHurdleExitSpeedRatio;
		break;
	}
	case EPTTraversalAction::Vault:
	{
		if (!Result.bHasBackLedge || !Result.bHasBackFloor) { return false; }
		const float Speed = FMath::Max(Result.Speed, ProceduralVaultSpeed);
		const float OverZ = TopZ + HalfHeight - ProceduralFeetTuck;
		// 앞 모서리를 짚고 → 윗면을 가로질러 → 뒤쪽 바닥
		const FVector Front = Result.FrontLedgeLocation + Fwd * 10.f;
		AddSegment(FVector(Front.X, Front.Y, OverZ), Speed);
		const FVector Back = Result.BackLedgeLocation + Fwd * (Radius * 0.5f);
		AddSegment(FVector(Back.X, Back.Y, OverZ), Speed);
		const FVector Land = Result.BackFloorLocation + Fwd * 20.f;
		AddSegment(FVector(Land.X, Land.Y, Result.BackFloorLocation.Z + HalfHeight + 2.f), Speed);
		OutExitVelocity = Fwd * Result.Speed * ProceduralVaultExitSpeedRatio;
		break;
	}
	case EPTTraversalAction::Mantle:
	{
		const float Speed = FMath::Max(Result.Speed, ProceduralMantleSpeed);
		const float StandZ = TopZ + HalfHeight + 2.f;
		// 벽 앞에서 윗면 높이까지 올라간 뒤 → 윗면 위로 올라서기
		const FVector Wall = Result.FrontLedgeLocation + Normal * (Radius + 2.f);
		AddSegment(FVector(Wall.X, Wall.Y, StandZ), Speed);
		const FVector Top = Result.FrontLedgeLocation + Fwd * (Radius + 15.f);
		AddSegment(FVector(Top.X, Top.Y, StandZ), ProceduralMantleSpeed * 0.8f);
		OutExitVelocity = FVector::ZeroVector;
		break;
	}
	default:
		return false;
	}

	if (PTTraversalCVars::bDebugDraw && GetWorld())
	{
		FVector Prev = Character->GetActorLocation();
		for (const FPTTraversalPathSegment& Seg : OutSegments)
		{
			DrawDebugLine(GetWorld(), Prev, Seg.Target, FColor::Orange, false, 3.f, 0, 2.f);
			DrawDebugSphere(GetWorld(), Seg.Target, 6.f, 6, FColor::Orange, false, 3.f);
			Prev = Seg.Target;
		}
	}
	return OutSegments.Num() > 0;
}

APTCharacter* UPTTraversalComponent::GetPTCharacter() const
{
	return Cast<APTCharacter>(GetOwner());
}

bool UPTTraversalComponent::TraceDown(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PTTraversalDown), false, GetOwner());
	const bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, Params);
	if (PTTraversalCVars::bDebugDraw)
	{
		DrawDebugLine(GetWorld(), Start, bHit ? OutHit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 3.f);
	}
	return bHit && !OutHit.bStartPenetrating;
}

bool UPTTraversalComponent::CapsuleFits(const FVector& Center, float Radius, float HalfHeight) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PTTraversalRoom), false, GetOwner());
	const bool bBlocked = GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, TraceChannel, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
	if (PTTraversalCVars::bDebugDraw)
	{
		DrawDebugCapsule(GetWorld(), Center, HalfHeight, Radius, FQuat::Identity, bBlocked ? FColor::Red : FColor::Cyan, false, 3.f);
	}
	return !bBlocked;
}

bool UPTTraversalComponent::PerformTraversalCheck(FPTTraversalCheckResult& OutResult) const
{
	OutResult = FPTTraversalCheckResult();

	const APTCharacter* Character = GetPTCharacter();
	const UPTCharacterMovementComponent* MoveComp = Character ? Character->GetPTMovementComponent() : nullptr;
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	UWorld* World = GetWorld();
	if (!Character || !MoveComp || !Capsule || !World)
	{
		return false;
	}

	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector ActorLocation = Character->GetActorLocation();
	const FVector Feet = ActorLocation - FVector(0.f, 0.f, CapsuleHalfHeight);
	const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	const float Speed = MoveComp->Velocity.Size2D();
	OutResult.Speed = Speed;

	// 1) 앞쪽 장애물
	const float TraceDistance = FMath::GetMappedRangeValueClamped(FVector2f(0.f, TraceDistanceSpeed), FVector2f(MinTraceDistance, MaxTraceDistance), Speed);
	FHitResult WallHit;
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PTTraversalForward), false, Character);
		const bool bHit = World->SweepSingleByChannel(WallHit, ActorLocation, ActorLocation + Forward * TraceDistance, FQuat::Identity, TraceChannel,
			FCollisionShape::MakeCapsule(TraceRadius, TraceHalfHeight), Params);
		if (PTTraversalCVars::bDebugDraw)
		{
			DrawDebugCapsule(World, bHit ? WallHit.Location : ActorLocation + Forward * TraceDistance, TraceHalfHeight, TraceRadius, FQuat::Identity, bHit ? FColor::Green : FColor::Red, false, 3.f);
		}
		if (!bHit || WallHit.bStartPenetrating)
		{
			return false;
		}
	}

	FVector WallNormal = WallHit.ImpactNormal;
	WallNormal.Z = 0.f;
	if (!WallNormal.Normalize() || FMath::Abs(WallHit.ImpactNormal.Z) > 0.4f || FVector::DotProduct(Forward, -WallNormal) < 0.5f)
	{
		return false; // 경사면 / 비스듬한 면
	}

	// 2) 윗면(앞 모서리) 높이
	const FVector IntoWall = WallHit.ImpactPoint - WallNormal * 5.f;
	FHitResult TopHit;
	if (!TraceDown(FVector(IntoWall.X, IntoWall.Y, Feet.Z + MaxMantleHeight + 10.f), FVector(IntoWall.X, IntoWall.Y, Feet.Z), TopHit) || TopHit.ImpactNormal.Z < 0.7f)
	{
		return false; // 너무 높거나 윗면이 걸을 수 없는 면
	}

	const float TopZ = TopHit.ImpactPoint.Z;
	OutResult.ObstacleHeight = TopZ - Feet.Z;
	if (OutResult.ObstacleHeight < MinObstacleHeight || OutResult.ObstacleHeight > MaxMantleHeight)
	{
		return false;
	}

	OutResult.bHasFrontLedge = true;
	OutResult.FrontLedgeLocation = FVector(WallHit.ImpactPoint.X, WallHit.ImpactPoint.Y, TopZ);
	OutResult.FrontLedgeNormal = WallNormal;
	OutResult.HitComponent = WallHit.GetComponent();

	// 3) 깊이: 윗면을 따라 들어가며 뒤쪽 끝 찾기
	auto IsOnTop = [this, TopZ](const FVector& XY)
	{
		FHitResult Hit;
		return TraceDown(FVector(XY.X, XY.Y, TopZ + 20.f), FVector(XY.X, XY.Y, TopZ - 30.f), Hit) && FMath::Abs(Hit.ImpactPoint.Z - TopZ) < 20.f;
	};

	const float Step = 10.f;
	float LastOnTop = 0.f;
	for (float Depth = Step; Depth <= MaxDepthCheck; Depth += Step)
	{
		if (IsOnTop(OutResult.FrontLedgeLocation - WallNormal * Depth))
		{
			LastOnTop = Depth;
			continue;
		}

		// 경계를 이분 탐색으로 다듬기
		float Lo = LastOnTop;
		float Hi = Depth;
		for (int32 i = 0; i < 4; ++i)
		{
			const float Mid = (Lo + Hi) * 0.5f;
			if (IsOnTop(OutResult.FrontLedgeLocation - WallNormal * Mid))
			{
				Lo = Mid;
			}
			else
			{
				Hi = Mid;
			}
		}

		OutResult.bHasBackLedge = true;
		OutResult.ObstacleDepth = Lo;
		OutResult.BackLedgeLocation = OutResult.FrontLedgeLocation - WallNormal * Lo;
		break;
	}

	// 4) 뒤쪽 바닥
	if (OutResult.bHasBackLedge)
	{
		const FVector Beyond = OutResult.BackLedgeLocation - WallNormal * (CapsuleRadius + 20.f);
		FHitResult FloorHit;
		if (TraceDown(FVector(Beyond.X, Beyond.Y, TopZ + 10.f), FVector(Beyond.X, Beyond.Y, Feet.Z - MaxBackFloorDrop), FloorHit) && FloorHit.ImpactNormal.Z > 0.7f)
		{
			OutResult.bHasBackFloor = true;
			OutResult.BackFloorLocation = FloorHit.ImpactPoint;
			OutResult.BackFloorHeight = FloorHit.ImpactPoint.Z - Feet.Z;
		}
	}

	const bool bOnGround = MoveComp->IsMovingOnGround();

	// 5) 동작 결정
	const bool bThin = OutResult.bHasBackLedge && OutResult.ObstacleDepth <= MaxVaultDepth;
	if (bOnGround && bThin && OutResult.bHasBackFloor && OutResult.ObstacleHeight <= MaxVaultHeight)
	{
		// 윗면 위로 지나갈 공간이 있는지
		const FVector TopMid = (OutResult.FrontLedgeLocation + OutResult.BackLedgeLocation) * 0.5f;
		if (CapsuleFits(TopMid + FVector(0.f, 0.f, CapsuleRadius + 5.f), CapsuleRadius * 0.8f, CapsuleRadius))
		{
			const bool bFast = Speed >= MinHurdleSpeed;
			OutResult.Action = (bFast && OutResult.BackFloorHeight >= MaxHurdleDrop) ? EPTTraversalAction::Hurdle : EPTTraversalAction::Vault;
		}
	}

	if (OutResult.Action == EPTTraversalAction::None)
	{
		// 올라탈 자리에 캡슐이 들어가는지
		const FVector Landing = OutResult.FrontLedgeLocation - WallNormal * (CapsuleRadius + 10.f) + FVector(0.f, 0.f, CapsuleHalfHeight + 2.f);
		if (CapsuleFits(Landing, CapsuleRadius, CapsuleHalfHeight))
		{
			OutResult.Action = EPTTraversalAction::Mantle;
		}
	}

	if (PTTraversalCVars::bDebugDraw)
	{
		DrawDebugSphere(World, OutResult.FrontLedgeLocation, 8.f, 8, FColor::Green, false, 3.f);
		if (OutResult.bHasBackLedge) { DrawDebugSphere(World, OutResult.BackLedgeLocation, 8.f, 8, FColor::Blue, false, 3.f); }
		if (OutResult.bHasBackFloor) { DrawDebugSphere(World, OutResult.BackFloorLocation, 8.f, 8, FColor::Yellow, false, 3.f); }
		UE_LOG(LogPT, Log, TEXT("Traversal: action=%d height=%.1f depth=%.1f backFloor=%.1f speed=%.0f"),
			(int32)OutResult.Action, OutResult.ObstacleHeight, OutResult.ObstacleDepth, OutResult.BackFloorHeight, Speed);
	}

	return OutResult.Action != EPTTraversalAction::None;
}

bool UPTTraversalComponent::TryTraversal()
{
	APTCharacter* Character = GetPTCharacter();
	UPTAbilitySystemComponent* ASC = Character ? Character->GetPTAbilitySystemComponent() : nullptr;
	if (!Character || !Character->IsLocallyControlled() || !ASC || (!AnimSet && !bUseProceduralFallback))
	{
		return false;
	}

	if (ASC->HasMatchingGameplayTag(PTGameplayTags::Status_Traversing) || ASC->HasMatchingGameplayTag(PTGameplayTags::Status_Dead))
	{
		return false;
	}

	FPTTraversalCheckResult Result;
	if (!PerformTraversalCheck(Result))
	{
		return false;
	}

	const UPTCharacterMovementComponent* MoveComp = Character->GetPTMovementComponent();
	const bool bInAir = MoveComp && MoveComp->IsFalling();
	const bool bHasMontage = AnimSet && AnimSet->SelectMontage(Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr, Character->GetGait(), bInAir, Result);
	if (!bHasMontage)
	{
		if (!bUseProceduralFallback)
		{
			if (PTTraversalCVars::bDebugDraw)
			{
				UE_LOG(LogPT, Warning, TEXT("Traversal: no montage for action %d (height %.1f) - check the TraversalAnimSet"), (int32)Result.Action, Result.ObstacleHeight);
			}
			return false;
		}

		// 몽타주 없음 → 절차적 경로로 실행
		Result.Montage = nullptr;
		Result.bProcedural = true;

		TArray<FPTTraversalPathSegment> Segments;
		FVector ExitVelocity;
		if (!BuildProceduralPath(Result, Segments, ExitVelocity))
		{
			return false;
		}
	}

	// 결과를 TargetData 로 담아 이벤트 발송 → LocalPredicted 어빌리티가 클라/서버에서 같은 데이터로 실행
	FPTGameplayAbilityTargetData_Traversal* TargetData = new FPTGameplayAbilityTargetData_Traversal();
	TargetData->Result = Result;

	FGameplayEventData Payload;
	Payload.EventTag = PTGameplayTags::Event_Traversal;
	Payload.Instigator = Character;
	Payload.Target = Character;
	Payload.TargetData.Add(TargetData);

	return ASC->HandleGameplayEvent(PTGameplayTags::Event_Traversal, &Payload) > 0;
}

bool UPTTraversalComponent::ValidateResult(const FPTTraversalCheckResult& Result) const
{
	const APTCharacter* Character = GetPTCharacter();
	if (!Character || !Result.IsValid())
	{
		return false;
	}
	if (Result.bProcedural ? !bUseProceduralFallback : (!AnimSet || !AnimSet->ContainsMontage(Result.Montage)))
	{
		return false;
	}

	const float Distance = FVector::Dist2D(Character->GetActorLocation(), Result.FrontLedgeLocation);
	const float Height = Result.ObstacleHeight;
	return Distance <= ServerValidateDistance && Height >= MinObstacleHeight - 10.f && Height <= MaxMantleHeight + 10.f;
}
