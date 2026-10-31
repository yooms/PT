// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "EnhancedInputComponent.h"
#include "Input/PTInputConfig.h"
#include "PTInputComponent.generated.h"

/** 태그 기반 바인딩을 지원하는 EnhancedInputComponent (DefaultInput.ini 의 DefaultInputComponentClass) */
UCLASS(Config = Input)
class PT_API UPTInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	template<class UserClass, typename FuncType>
	void BindNativeAction(const UPTInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bLogIfNotFound)
	{
		check(InputConfig);
		if (const UInputAction* IA = InputConfig->FindNativeInputActionForTag(InputTag, bLogIfNotFound))
		{
			BindAction(IA, TriggerEvent, Object, Func);
		}
	}

	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const UPTInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>& BindHandles)
	{
		check(InputConfig);
		for (const FPTInputAction& Action : InputConfig->AbilityInputActions)
		{
			if (Action.InputAction && Action.InputTag.IsValid())
			{
				if (PressedFunc)
				{
					BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Started, Object, PressedFunc, Action.InputTag).GetHandle());
				}
				if (ReleasedFunc)
				{
					BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag).GetHandle());
				}
			}
		}
	}

	void RemoveBinds(TArray<uint32>& BindHandles)
	{
		for (uint32 Handle : BindHandles)
		{
			RemoveBindingByHandle(Handle);
		}
		BindHandles.Reset();
	}
};
