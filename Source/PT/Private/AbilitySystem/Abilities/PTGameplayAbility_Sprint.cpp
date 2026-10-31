// Copyright NeedsGames. All Rights Reserved.

#include "AbilitySystem/Abilities/PTGameplayAbility_Sprint.h"
#include "AbilitySystem/Attributes/PTStaminaSet.h"
#include "AbilitySystemComponent.h"
#include "Character/PTCharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "PTGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTGameplayAbility_Sprint)

UPTGameplayAbility_Sprint::UPTGameplayAbility_Sprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EPTAbilityActivationPolicy::WhileInputActive;

	FGameplayTagContainer Tags;
	Tags.AddTag(PTGameplayTags::Ability_Movement_Sprint);
	SetAssetTags(Tags);

	ActivationOwnedTags.AddTag(PTGameplayTags::Status_Sprinting);
	ActivationBlockedTags.AddTag(PTGameplayTags::Ability_Block_Movement);
}

bool UPTGameplayAbility_Sprint::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (StaminaCostPerSecond > 0.f && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		bool bFound = false;
		const float Stamina = ActorInfo->AbilitySystemComponent->GetGameplayAttributeValue(UPTStaminaSet::GetStaminaAttribute(), bFound);
		if (bFound && Stamina < FMath::Max(MinStaminaToStart, KINDA_SMALL_NUMBER))
		{
			return false;
		}
	}
	return true;
}

void UPTGameplayAbility_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (IsLocallyControlledAvatar())
	{
		if (UPTCharacterMovementComponent* MoveComp = GetPTMovementComponentFromActorInfo())
		{
			MoveComp->SetWantsToSprint(true);
		}
	}

	if (StaminaCostPerSecond > 0.f && ActorInfo && ActorInfo->IsNetAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(StaminaTimerHandle, this, &ThisClass::TickStaminaCost, StaminaTickInterval, true);
		}
	}
}

void UPTGameplayAbility_Sprint::TickStaminaCost()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !ASC->HasAttributeSetForAttribute(UPTStaminaSet::GetStaminaAttribute()))
	{
		return;
	}

	const float Cost = StaminaCostPerSecond * StaminaTickInterval;
	ASC->ApplyModToAttribute(UPTStaminaSet::GetStaminaAttribute(), EGameplayModOp::Additive, -Cost);

	if (ASC->GetNumericAttribute(UPTStaminaSet::GetStaminaAttribute()) <= 0.f)
	{
		// 서버에서 종료 -> 클라이언트에 복제되어 로컬 EndAbility 에서 스프린트 플래그 해제
		CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
	}
}

void UPTGameplayAbility_Sprint::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UPTGameplayAbility_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaminaTimerHandle);
	}

	if (IsLocallyControlledAvatar())
	{
		if (UPTCharacterMovementComponent* MoveComp = GetPTMovementComponentFromActorInfo())
		{
			MoveComp->SetWantsToSprint(false);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
