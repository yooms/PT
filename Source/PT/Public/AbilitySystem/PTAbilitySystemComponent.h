// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "PTAbilitySystemComponent.generated.h"

/**
 * 프로젝트 기본 ASC.
 *  - PlayerState 가 소유 (Owner = PlayerState, Avatar = Pawn) -> 리스폰/호스트 인계 시에도 상태가 PlayerState 단위로 관리됨
 *  - 입력 태그 기반 어빌리티 활성화 (InputTag.* <-> AbilitySpec DynamicSpecSourceTags)
 */
UCLASS(ClassGroup = AbilitySystem, meta = (BlueprintSpawnableComponent))
class PT_API UPTAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UPTAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	/** 입력 태그가 눌렸을 때 (로컬 클라이언트) */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	/** 입력 태그가 떼졌을 때 (로컬 클라이언트) */
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/** PlayerController::PostProcessInput 에서 매 프레임 호출 */
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	void ClearAbilityInput();

	/** ActivationPolicy == OnSpawn 인 어빌리티 활성화 */
	void TryActivateAbilitiesOnSpawn();

	/** 태그를 가진 활성 어빌리티 취소 */
	UFUNCTION(BlueprintCallable, Category = "PT|Ability")
	void CancelAbilitiesWithTag(const FGameplayTagContainer& Tags);

protected:
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;
};
