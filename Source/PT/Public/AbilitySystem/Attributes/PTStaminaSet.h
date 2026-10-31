// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "AbilitySystem/Attributes/PTAttributeSet.h"
#include "PTStaminaSet.generated.h"

/** 스태미나 어트리뷰트 (스프린트 등 이동 어빌리티 비용으로 사용) */
UCLASS(BlueprintType)
class PT_API UPTStaminaSet : public UPTAttributeSet
{
	GENERATED_BODY()

public:
	UPTStaminaSet();

	PT_ATTRIBUTE_ACCESSORS(UPTStaminaSet, Stamina);
	PT_ATTRIBUTE_ACCESSORS(UPTStaminaSet, MaxStamina);
	PT_ATTRIBUTE_ACCESSORS(UPTStaminaSet, StaminaRegenRate);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_StaminaRegenRate(const FGameplayAttributeData& OldValue);

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "PT|Stamina", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Stamina;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "PT|Stamina", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxStamina;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaRegenRate, Category = "PT|Stamina", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData StaminaRegenRate;
};
