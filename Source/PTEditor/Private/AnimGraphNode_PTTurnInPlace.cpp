// Copyright NeedsGames. All Rights Reserved.

#include "AnimGraphNode_PTTurnInPlace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_PTTurnInPlace)

#define LOCTEXT_NAMESPACE "PTTurnInPlace"

FText UAnimGraphNode_PTTurnInPlace::GetControllerDescription() const
{
	return LOCTEXT("ControllerDescription", "PT Turn In Place (Procedural)");
}

FText UAnimGraphNode_PTTurnInPlace::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_PTTurnInPlace::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Keeps the mesh in place while the capsule turns and catches up with procedural foot steps (two-bone IK). Bind the pins to the Turn* variables of PTMotionMatchingAnimInstance.");
}

FLinearColor UAnimGraphNode_PTTurnInPlace::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.75f, 0.45f);
}

#undef LOCTEXT_NAMESPACE
