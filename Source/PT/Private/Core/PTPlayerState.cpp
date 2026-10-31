// Copyright yoomsGames. All Rights Reserved.

#include "Core/PTPlayerState.h"
#include "AbilitySystem/PTAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/PTHealthSet.h"
#include "AbilitySystem/Attributes/PTStaminaSet.h"
#include "Character/PTCharacter.h"
#include "PTGameplayTags.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTPlayerState)

APTPlayerState::APTPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<UPTAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// 기본 어트리뷰트 셋 (서브오브젝트로 생성하면 ASC 가 자동 등록)
	HealthSet = CreateDefaultSubobject<UPTHealthSet>(TEXT("HealthSet"));
	StaminaSet = CreateDefaultSubobject<UPTStaminaSet>(TEXT("StaminaSet"));

	// ASC 가 붙은 PlayerState 는 빠른 갱신 필요
	SetNetUpdateFrequency(100.f);
}

UAbilitySystemComponent* APTPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void APTPlayerState::GrantAbilitiesFromPawn(APTCharacter* Pawn)
{
	if (!HasAuthority() || bAbilitiesGranted || !Pawn || !AbilitySystemComponent)
	{
		return;
	}
	bAbilitiesGranted = true;

	for (const UPTAbilitySet* Set : Pawn->GetAbilitySets())
	{
		if (Set)
		{
			Set->GiveToAbilitySystem(AbilitySystemComponent, &GrantedHandles, this);
		}
	}

	for (const FPTAbilitySet_GameplayAbility& Entry : Pawn->GetDefaultAbilities())
	{
		UPTAbilitySet::GrantAbility(AbilitySystemComponent, Entry, this, &GrantedHandles);
	}
}

void APTPlayerState::ResetForRespawn()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(PTGameplayTags::Status_Dead))
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(PTGameplayTags::Status_Dead, 0, EGameplayTagReplicationState::TagOnly);

		if (HealthSet)
		{
			AbilitySystemComponent->SetNumericAttributeBase(UPTHealthSet::GetHealthAttribute(), HealthSet->GetMaxHealth());
		}
		if (StaminaSet)
		{
			AbilitySystemComponent->SetNumericAttributeBase(UPTStaminaSet::GetStaminaAttribute(), StaminaSet->GetMaxStamina());
		}
	}
}

void APTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, PTPlayerId);
	DOREPLIFETIME(ThisClass, JoinOrder);
}

void APTPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (APTPlayerState* Other = Cast<APTPlayerState>(PlayerState))
	{
		Other->PTPlayerId = PTPlayerId;
		Other->JoinOrder = JoinOrder;
	}
}

void APTPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);

	if (const APTPlayerState* Other = Cast<APTPlayerState>(PlayerState))
	{
		PTPlayerId = Other->PTPlayerId;
		JoinOrder = Other->JoinOrder;
	}
}
