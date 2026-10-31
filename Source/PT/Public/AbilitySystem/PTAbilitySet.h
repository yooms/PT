// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "PTAbilitySet.generated.h"

class UAttributeSet;
class UGameplayEffect;
class UPTAbilitySystemComponent;
class UPTGameplayAbility;

USTRUCT(BlueprintType)
struct FPTAbilitySet_GameplayAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UPTGameplayAbility> Ability = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 AbilityLevel = 1;

	/** 이 어빌리티를 활성화할 입력 태그 (InputTag.*) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

USTRUCT(BlueprintType)
struct FPTAbilitySet_GameplayEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float EffectLevel = 1.0f;
};

USTRUCT(BlueprintType)
struct FPTAbilitySet_AttributeSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UAttributeSet> AttributeSet;
};

/** 부여한 항목의 핸들 (회수용) */
USTRUCT(BlueprintType)
struct FPTAbilitySet_GrantedHandles
{
	GENERATED_BODY()

public:
	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void AddAttributeSet(UAttributeSet* Set);

	/** 부여했던 어빌리티/이펙트/어트리뷰트셋 회수 (서버) */
	void TakeFromAbilitySystem(UPTAbilitySystemComponent* ASC);

protected:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	UPROPERTY()
	TArray<TObjectPtr<UAttributeSet>> GrantedAttributeSets;
};

/**
 * 어빌리티/이펙트/어트리뷰트셋 묶음 데이터 에셋.
 * 캐릭터 BP 의 AbilitySets 에 지정하면 서버에서 PlayerState 의 ASC 로 부여됩니다.
 */
UCLASS(BlueprintType, Const)
class PT_API UPTAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 서버 전용 */
	void GiveToAbilitySystem(UPTAbilitySystemComponent* ASC, FPTAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;

	static FGameplayAbilitySpecHandle GrantAbility(UPTAbilitySystemComponent* ASC, const FPTAbilitySet_GameplayAbility& Entry, UObject* SourceObject, FPTAbilitySet_GrantedHandles* OutGrantedHandles);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Abilities", meta = (TitleProperty = Ability))
	TArray<FPTAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects", meta = (TitleProperty = GameplayEffect))
	TArray<FPTAbilitySet_GameplayEffect> GrantedGameplayEffects;

	UPROPERTY(EditDefaultsOnly, Category = "Attribute Sets", meta = (TitleProperty = AttributeSet))
	TArray<FPTAbilitySet_AttributeSet> GrantedAttributes;
};
