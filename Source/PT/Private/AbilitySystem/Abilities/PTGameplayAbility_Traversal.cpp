// Copyright NeedsGames. All Rights Reserved.

#include "AbilitySystem/Abilities/PTGameplayAbility_Traversal.h"
#include "Character/PTCharacter.h"
#include "Character/PTCharacterMovementComponent.h"
#include "Traversal/PTTraversalAnimSet.h"
#include "Traversal/PTTraversalComponent.h"
#include "PTGameplayTags.h"
#include "PTLogChannels.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/RootMotionSource.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTGameplayAbility_Traversal)

UPTGameplayAbility_Traversal::UPTGameplayAbility_Traversal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EPTAbilityActivationPolicy::OnInputTriggered;

	FGameplayTagContainer Tags;
	Tags.AddTag(PTGameplayTags::Ability_Movement_Traversal);
	SetAssetTags(Tags);

	ActivationOwnedTags.AddTag(PTGameplayTags::Status_Traversing);
	ActivationBlockedTags.AddTag(PTGameplayTags::Status_Traversing);
	ActivationBlockedTags.AddTag(PTGameplayTags::Ability_Block_Movement);

	// 실행 중 점프는 막음
	BlockAbilitiesWithTag.AddTag(PTGameplayTags::Ability_Movement_Jump);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = PTGameplayTags::Event_Traversal;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UPTGameplayAbility_Traversal::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	APTCharacter* Character = GetPTCharacterFromActorInfo();
	UPTTraversalComponent* Traversal = Character ? Character->FindComponentByClass<UPTTraversalComponent>() : nullptr;

	// 이벤트에 실려 온 판정 결과
	const FPTGameplayAbilityTargetData_Traversal* Data = nullptr;
	if (TriggerEventData && TriggerEventData->TargetData.Num() > 0)
	{
		const FGameplayAbilityTargetData* Raw = TriggerEventData->TargetData.Get(0);
		if (Raw && Raw->GetScriptStruct() == FPTGameplayAbilityTargetData_Traversal::StaticStruct())
		{
			Data = static_cast<const FPTGameplayAbilityTargetData_Traversal*>(Raw);
		}
	}

	if (!Character || !Traversal || !Data || !Data->Result.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 서버: 클라이언트 데이터 검증
	if (ActorInfo->IsNetAuthority() && !Traversal->ValidateResult(Data->Result))
	{
		UE_LOG(LogPTAbility, Warning, TEXT("Traversal rejected by server validation (%s)"), *GetNameSafe(Character));
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveResult = Data->Result;

	// 이동: 루트 모션이 위아래로 움직일 수 있도록 Flying, 장애물과의 충돌 무시
	if (UPTCharacterMovementComponent* MoveComp = Character->GetPTMovementComponent())
	{
		MoveComp->SetMovementMode(MOVE_Flying);
		MoveComp->Velocity = FVector::ZeroVector;
		bMovementModified = true;
	}
	if (ActiveResult.HitComponent)
	{
		Character->GetCapsuleComponent()->IgnoreComponentWhenMoving(ActiveResult.HitComponent, true);
		IgnoredComponent = ActiveResult.HitComponent;
	}

	if (ActiveResult.bProcedural)
	{
		if (!StartProcedural(Character))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		}
		return;
	}

	ApplyWarpTargets(ActiveResult);

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, ActiveResult.Montage, ActiveResult.MontagePlayRate, NAME_None, true, 1.f, ActiveResult.MontageStartTime);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->ReadyForActivation();
}

bool UPTGameplayAbility_Traversal::StartProcedural(APTCharacter* Character)
{
	const UPTTraversalComponent* Traversal = Character ? Character->FindComponentByClass<UPTTraversalComponent>() : nullptr;
	if (!Traversal || !Traversal->BuildProceduralPath(ActiveResult, ProceduralSegments, ProceduralExitVelocity))
	{
		return false;
	}

	float TotalDuration = 0.f;
	for (const FPTTraversalPathSegment& Seg : ProceduralSegments)
	{
		TotalDuration += Seg.Duration;
	}

	// 애니메이션: 전체 이동 시간에 맞춰 재생 속도 조절 (ASC 가 시뮬레이티드 프록시에도 복제)
	if (UAnimSequenceBase* Anim = Traversal->GetProceduralAnimation(ActiveResult.Action))
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			const float Length = Anim->GetPlayLength();
			const float PlayRate = (TotalDuration > KINDA_SMALL_NUMBER) ? FMath::Clamp(Length / (TotalDuration + 0.15f), 0.6f, 1.8f) : 1.f;
			ASC->PlaySlotAnimationAsDynamicMontage(this, CurrentActivationInfo, Anim, Traversal->ProceduralSlotName, 0.1f, 0.25f, PlayRate, 0.f);
		}
	}

	// 경로가 어떤 이유로 끝나지 않아도 어빌리티가 남지 않도록
	if (UAbilityTask_WaitDelay* Safety = UAbilityTask_WaitDelay::WaitDelay(this, TotalDuration + 0.75f))
	{
		Safety->OnFinish.AddDynamic(this, &ThisClass::OnProceduralSafetyTimeout);
		Safety->ReadyForActivation();
	}

	ProceduralSegmentIndex = INDEX_NONE;
	StartNextProceduralSegment();
	return true;
}

