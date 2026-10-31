// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_PTAimOffset.h"
#include "AnimGraphNode_PTAimOffset.generated.h"

/** AnimBP 그래프용 "PT Aim Offset" 노드 */
UCLASS(meta = (Keywords = "Aim Offset Look Spine Procedural PT"))
class PTEDITOR_API UAnimGraphNode_PTAimOffset : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_PTAimOffset Node;

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

protected:
	// UAnimGraphNode_SkeletalControlBase
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
