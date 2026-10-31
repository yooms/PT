// Copyright yoomsGames. All Rights Reserved.

#include "Input/PTInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"
#include "PTGameplayTags.h"
#include "PTLogChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTInputConfig)

const UInputAction* UPTInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FPTInputAction& Action : NativeInputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogPT, Error, TEXT("Can't find NativeInputAction for InputTag [%s] on InputConfig [%s]."), *InputTag.ToString(), *GetNameSafe(this));
	}
	return nullptr;
}

const UInputAction* UPTInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FPTInputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogPT, Error, TEXT("Can't find AbilityInputAction for InputTag [%s] on InputConfig [%s]."), *InputTag.ToString(), *GetNameSafe(this));
	}
	return nullptr;
}

namespace PTInputConfigPrivate
{
	static UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name, RF_Transient);
		Action->ValueType = ValueType;
		return Action;
	}

	template<typename TModifier>
	static TModifier* AddModifier(UInputMappingContext* IMC, FEnhancedActionKeyMapping& Mapping)
	{
		TModifier* Modifier = NewObject<TModifier>(IMC, NAME_None, RF_Transient);
		Mapping.Modifiers.Add(Modifier);
		return Modifier;
	}
}

UPTInputConfig* UPTInputConfig::CreateDefaultRuntimeConfig(UObject* Outer, UInputMappingContext*& OutMappingContext)
{
	using namespace PTInputConfigPrivate;

	UPTInputConfig* Config = NewObject<UPTInputConfig>(Outer, TEXT("PTDefaultInputConfig"), RF_Transient);
	UInputMappingContext* IMC = NewObject<UInputMappingContext>(Outer, TEXT("PTDefaultMappingContext"), RF_Transient);

	UInputAction* IA_Move   = MakeAction(Config, TEXT("IA_PT_Move"),   EInputActionValueType::Axis2D);
	UInputAction* IA_Look   = MakeAction(Config, TEXT("IA_PT_Look"),   EInputActionValueType::Axis2D);
	UInputAction* IA_Jump   = MakeAction(Config, TEXT("IA_PT_Jump"),   EInputActionValueType::Boolean);
	UInputAction* IA_Sprint = MakeAction(Config, TEXT("IA_PT_Sprint"), EInputActionValueType::Boolean);
	UInputAction* IA_Crouch = MakeAction(Config, TEXT("IA_PT_Crouch"), EInputActionValueType::Boolean);
	UInputAction* IA_Walk   = MakeAction(Config, TEXT("IA_PT_Walk"),   EInputActionValueType::Boolean);
	UInputAction* IA_Strafe = MakeAction(Config, TEXT("IA_PT_Strafe"), EInputActionValueType::Boolean);

	// ---- Move (WASD) : X = Right, Y = Forward ----
	{
		FEnhancedActionKeyMapping& W = IMC->MapKey(IA_Move, EKeys::W);
		AddModifier<UInputModifierSwizzleAxis>(IMC, W)->Order = EInputAxisSwizzle::YXZ;

		FEnhancedActionKeyMapping& S = IMC->MapKey(IA_Move, EKeys::S);
		AddModifier<UInputModifierSwizzleAxis>(IMC, S)->Order = EInputAxisSwizzle::YXZ;
		AddModifier<UInputModifierNegate>(IMC, S);

		FEnhancedActionKeyMapping& A = IMC->MapKey(IA_Move, EKeys::A);
		AddModifier<UInputModifierNegate>(IMC, A);

		IMC->MapKey(IA_Move, EKeys::D);

		FEnhancedActionKeyMapping& Stick = IMC->MapKey(IA_Move, EKeys::Gamepad_Left2D);
		AddModifier<UInputModifierDeadZone>(IMC, Stick);
	}

	// ---- Look ----
	{
		IMC->MapKey(IA_Look, EKeys::Mouse2D);
		FEnhancedActionKeyMapping& Stick = IMC->MapKey(IA_Look, EKeys::Gamepad_Right2D);
		AddModifier<UInputModifierDeadZone>(IMC, Stick);
	}

	IMC->MapKey(IA_Jump, EKeys::SpaceBar);
	IMC->MapKey(IA_Jump, EKeys::Gamepad_FaceButton_Bottom);

	IMC->MapKey(IA_Sprint, EKeys::LeftShift);
	IMC->MapKey(IA_Sprint, EKeys::Gamepad_LeftThumbstick);

	IMC->MapKey(IA_Crouch, EKeys::C);
	IMC->MapKey(IA_Crouch, EKeys::Gamepad_FaceButton_Right);

	IMC->MapKey(IA_Walk, EKeys::LeftControl);
	IMC->MapKey(IA_Strafe, EKeys::Tab);

	Config->NativeInputActions.Add({ IA_Move,   PTGameplayTags::InputTag_Move });
	Config->NativeInputActions.Add({ IA_Look,   PTGameplayTags::InputTag_Look });
	Config->NativeInputActions.Add({ IA_Crouch, PTGameplayTags::InputTag_Crouch });
	Config->NativeInputActions.Add({ IA_Walk,   PTGameplayTags::InputTag_Walk });
	Config->NativeInputActions.Add({ IA_Strafe, PTGameplayTags::InputTag_Strafe });

	Config->AbilityInputActions.Add({ IA_Jump,   PTGameplayTags::InputTag_Jump });
	Config->AbilityInputActions.Add({ IA_Sprint, PTGameplayTags::InputTag_Sprint });

	OutMappingContext = IMC;
	return Config;
}
