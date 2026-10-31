// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_PTFootIK.h"
#include "AnimGraphNode_PTFootIK.generated.h"

/** AnimBP 그래프용 "PT Foot IK" 노드 (경사면/계단 발 맞춤) */
UCLASS(meta = (Keywords = "Foot IK Slope Ground Leg PT"))
class PTEDITOR_API UAnimGraphNode_PTFootIK : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_PTFootIK Node;

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

protected:
	// UAnimGraphNode_SkeletalControlBase
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
