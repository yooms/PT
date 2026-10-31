// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbility.h"
#include "PTGameplayAbility.generated.h"

class APTCharacter;
class APTPlayerController;
class UPTAbilitySystemComponent;
class UPTCharacterMovementComponent;

UENUM(BlueprintType)
enum class EPTAbilityActivationPolicy : uint8
{
	/** 입력이 눌린 순간 1회 활성화 */
	OnInputTriggered,
	/** 입력을 누르고 있는 동안 지속적으로 활성화 시도 */
	WhileInputActive,
	/** 아바타가 설정되면(스폰) 자동 활성화 - 패시브 */
	OnSpawn
};

/**
 * 프로젝트 기본 어빌리티.
 * 기본값: InstancedPerActor / LocalPredicted
 */
UCLASS(Abstract, HideCategories = Input)
class PT_API UPTGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPTGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	EPTAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	UFUNCTION(BlueprintPure, Category = "PT|Ability")
	UPTAbilitySystemComponent* GetPTAbilitySystemComponentFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "PT|Ability")
	APTCharacter* GetPTCharacterFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "PT|Ability")
	APTPlayerController* GetPTPlayerControllerFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "PT|Ability")
	UPTCharacterMovementComponent* GetPTMovementComponentFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "PT|Ability")
	bool IsLocallyControlledAvatar() const;

	void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const;

protected:
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/** 어빌리티 활성화 정책 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Ability Activation")
	EPTAbilityActivationPolicy ActivationPolicy = EPTAbilityActivationPolicy::OnInputTriggered;
};
