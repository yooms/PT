// Copyright NeedsGames. All Rights Reserved.

#include "AnimGraphNode_PTAimOffset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_PTAimOffset)

#define LOCTEXT_NAMESPACE "PTAimOffset"

FText UAnimGraphNode_PTAimOffset::GetControllerDescription() const
{
	return LOCTEXT("ControllerDescription", "PT Aim Offset (Procedural)");
}

FText UAnimGraphNode_PTAimOffset::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_PTAimOffset::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Distributes aim yaw/pitch across a spine-to-head bone chain without an Aim Offset asset. Bind Aim Yaw / Aim Pitch / Alpha to the PTMotionMatchingAnimInstance variables.");
}

FLinearColor UAnimGraphNode_PTAimOffset::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.5f, 0.9f);
}

#undef LOCTEXT_NAMESPACE
