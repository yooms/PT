// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_PTTurnInPlace.h"
#include "AnimGraphNode_PTTurnInPlace.generated.h"

/** AnimBP 그래프용 "PT Turn In Place" 노드 */
UCLASS(meta = (Keywords = "Turn In Place Step Foot Procedural PT"))
class PTEDITOR_API UAnimGraphNode_PTTurnInPlace : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_PTTurnInPlace Node;

	// UEdGraphNode
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

protected:
	// UAnimGraphNode_SkeletalControlBase
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
