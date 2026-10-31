// Copyright NeedsGames. All Rights Reserved.

#include "PTGameplayTags.h"

namespace PTGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move,   "InputTag.Move",   "Move input (native).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look,   "InputTag.Look",   "Look input (native).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Jump,   "InputTag.Jump",   "Jump input (ability).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Sprint, "InputTag.Sprint", "Sprint input (ability).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Crouch, "InputTag.Crouch", "Crouch toggle (native).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Walk,   "InputTag.Walk",   "Walk toggle (native).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Strafe, "InputTag.Strafe", "Strafe / orient-to-movement toggle (native).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Jump,   "Ability.Movement.Jump",   "Jump ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Sprint, "Ability.Movement.Sprint", "Sprint ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Traversal, "Ability.Movement.Traversal", "Parkour traversal (vault / hurdle / mantle).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Block_Movement,  "Ability.Block.Movement",  "Blocks movement abilities.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_IsDead,   "Ability.ActivateFail.IsDead",   "Activation failed: owner is dead.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cooldown, "Ability.ActivateFail.Cooldown", "Activation failed: on cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cost,     "Ability.ActivateFail.Cost",     "Activation failed: cannot pay cost.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Dead,      "Status.Dead",      "Owner is dead.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Sprinting, "Status.Sprinting", "Owner is sprinting.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Crouching, "Status.Crouching", "Owner is crouching.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_InAir,     "Status.InAir",     "Owner is falling / jumping.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Traversing, "Status.Traversing", "Owner is performing a traversal action.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage,  "SetByCaller.Damage",  "SetByCaller magnitude: damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Heal,    "SetByCaller.Heal",    "SetByCaller magnitude: heal.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Stamina, "SetByCaller.Stamina", "SetByCaller magnitude: stamina delta.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death,              "Event.Death",              "Sent when health reaches zero.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Migration_Restored, "Event.Migration.Restored", "Sent to avatar after host-migration state restore.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Traversal, "Event.Traversal", "Triggers the traversal ability (payload: FPTGameplayAbilityTargetData_Traversal).");
}
