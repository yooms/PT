// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "PTInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

USTRUCT(BlueprintType)
struct FPTInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * InputAction <-> GameplayTag 매핑.
 *  - NativeInputActions : C++ 함수에 직접 바인딩 (Move/Look/Crouch/Walk/Strafe)
 *  - AbilityInputActions: 태그로 ASC 어빌리티 활성화 (Jump/Sprint/...)
 *
 * 에셋을 지정하지 않으면 APTCharacter 가 CreateDefaultRuntimeConfig() 로 기본 키 배치를 런타임 생성합니다.
 */
UCLASS(BlueprintType, Const)
class PT_API UPTInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PT|Input")
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = false) const;

	UFUNCTION(BlueprintCallable, Category = "PT|Input")
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = false) const;

	/**
	 * 에셋 없이 동작하는 기본 입력 구성 생성.
	 * WASD 이동 / 마우스 시점 / Space 점프 / LShift 스프린트 / C 앉기 / LCtrl 걷기 토글 / Tab 스트레이프 토글
	 * 게임패드: 좌스틱 이동 / 우스틱 시점 / A 점프 / 좌스틱 클릭 스프린트 / B 앉기
	 */
	static UPTInputConfig* CreateDefaultRuntimeConfig(UObject* Outer, UInputMappingContext*& OutMappingContext);

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (TitleProperty = "InputAction"))
	TArray<FPTInputAction> NativeInputActions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (TitleProperty = "InputAction"))
	TArray<FPTInputAction> AbilityInputActions;
};
