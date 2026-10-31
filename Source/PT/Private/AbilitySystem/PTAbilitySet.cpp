// Copyright NeedsGames. All Rights Reserved.

#include "AbilitySystem/PTAbilitySet.h"
#include "AbilitySystem/PTAbilitySystemComponent.h"
#include "AbilitySystem/PTGameplayAbility.h"
#include "GameplayEffect.h"
#include "PTLogChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTAbilitySet)

void FPTAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FPTAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FPTAbilitySet_GrantedHandles::AddAttributeSet(UAttributeSet* Set)
{
	GrantedAttributeSets.Add(Set);
}

void FPTAbilitySet_GrantedHandles::TakeFromAbilitySystem(UPTAbilitySystemComponent* ASC)
{
	check(ASC);
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	for (UAttributeSet* Set : GrantedAttributeSets)
	{
		if (Set)
		{
			ASC->RemoveSpawnedAttribute(Set);
		}
	}

	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
	GrantedAttributeSets.Reset();
}

FGameplayAbilitySpecHandle UPTAbilitySet::GrantAbility(UPTAbilitySystemComponent* ASC, const FPTAbilitySet_GameplayAbility& Entry, UObject* SourceObject, FPTAbilitySet_GrantedHandles* OutGrantedHandles)
{
	if (!ASC || !IsValid(Entry.Ability))
	{
		return FGameplayAbilitySpecHandle();
	}

	UPTGameplayAbility* AbilityCDO = Entry.Ability->GetDefaultObject<UPTGameplayAbility>();

	FGameplayAbilitySpec AbilitySpec(AbilityCDO, Entry.AbilityLevel);
	AbilitySpec.SourceObject = SourceObject;
	if (Entry.InputTag.IsValid())
	{
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
	}

	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(AbilitySpec);
	if (OutGrantedHandles)
	{
		OutGrantedHandles->AddAbilitySpecHandle(Handle);
	}
	return Handle;
}

void UPTAbilitySet::GiveToAbilitySystem(UPTAbilitySystemComponent* ASC, FPTAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	check(ASC);
	if (!ASC->IsOwnerActorAuthoritative())
	{
		// 부여는 서버에서만
		return;
	}

	// 1) Attribute Sets (어빌리티/이펙트가 참조할 수 있으므로 먼저)
	for (int32 Index = 0; Index < GrantedAttributes.Num(); ++Index)
	{
		const FPTAbilitySet_AttributeSet& SetToGrant = GrantedAttributes[Index];
		if (!IsValid(SetToGrant.AttributeSet))
		{
			UE_LOG(LogPTAbility, Error, TEXT("GrantedAttributes[%d] on ability set [%s] is not valid"), Index, *GetNameSafe(this));
			continue;
		}

		UAttributeSet* NewSet = NewObject<UAttributeSet>(ASC->GetOwner(), SetToGrant.AttributeSet);
		ASC->AddAttributeSetSubobject(NewSet);
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAttributeSet(NewSet);
		}
	}

	// 2) Abilities
	for (int32 Index = 0; Index < GrantedGameplayAbilities.Num(); ++Index)
	{
		if (!IsValid(GrantedGameplayAbilities[Index].Ability))
		{
			UE_LOG(LogPTAbility, Error, TEXT("GrantedGameplayAbilities[%d] on ability set [%s] is not valid."), Index, *GetNameSafe(this));
			continue;
		}
		GrantAbility(ASC, GrantedGameplayAbilities[Index], SourceObject, OutGrantedHandles);
	}

	// 3) Effects
	for (int32 Index = 0; Index < GrantedGameplayEffects.Num(); ++Index)
	{
		const FPTAbilitySet_GameplayEffect& EffectToGrant = GrantedGameplayEffects[Index];
		if (!IsValid(EffectToGrant.GameplayEffect))
		{
			UE_LOG(LogPTAbility, Error, TEXT("GrantedGameplayEffects[%d] on ability set [%s] is not valid"), Index, *GetNameSafe(this));
			continue;
		}

		const UGameplayEffect* GameplayEffect = EffectToGrant.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(GameplayEffect, EffectToGrant.EffectLevel, ASC->MakeEffectContext());
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddGameplayEffectHandle(Handle);
		}
	}
}
