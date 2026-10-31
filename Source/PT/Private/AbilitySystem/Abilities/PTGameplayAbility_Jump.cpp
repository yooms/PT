// Copyright yoomsGames. All Rights Reserved.

#include "AbilitySystem/Abilities/PTGameplayAbility_Jump.h"
#include "Character/PTCharacter.h"
#include "PTGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTGameplayAbility_Jump)

UPTGameplayAbility_Jump::UPTGameplayAbility_Jump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EPTAbilityActivationPolicy::OnInputTriggered;

	FGameplayTagContainer Tags;
	Tags.AddTag(PTGameplayTags::Ability_Movement_Jump);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(PTGameplayTags::Ability_Block_Movement);
}

bool UPTGameplayAbility_Jump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	const APTCharacter* Character = Cast<APTCharacter>(ActorInfo->AvatarActor.Get());
	if (!Character || !Character->CanJump())
	{
		return false;
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UPTGameplayAbility_Jump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 점프 자체는 CharacterMovement 가 예측/복제하므로 로컬 컨트롤러에서만 호출
	if (APTCharacter* Character = GetPTCharacterFromActorInfo())
	{
		if (Character->IsLocallyControlled() && !Character->bPressedJump)
		{
			Character->UnCrouch();
			Character->Jump();
		}
	}
}

void UPTGameplayAbility_Jump::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UPTGameplayAbility_Jump::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (APTCharacter* Character = GetPTCharacterFromActorInfo())
	{
		if (Character->IsLocallyControlled() && Character->bPressedJump)
		{
			Character->StopJumping();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
