// Copyright yoomsGames. All Rights Reserved.

#include "AbilitySystem/Attributes/PTAttributeSet.h"
#include "AbilitySystem/PTAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTAttributeSet)

UPTAttributeSet::UPTAttributeSet()
{
}

UPTAbilitySystemComponent* UPTAttributeSet::GetPTAbilitySystemComponent() const
{
	return Cast<UPTAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}
