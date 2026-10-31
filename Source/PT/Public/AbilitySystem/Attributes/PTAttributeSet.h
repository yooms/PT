// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "PTAttributeSet.generated.h"

class UPTAbilitySystemComponent;

/** Getter/Setter/Initter 를 생성하는 매크로 */
#define PT_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) ATTRIBUTE_ACCESSORS_BASIC(ClassName, PropertyName)

/** 프로젝트 기본 AttributeSet */
UCLASS(Abstract)
class PT_API UPTAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPTAttributeSet();

	UPTAbilitySystemComponent* GetPTAbilitySystemComponent() const;
};
