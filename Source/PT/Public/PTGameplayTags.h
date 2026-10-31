// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/**
 * 프로젝트 전역 네이티브 게임플레이 태그.
 * 새 태그는 여기(.h)에 선언하고 PTGameplayTags.cpp 에 정의하세요.
 */
namespace PTGameplayTags
{
	// ---- Input ----
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Jump);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Sprint);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Crouch);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Walk);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Strafe);

	// ---- Ability ----
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Jump);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Sprint);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Traversal);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Block_Movement);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_IsDead);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cooldown);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cost);

	// ---- Status ----
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Dead);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Sprinting);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Crouching);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_InAir);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Traversing);

	// ---- SetByCaller ----
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Heal);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Stamina);

	// ---- Gameplay Event ----
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Death);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Migration_Restored);
	PT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Traversal);
}
