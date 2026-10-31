// Copyright yoomsGames. All Rights Reserved.

/**
 * 에디터 콘솔 명령:  pt.Editor.SetupLocomotionABP [원본 ABP 경로] [대상 ABP 경로]
 *                    (pt.Editor.SetupAimOffsetABP 는 같은 명령의 이전 이름)
 *
 *  대상 ABP 가 없으면: 원본을 복제하고 부모 클래스를 UPTMotionMatchingAnimInstance 로 변경
 *  대상 ABP 가 있으면: 그 ABP 를 그대로 수정 (이미 있는 노드는 다시 만들지 않음)
 *
 *  AnimGraph 의 Output Pose 직전에 다음 체인을 보장하고 변수 핀을 연결한 뒤 컴파일/저장:
 *    Local To Component → PT Turn In Place → PT Aim Offset → Component To Local
 *
 * 기본값: /Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed  →  /Game/PT/Animation/ABP_PTUnarmed
 */

#include "AnimGraphNode_PTAimOffset.h"
#include "AnimGraphNode_PTTurnInPlace.h"
#include "Animation/PTMotionMatchingAnimInstance.h"

#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_Root.h"
#include "Animation/AnimBlueprint.h"
#include "AssetToolsModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPTEditor, Log, All);

namespace PTAnimBPSetup
{
	static bool IsPosePin(const UEdGraphPin* Pin, bool bComponentSpace)
	{
		if (!Pin || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
		{
			return false;
		}
		// FPoseLink / FComponentSpacePoseLink 는 DLL export 되지 않으므로 이름으로 비교
		const UObject* Struct = Pin->PinType.PinSubCategoryObject.Get();
		return Struct && Struct->GetFName() == (bComponentSpace ? FName(TEXT("ComponentSpacePoseLink")) : FName(TEXT("PoseLink")));
	}

	static UEdGraphPin* FindPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction, bool bComponentSpace)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == Direction && IsPosePin(Pin, bComponentSpace))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	template<typename T>
	static T* SpawnNode(UEdGraph* Graph, int32 X, int32 Y)
	{
		FGraphNodeCreator<T> Creator(*Graph);
		T* NewNode = Creator.CreateNode(false);
		NewNode->NodePosX = X;
		NewNode->NodePosY = Y;
		Creator.Finalize();
		return NewNode;
	}

	template<typename T>
	static T* FindFirstNode(UEdGraph* Graph)
	{
		TArray<T*> Found;
		Graph->GetNodesOfClass(Found);
		return Found.Num() > 0 ? Found[0] : nullptr;
	}

	/** 핀이 비어 있으면 변수 Get 노드를 만들어 연결 */
	static bool BindVariable(UEdGraph* Graph, UEdGraphNode* TargetNode, FName PinName, FName VariableName, int32 X, int32 Y)
	{
		UEdGraphPin* TargetPin = TargetNode->FindPin(PinName, EGPD_Input);
		if (!TargetPin)
		{
			// 노드 구조체에 새로 추가된 핀(예: LeanPitch)은 노드를 재구성해야 생김
			TargetNode->ReconstructNode();
			TargetPin = TargetNode->FindPin(PinName, EGPD_Input);
		}
		if (!TargetPin)
		{
			UE_LOG(LogPTEditor, Warning, TEXT("Pin '%s' not found on %s"), *PinName.ToString(), *TargetNode->GetName());
			return false;
		}
		if (TargetPin->LinkedTo.Num() > 0)
		{
			return true; // 이미 연결됨
		}

		FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
		UK2Node_VariableGet* VarNode = Creator.CreateNode(false);
		VarNode->VariableReference.SetSelfMember(VariableName);
		VarNode->NodePosX = X;
		VarNode->NodePosY = Y;
		Creator.Finalize();

		UEdGraphPin* ValuePin = VarNode->GetValuePin();
		if (!ValuePin || !Graph->GetSchema()->TryCreateConnection(ValuePin, TargetPin))
		{
			UE_LOG(LogPTEditor, Warning, TEXT("Could not connect variable '%s' to pin '%s'"), *VariableName.ToString(), *PinName.ToString());
			return false;
		}
		return true;
	}

	/** "/Game/A/B" -> "/Game/A/B.B" */
	static FString ToObjectPath(const FString& Path)
	{
		return Path.Contains(TEXT(".")) ? Path : FString::Printf(TEXT("%s.%s"), *Path, *FPackageName::GetShortName(Path));
	}

	static FString ToPackagePath(const FString& Path)
	{
		FString Package = Path;
		int32 DotIndex = INDEX_NONE;
		if (Package.FindChar(TEXT('.'), DotIndex))
		{
			Package.LeftInline(DotIndex);
		}
		return Package;
	}

	static UAnimBlueprint* GetOrCreateTarget(const FString& SourcePath, const FString& DestPackage)
	{
		if (FPackageName::DoesPackageExist(DestPackage))
		{
			UAnimBlueprint* Existing = LoadObject<UAnimBlueprint>(nullptr, *ToObjectPath(DestPackage));
			if (Existing)
			{
				UE_LOG(LogPTEditor, Display, TEXT("Updating existing %s"), *DestPackage);
			}
			return Existing;
		}

		UAnimBlueprint* Source = LoadObject<UAnimBlueprint>(nullptr, *ToObjectPath(SourcePath));
		if (!Source)
		{
			UE_LOG(LogPTEditor, Error, TEXT("Source AnimBlueprint not found: %s"), *SourcePath);
			return nullptr;
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		return Cast<UAnimBlueprint>(AssetTools.DuplicateAsset(FPackageName::GetShortName(DestPackage), FPackageName::GetLongPackagePath(DestPackage), Source));
	}

	static void Run(const TArray<FString>& Args)
	{
		const FString SourcePath = ToPackagePath(Args.Num() > 0 ? Args[0] : TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
		const FString DestPackage = ToPackagePath(Args.Num() > 1 ? Args[1] : TEXT("/Game/PT/Animation/ABP_PTUnarmed"));

		UAnimBlueprint* AnimBP = GetOrCreateTarget(SourcePath, DestPackage);
		if (!AnimBP)
		{
			UE_LOG(LogPTEditor, Error, TEXT("SetupLocomotionABP: could not get or create %s"), *DestPackage);
			return;
		}

		// 부모 클래스
		if (!AnimBP->ParentClass || !AnimBP->ParentClass->IsChildOf(UPTMotionMatchingAnimInstance::StaticClass()))
		{
			AnimBP->ParentClass = UPTMotionMatchingAnimInstance::StaticClass();
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
		}
		FBlueprintEditorUtils::RefreshAllNodes(AnimBP); // 새로 추가된 핀(ForwardYawOffset 등) 반영
		FKismetEditorUtilities::CompileBlueprint(AnimBP);

		UEdGraph* AnimGraph = nullptr;
		for (UEdGraph* Graph : AnimBP->FunctionGraphs)
		{
			if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
			{
				AnimGraph = Graph;
				break;
			}
		}
		if (!AnimGraph)
		{
			UE_LOG(LogPTEditor, Error, TEXT("SetupLocomotionABP: AnimGraph not found"));
			return;
		}

		const UEdGraphSchema* Schema = AnimGraph->GetSchema();
		UAnimGraphNode_PTAimOffset* AimNode = FindFirstNode<UAnimGraphNode_PTAimOffset>(AnimGraph);
		UAnimGraphNode_PTTurnInPlace* TurnNode = FindFirstNode<UAnimGraphNode_PTTurnInPlace>(AnimGraph);

		if (!AimNode)
		{
			// 전체 체인 삽입: (기존 포즈) → LTC → Turn → Aim → CTL → Output
			UAnimGraphNode_Root* Root = FindFirstNode<UAnimGraphNode_Root>(AnimGraph);
			UEdGraphPin* RootPosePin = FindPosePin(Root, EGPD_Input, false);
			if (!RootPosePin || RootPosePin->LinkedTo.Num() == 0)
			{
				UE_LOG(LogPTEditor, Error, TEXT("SetupLocomotionABP: Output Pose has no input pose"));
				return;
			}

			UEdGraphPin* PrevOutput = RootPosePin->LinkedTo[0];
			RootPosePin->BreakAllPinLinks();

			const int32 X = Root->NodePosX;
			const int32 Y = Root->NodePosY;
			Root->NodePosX = X + 1300;

			UAnimGraphNode_LocalToComponentSpace* ToComponent = SpawnNode<UAnimGraphNode_LocalToComponentSpace>(AnimGraph, X - 50, Y);
			TurnNode = SpawnNode<UAnimGraphNode_PTTurnInPlace>(AnimGraph, X + 250, Y);
			AimNode = SpawnNode<UAnimGraphNode_PTAimOffset>(AnimGraph, X + 650, Y);
			UAnimGraphNode_ComponentToLocalSpace* ToLocal = SpawnNode<UAnimGraphNode_ComponentToLocalSpace>(AnimGraph, X + 1050, Y);

			UEdGraphPin* Pins[] = {
				FindPosePin(ToComponent, EGPD_Input, false), FindPosePin(ToComponent, EGPD_Output, true),
				FindPosePin(TurnNode, EGPD_Input, true), FindPosePin(TurnNode, EGPD_Output, true),
				FindPosePin(AimNode, EGPD_Input, true), FindPosePin(AimNode, EGPD_Output, true),
				FindPosePin(ToLocal, EGPD_Input, true), FindPosePin(ToLocal, EGPD_Output, false) };
			for (UEdGraphPin* Pin : Pins)
			{
				if (!Pin)
				{
					UE_LOG(LogPTEditor, Error, TEXT("SetupLocomotionABP: pose pin not found"));
					return;
				}
			}

			const bool bOk = Schema->TryCreateConnection(PrevOutput, Pins[0])
				&& Schema->TryCreateConnection(Pins[1], Pins[2])
				&& Schema->TryCreateConnection(Pins[3], Pins[4])
				&& Schema->TryCreateConnection(Pins[5], Pins[6])
				&& Schema->TryCreateConnection(Pins[7], RootPosePin);
			if (!bOk)
			{
				UE_LOG(LogPTEditor, Error, TEXT("SetupLocomotionABP: failed to connect pose pins"));
				return;
			}
		}
		else if (!TurnNode)
		{
			// 기존 Aim Offset 앞에 Turn In Place 삽입
			UEdGraphPin* AimIn = FindPosePin(AimNode, EGPD_Input, true);
			if (!AimIn || AimIn->LinkedTo.Num() == 0)
			{
				UE_LOG(LogPTEditor, Error, TEXT("SetupLocomotionABP: PT Aim Offset has no input pose"));
				return;
			}
			UEdGraphPin* PrevOutput = AimIn->LinkedTo[0];
			AimIn->BreakAllPinLinks();

			TurnNode = SpawnNode<UAnimGraphNode_PTTurnInPlace>(AnimGraph, AimNode->NodePosX - 350, AimNode->NodePosY - 250);
			UEdGraphPin* TurnIn = FindPosePin(TurnNode, EGPD_Input, true);
			UEdGraphPin* TurnOut = FindPosePin(TurnNode, EGPD_Output, true);
			if (!TurnIn || !TurnOut || !Schema->TryCreateConnection(PrevOutput, TurnIn) || !Schema->TryCreateConnection(TurnOut, AimIn))
			{
				UE_LOG(LogPTEditor, Error, TEXT("SetupLocomotionABP: failed to insert PT Turn In Place"));
				return;
			}
		}

		// 변수 연결 (이미 연결된 핀은 건너뜀)
		const int32 TX = TurnNode->NodePosX - 250;
		const int32 TY = TurnNode->NodePosY + 200;
		BindVariable(AnimGraph, TurnNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTTurnInPlace, RootYawOffset), TEXT("TurnRootYawOffset"), TX, TY);
		BindVariable(AnimGraph, TurnNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTTurnInPlace, StepFoot), TEXT("TurnStepFoot"), TX, TY + 60);
		BindVariable(AnimGraph, TurnNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTTurnInPlace, StepAlpha), TEXT("TurnStepAlpha"), TX, TY + 120);
		BindVariable(AnimGraph, TurnNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTTurnInPlace, StepEndYawOffset), TEXT("TurnStepEndYawOffset"), TX, TY + 180);
		BindVariable(AnimGraph, TurnNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTTurnInPlace, FootLockAlpha), TEXT("TurnFootLockAlpha"), TX, TY + 240);

		const int32 AX = AimNode->NodePosX - 250;
		const int32 AY = AimNode->NodePosY + 200;
		BindVariable(AnimGraph, AimNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTAimOffset, AimYaw), TEXT("AimYaw"), AX, AY);
		BindVariable(AnimGraph, AimNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTAimOffset, AimPitch), TEXT("AimPitch"), AX, AY + 60);
		BindVariable(AnimGraph, AimNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTAimOffset, ForwardYawOffset), TEXT("TurnRootYawOffset"), AX, AY + 120);
		BindVariable(AnimGraph, AimNode, TEXT("Alpha"), TEXT("AimAlpha"), AX, AY + 180);
		BindVariable(AnimGraph, AimNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTAimOffset, LeanPitch), TEXT("BodyLeanPitch"), AX, AY + 240);

		FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);
		FKismetEditorUtilities::CompileBlueprint(AnimBP);
		if (AnimBP->Status == BS_Error)
		{
			UE_LOG(LogPTEditor, Error, TEXT("SetupLocomotionABP: %s compiled with errors - open it and check the compiler results"), *DestPackage);
		}

		UEditorLoadingAndSavingUtils::SavePackages({ AnimBP->GetOutermost() }, false);
		UE_LOG(LogPTEditor, Display, TEXT("SetupLocomotionABP: %s is ready (Turn In Place + Aim Offset)"), *DestPackage);
	}

	static FAutoConsoleCommand SetupLocomotionABPCmd(
		TEXT("pt.Editor.SetupLocomotionABP"),
		TEXT("Create/update an AnimBP with PT Turn In Place + PT Aim Offset. Usage: pt.Editor.SetupLocomotionABP [SourcePath] [DestPath]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));

	static FAutoConsoleCommand SetupAimOffsetABPCmd(
		TEXT("pt.Editor.SetupAimOffsetABP"),
		TEXT("Old name of pt.Editor.SetupLocomotionABP."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
