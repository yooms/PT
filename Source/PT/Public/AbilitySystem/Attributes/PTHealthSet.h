// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "AbilitySystem/Attributes/PTAttributeSet.h"
#include "PTHealthSet.generated.h"

DECLARE_MULTICAST_DELEGATE_FourParams(FPTAttributeEvent, AActor* /*Instigator*/, AActor* /*Causer*/, float /*OldValue*/, float /*NewValue*/);

/**
 * 체력 어트리뷰트.
 *  - Health / MaxHealth : 복제
 *  - Damage / Healing   : 메타 어트리뷰트 (서버 전용, GE Execution 에서 사용)
 */
UCLASS(BlueprintType)
class PT_API UPTHealthSet : public UPTAttributeSet
{
	GENERATED_BODY()

public:
	UPTHealthSet();

	PT_ATTRIBUTE_ACCESSORS(UPTHealthSet, Health);
	PT_ATTRIBUTE_ACCESSORS(UPTHealthSet, MaxHealth);
	PT_ATTRIBUTE_ACCESSORS(UPTHealthSet, Damage);
	PT_ATTRIBUTE_ACCESSORS(UPTHealthSet, Healing);

	/** 체력이 0 이 되었을 때 (서버) */
	mutable FPTAttributeEvent OnOutOfHealth;
	mutable FPTAttributeEvent OnHealthChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "PT|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "PT|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Damage;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Healing;

	bool bOutOfHealth = false;
	float HealthBeforeAttributeChange = 0.f;
};
