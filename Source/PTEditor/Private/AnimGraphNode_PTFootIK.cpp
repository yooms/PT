// Copyright yoomsGames. All Rights Reserved.

#include "AnimGraphNode_PTFootIK.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_PTFootIK)

#define LOCTEXT_NAMESPACE "PTFootIK"

FText UAnimGraphNode_PTFootIK::GetControllerDescription() const
{
	return LOCTEXT("ControllerDescription", "PT Foot IK (Slope)");
}

FText UAnimGraphNode_PTFootIK::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_PTFootIK::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Lowers the pelvis and places each foot on the ground under it (slopes, stairs) with two-bone IK, aligning the sole to the ground normal. Bind the pins to the FootIK* variables of PTMotionMatchingAnimInstance.");
}

FLinearColor UAnimGraphNode_PTFootIK::GetNodeTitleColor() const
{
	return FLinearColor(0.3f, 0.55f, 0.85f);
}

#undef LOCTEXT_NAMESPACE