void UPTGameplayAbility_Traversal::StartNextProceduralSegment()
{
	if (!IsActive())
	{
		return;
	}

	++ProceduralSegmentIndex;
	if (!ProceduralSegments.IsValidIndex(ProceduralSegmentIndex))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const FPTTraversalPathSegment& Seg = ProceduralSegments[ProceduralSegmentIndex];
	const bool bLast = (ProceduralSegmentIndex == ProceduralSegments.Num() - 1);

	// 중간 구간은 속도 유지(다음 구간으로 자연스럽게), 마지막 구간은 지정한 탈출 속도
	UAbilityTask_ApplyRootMotionMoveToForce* Task = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, NAME_None, Seg.Target, Seg.Duration,
		/*bSetNewMovementMode*/ false, MOVE_Flying,
		/*bRestrictSpeedToExpected*/ false, /*PathOffsetCurve*/ nullptr,
		bLast ? ERootMotionFinishVelocityMode::SetVelocity : ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity,
		bLast ? ProceduralExitVelocity : FVector::ZeroVector, 0.f);
	if (!Task)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	Task->OnTimedOut.AddDynamic(this, &ThisClass::StartNextProceduralSegment);
	Task->OnTimedOutAndDestinationReached.AddDynamic(this, &ThisClass::StartNextProceduralSegment);
	Task->ReadyForActivation();
}

void UPTGameplayAbility_Traversal::OnProceduralSafetyTimeout()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UPTGameplayAbility_Traversal::ApplyWarpTargets(const FPTTraversalCheckResult& Result)
{
	APTCharacter* Character = GetPTCharacterFromActorInfo();
	UMotionWarpingComponent* Warping = Character ? Character->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	const UPTTraversalComponent* Traversal = Character ? Character->FindComponentByClass<UPTTraversalComponent>() : nullptr;
	const UPTTraversalAnimSet* AnimSet = Traversal ? Traversal->GetAnimSet() : nullptr;
	if (!Warping || !AnimSet)
	{
		return;
	}

	const FRotator Facing = Result.GetWarpRotation();
	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(AnimSet->FrontLedgeWarpName, Result.FrontLedgeLocation, Facing);

	if (Result.bHasBackLedge && Result.Action != EPTTraversalAction::Mantle)
	{
		Warping->AddOrUpdateWarpTargetFromLocationAndRotation(AnimSet->BackLedgeWarpName, Result.BackLedgeLocation, Facing);
	}
	else
	{
		Warping->RemoveWarpTarget(AnimSet->BackLedgeWarpName);
	}

	if (Result.bHasBackFloor && Result.Action == EPTTraversalAction::Vault)
	{
		Warping->AddOrUpdateWarpTargetFromLocationAndRotation(AnimSet->BackFloorWarpName, Result.BackFloorLocation, Facing);
	}
	else
	{
		Warping->RemoveWarpTarget(AnimSet->BackFloorWarpName);
	}
}

void UPTGameplayAbility_Traversal::RestoreMovement()
{
	APTCharacter* Character = GetPTCharacterFromActorInfo();
	if (!Character)
	{
		return;
	}

	if (IgnoredComponent)
	{
		Character->GetCapsuleComponent()->IgnoreComponentWhenMoving(IgnoredComponent, false);
		IgnoredComponent = nullptr;
	}

	if (bMovementModified)
	{
		if (UPTCharacterMovementComponent* MoveComp = Character->GetPTMovementComponent())
		{
			// Walking 은 바닥이 없으면 다음 틱에 자동으로 Falling 으로 전환됨
			if (MoveComp->MovementMode == MOVE_Flying)
			{
				MoveComp->SetMovementMode(MOVE_Walking);
			}
		}
		bMovementModified = false;
	}
}

void UPTGameplayAbility_Traversal::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPTGameplayAbility_Traversal::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UPTGameplayAbility_Traversal::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RestoreMovement();
	ActiveResult = FPTTraversalCheckResult();
	ProceduralSegments.Reset();
	ProceduralSegmentIndex = INDEX_NONE;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
