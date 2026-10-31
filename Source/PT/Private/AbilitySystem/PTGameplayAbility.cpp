// Copyright yoomsGames. All Rights Reserved.

#include "AbilitySystem/PTGameplayAbility.h"
#include "AbilitySystem/PTAbilitySystemComponent.h"
#include "Character/PTCharacter.h"
#include "Character/PTCharacterMovementComponent.h"
#include "Core/PTPlayerController.h"
#include "PTGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTGameplayAbility)

UPTGameplayAbility::UPTGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;

	ActivationBlockedTags.AddTag(PTGameplayTags::Status_Dead);
}

UPTAbilitySystemComponent* UPTGameplayAbility::GetPTAbilitySystemComponentFromActorInfo() const
{
	return CurrentActorInfo ? Cast<UPTAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()) : nullptr;
}

APTCharacter* UPTGameplayAbility::GetPTCharacterFromActorInfo() const
{
	return CurrentActorInfo ? Cast<APTCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

APTPlayerController* UPTGameplayAbility::GetPTPlayerControllerFromActorInfo() const
{
	return CurrentActorInfo ? Cast<APTPlayerController>(CurrentActorInfo->PlayerController.Get()) : nullptr;
}

UPTCharacterMovementComponent* UPTGameplayAbility::GetPTMovementComponentFromActorInfo() const
{
	const APTCharacter* Character = GetPTCharacterFromActorInfo();
	return Character ? Character->GetPTMovementComponent() : nullptr;
}

bool UPTGameplayAbility::IsLocallyControlledAvatar() const
{
	const APawn* Pawn = CurrentActorInfo ? Cast<APawn>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	return Pawn && Pawn->IsLocallyControlled();
}

void UPTGameplayAbility::TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const
{
	if (!ActorInfo || Spec.IsActive() || ActivationPolicy != EPTAbilityActivationPolicy::OnSpawn)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!ASC || !AvatarActor || AvatarActor->GetTearOff() || AvatarActor->GetLifeSpan() > 0.f)
	{
		return;
	}

	const bool bIsLocalExecution = (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted) || (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly);
	const bool bIsServerExecution = (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly) || (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated);

	const bool bClientShouldActivate = ActorInfo->IsLocallyControlled() && bIsLocalExecution;
	const bool bServerShouldActivate = ActorInfo->IsNetAuthority() && bIsServerExecution;

	if (bClientShouldActivate || bServerShouldActivate)
	{
		ASC->TryActivateAbility(Spec.Handle);
	}
}

void UPTGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	TryActivateAbilityOnSpawn(ActorInfo, Spec);
}

void UPTGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	TryActivateAbilityOnSpawn(ActorInfo, Spec);
}
