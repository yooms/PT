// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "AbilitySystem/PTGameplayAbility.h"
#include "PTGameplayAbility_Sprint.generated.h"

/**
 * 스프린트 어빌리티 (누르고 있는 동안 유지).
 * 실제 속도 변경은 UPTCharacterMovementComponent 의 SavedMove 플래그로 예측/복제됩니다.
 * 스태미나가 있다면 StaminaCostPerSecond 만큼 서버에서 소모합니다.
 */
UCLASS()
class PT_API UPTGameplayAbility_Sprint : public UPTGameplayAbility
{
	GENERATED_BODY()

public:
	UPTGameplayAbility_Sprint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	void TickStaminaCost();

	/** 초당 스태미나 소모량 (0 이면 소모 없음). PTStaminaSet 이 있을 때만 적용 */
	UPROPERTY(EditDefaultsOnly, Category = "PT|Sprint", meta = (ClampMin = "0"))
	float StaminaCostPerSecond = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "PT|Sprint", meta = (ClampMin = "0.05"))
	float StaminaTickInterval = 0.25f;

	/** 스프린트를 (다시) 시작하는 데 필요한 최소 스태미나 - 0 근처에서 활성/취소가 반복되는 것을 방지 */
	UPROPERTY(EditDefaultsOnly, Category = "PT|Sprint", meta = (ClampMin = "0"))
	float MinStaminaToStart = 10.f;

	FTimerHandle StaminaTimerHandle;
};
