// Copyright NeedsGames. All Rights Reserved.

#include "AbilitySystem/Attributes/PTHealthSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "PTGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTHealthSet)

UPTHealthSet::UPTHealthSet()
	: Health(100.f)
	, MaxHealth(100.f)
{
}

void UPTHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UPTHealthSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPTHealthSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UPTHealthSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPTHealthSet, Health, OldValue);
	OnHealthChanged.Broadcast(nullptr, nullptr, OldValue.GetCurrentValue(), GetHealth());
}

void UPTHealthSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPTHealthSet, MaxHealth, OldValue);
}

bool UPTHealthSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}
	HealthBeforeAttributeChange = GetHealth();
	return true;
}

void UPTHealthSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
	AActor* Instigator = Context.GetOriginalInstigator();
	AActor* Causer = Context.GetEffectCauser();

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.f);
		if (LocalDamage > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() - LocalDamage, 0.f, GetMaxHealth()));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		const float LocalHealing = GetHealing();
		SetHealing(0.f);
		if (LocalHealing > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() + LocalHealing, 0.f, GetMaxHealth()));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}

	if (GetHealth() != HealthBeforeAttributeChange)
	{
		OnHealthChanged.Broadcast(Instigator, Causer, HealthBeforeAttributeChange, GetHealth());
	}

	if (GetHealth() <= 0.f && !bOutOfHealth)
	{
		OnOutOfHealth.Broadcast(Instigator, Causer, HealthBeforeAttributeChange, GetHealth());

		// 사망 이벤트 (어빌리티가 Event.Death 트리거로 반응 가능)
		if (AActor* Avatar = GetOwningAbilitySystemComponent() ? GetOwningAbilitySystemComponent()->GetAvatarActor() : nullptr)
		{
			FGameplayEventData Payload;
			Payload.EventTag = PTGameplayTags::Event_Death;
			Payload.Instigator = Instigator;
			Payload.Target = Avatar;
			Payload.EventMagnitude = HealthBeforeAttributeChange - GetHealth();
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, PTGameplayTags::Event_Death, Payload);
		}
	}

	bOutOfHealth = (GetHealth() <= 0.f);
}

void UPTHealthSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UPTHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UPTHealthSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		// MaxHealth 가 줄어들면 Health 도 맞춰서 줄임
		if (GetHealth() > NewValue)
		{
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				ASC->ApplyModToAttribute(GetHealthAttribute(), EGameplayModOp::Override, NewValue);
			}
		}
	}

	if (bOutOfHealth && GetHealth() > 0.f)
	{
		bOutOfHealth = false;
	}
}

void UPTHealthSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}
