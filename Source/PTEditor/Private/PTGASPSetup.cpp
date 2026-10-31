// Copyright NeedsGames. All Rights Reserved.

/**
 * 에디터 콘솔 명령:  pt.Editor.SetupGASP [rebuild]
 *
 * Game Animation Sample(GASP)에서 이주한 에셋으로 다음을 만들거나 갱신합니다.
 *   1) /Game/PT/Animation/DA_PTLocomotionDatabases   - 이동 상태 → GASP PoseSearch 데이터베이스 규칙 (Chooser 대체)
 *   2) /Game/PT/Traversal/DA_PTTraversalAnimSet       - Hurdle / Vault / Mantle / Climb / Catch 몽타주 후보
 *   3) /Game/PT/Animation/ABP_PTMotionMatching        - UEFN 마네킹용 C++ 모션 매칭 AnimBP
 *        Motion Matching → Pose History("PoseHistory") → Slot(DefaultSlot)
 *        → Local To Component → PT Turn In Place → PT Aim Offset → Component To Local → Output Pose
 *
 * 데이터 에셋은 이미 있으면 그대로 둡니다 (직접 수정한 값 보존). 'rebuild' 인자를 주면 기본값으로 다시 채웁니다.
 * AnimBP 는 이미 있으면 그래프를 다시 만들지 않고, 빠진 핀 연결과 클래스 디폴트(DatabaseSet)만 갱신합니다.
 */

#include "AnimGraphNode_PTAimOffset.h"
#include "AnimGraphNode_PTTurnInPlace.h"
#include "Animation/PTLocomotionDatabaseSet.h"
#include "Animation/PTMotionMatchingAnimInstance.h"
#include "Traversal/PTTraversalAnimSet.h"

#include "AnimBlueprintExtension.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/Factory.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogPTGASP, Log, All);

namespace PTGASPSetup
{
	static const TCHAR* SkeletonPath = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin");
	static const TCHAR* MeshPath = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin");
	static const TCHAR* DatabaseRoot = TEXT("/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Databases/");
	static const TCHAR* TraversalRoot = TEXT("/Game/Characters/UEFN_Mannequin/Animations/Traversal/");

	static const TCHAR* DatabaseSetPackage = TEXT("/Game/PT/Animation/DA_PTLocomotionDatabases");
	static const TCHAR* TraversalSetPackage = TEXT("/Game/PT/Traversal/DA_PTTraversalAnimSet");
	static const TCHAR* AnimBPPackage = TEXT("/Game/PT/Animation/ABP_PTMotionMatching");

	static const FName PoseHistoryTag(TEXT("PoseHistory"));

	// ------------------------------------------------------------------------------------------
	// 공용 유틸
	// ------------------------------------------------------------------------------------------

	static FString ToObjectPath(const FString& PackageName)
	{
		return FString::Printf(TEXT("%s.%s"), *PackageName, *FPackageName::GetShortName(PackageName));
	}

	template<typename T>
	static T* LoadAsset(const FString& PackageName, bool bWarn = true)
	{
		if (!FPackageName::DoesPackageExist(PackageName))
		{
			if (bWarn)
			{
				UE_LOG(LogPTGASP, Warning, TEXT("Missing asset: %s"), *PackageName);
			}
			return nullptr;
		}
		T* Asset = LoadObject<T>(nullptr, *ToObjectPath(PackageName));
		if (!Asset && bWarn)
		{
			UE_LOG(LogPTGASP, Warning, TEXT("Could not load %s as %s"), *PackageName, *T::StaticClass()->GetName());
		}
		return Asset;
	}

	/** 있으면 로드, 없으면 생성. bOutCreated = 새로 만들었는지 */
	template<typename T>
	static T* GetOrCreateDataAsset(const FString& PackageName, bool& bOutCreated)
	{
		bOutCreated = false;
		if (T* Existing = LoadAsset<T>(PackageName, false))
		{
			return Existing;
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}
		T* Asset = NewObject<T>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Asset);
		Package->MarkPackageDirty();
		bOutCreated = true;
		return Asset;
	}

	static bool IsPosePin(const UEdGraphPin* Pin, bool bComponentSpace)
	{
		if (!Pin || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
		{
			return false;
		}
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
	static T* FindFirstNode(UEdGraph* Graph)
	{
		TArray<T*> Found;
		Graph->GetNodesOfClass(Found);
		return Found.Num() > 0 ? Found[0] : nullptr;
	}

	static UAnimGraphNode_Base* FindFirstNodeOfClass(UEdGraph* Graph, const UClass* Class)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Class && Node->IsA(Class))
			{
				return Cast<UAnimGraphNode_Base>(Node);
			}
		}
		return nullptr;
	}

	/** 내보내지 않은(Minimal API) 그래프 노드 클래스도 생성할 수 있도록 UClass 로 생성 */
	static UAnimGraphNode_Base* SpawnNodeOfClass(UEdGraph* Graph, UClass* Class, int32 X, int32 Y)
	{
		if (!Graph || !Class)
		{
			return nullptr;
		}
		UAnimGraphNode_Base* NewNode = NewObject<UAnimGraphNode_Base>(Graph, Class, NAME_None, RF_Transactional);
		Graph->AddNode(NewNode, false, false);
		NewNode->CreateNewGuid();
		NewNode->PostPlacedNewNode();
		NewNode->AllocateDefaultPins();
		NewNode->NodePosX = X;
		NewNode->NodePosY = Y;
		UAnimBlueprintExtension::RequestExtensionsForNode(NewNode);
		return NewNode;
	}

	template<typename T>
	static T* SpawnNode(UEdGraph* Graph, int32 X, int32 Y)
	{
		return Cast<T>(SpawnNodeOfClass(Graph, T::StaticClass(), X, Y));
	}

	static void ShowPin(UAnimGraphNode_Base* Node, FName PropertyName)
	{
		for (FOptionalPinFromProperty& Optional : Node->ShowPinForProperties)
		{
			if (Optional.PropertyName == PropertyName && !Optional.bShowPin)
			{
				Optional.bShowPin = true;
				Node->ReconstructNode();
				return;
			}
		}
	}

	/** 핀이 비어 있으면 변수 Get 노드를 만들어 연결 */
	static bool BindVariable(UEdGraph* Graph, UEdGraphNode* TargetNode, FName PinName, FName VariableName)
	{
		UEdGraphPin* TargetPin = TargetNode->FindPin(PinName, EGPD_Input);
		if (!TargetPin)
		{
			TargetNode->ReconstructNode();
			TargetPin = TargetNode->FindPin(PinName, EGPD_Input);
		}
		if (!TargetPin)
		{
			UE_LOG(LogPTGASP, Warning, TEXT("Pin '%s' not found on %s"), *PinName.ToString(), *TargetNode->GetName());
			return false;
		}
		if (TargetPin->LinkedTo.Num() > 0)
		{
			return true;
		}

		const int32 Index = TargetNode->Pins.IndexOfByKey(TargetPin);
		FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
		UK2Node_VariableGet* VarNode = Creator.CreateNode(false);
		VarNode->VariableReference.SetSelfMember(VariableName);
		VarNode->NodePosX = TargetNode->NodePosX - 280;
		VarNode->NodePosY = TargetNode->NodePosY + 80 + Index * 40;
		Creator.Finalize();

		UEdGraphPin* ValuePin = VarNode->GetValuePin();
		if (!ValuePin || !Graph->GetSchema()->TryCreateConnection(ValuePin, TargetPin))
		{
			UE_LOG(LogPTGASP, Warning, TEXT("Could not connect variable '%s' to pin '%s'"), *VariableName.ToString(), *PinName.ToString());
			return false;
		}
		return true;
	}

	/** 리플렉션으로 FMemberReference 프로퍼티(노드 함수 바인딩) 설정 */
	static void SetFunctionBinding(UObject* Node, FName PropertyName, FName FunctionName)
	{
		if (FStructProperty* Prop = FindFProperty<FStructProperty>(Node->GetClass(), PropertyName))
		{
			if (FMemberReference* Ref = Prop->ContainerPtrToValuePtr<FMemberReference>(Node))
			{
				Ref->SetSelfMember(FunctionName);
			}
		}
		else
		{
			UE_LOG(LogPTGASP, Warning, TEXT("Property %s not found on %s"), *PropertyName.ToString(), *Node->GetClass()->GetName());
		}
	}

	/** UAnimGraphNode_Base::Tag (private) 설정 → 런타임에 태그로 노드 검색 가능 (MotionMatch 의 PoseHistoryName) */
	static void SetNodeTag(UAnimGraphNode_Base* Node, FName Tag)
	{
		if (FNameProperty* Prop = FindFProperty<FNameProperty>(UAnimGraphNode_Base::StaticClass(), TEXT("Tag")))
		{
			Prop->SetPropertyValue_InContainer(Node, Tag);
		}
		// 태그를 쓰는 노드는 AnimBlueprintExtension_Tag 가 필요 (에디터에서 태그를 입력할 때와 동일하게 요청)
		// 요청하지 않으면 컴파일 중 'Cast of nullptr to AnimBlueprintExtension_Tag' 로 크래시
		UAnimBlueprintExtension::RequestExtensionsForNode(Node);
	}

	// ------------------------------------------------------------------------------------------
	// 1) 로코모션 데이터베이스 규칙
	// ------------------------------------------------------------------------------------------

	static UPoseSearchDatabase* DB(const TCHAR* RelativePath)
	{
		return LoadAsset<UPoseSearchDatabase>(FString(DatabaseRoot) + RelativePath);
	}

	static FPTDatabaseRule MakeRule(const TCHAR* Name,
		std::initializer_list<EPTMovementSituation> Situations,
		std::initializer_list<EPTMovementState> States,
		std::initializer_list<EPTStance> Stances,
		std::initializer_list<EPTGait> Gaits,
		std::initializer_list<const TCHAR*> Databases)
	{
		FPTDatabaseRule Rule;
		Rule.RuleName = FName(Name);
		Rule.Situations = TArray<EPTMovementSituation>(Situations);
		Rule.MovementStates = TArray<EPTMovementState>(States);
		Rule.Stances = TArray<EPTStance>(Stances);
		Rule.Gaits = TArray<EPTGait>(Gaits);
		for (const TCHAR* Path : Databases)
		{
			if (UPoseSearchDatabase* Database = DB(Path))
			{
				Rule.Databases.Add(Database);
			}
		}
		Rule.bStopEvaluation = true;
		return Rule;
	}

	static void FillDatabaseSet(UPTLocomotionDatabaseSet* Set)
	{
		using S = EPTMovementSituation;
		using M = EPTMovementState;
		using St = EPTStance;
		using G = EPTGait;

		Set->Rules.Reset();

		// 공중: 점프 / 낙하 (파쿠르 직후 낙하 포함)
		Set->Rules.Add(MakeRule(TEXT("InAir"), { S::InAir }, {}, {}, {},
			{ TEXT("Dense/PSD_Dense_Jumps"), TEXT("Dense/PSD_Dense_Jumps_Far"), TEXT("Dense/PSD_Dense_Jumps_FromTraversal") }));

		// 파쿠르 직후: GASP 의 FromTraversal 데이터 (몽타주 끝 포즈와 자연스럽게 이어짐)
		Set->Rules.Add(MakeRule(TEXT("FromTraversal_Idle"), { S::FromTraversal }, { M::Idle }, {}, {},
			{ TEXT("Dense/PSD_Dense_Stand_Walk_FromTraversal"), TEXT("Dense/PSD_Dense_Stand_Idles"), TEXT("Sparse/PSD_Sparse_Stand_Walk_Stops"), TEXT("Sparse/PSD_Sparse_Stand_Run_Stops") }));
		Set->Rules.Add(MakeRule(TEXT("FromTraversal_Walk"), { S::FromTraversal }, { M::Moving }, {}, { G::Walk },
			{ TEXT("Dense/PSD_Dense_Stand_Walk_FromTraversal"), TEXT("Sparse/PSD_Sparse_Stand_Walk_Loops"), TEXT("Sparse/PSD_Sparse_Stand_Walk_Starts") }));
		Set->Rules.Add(MakeRule(TEXT("FromTraversal_Run"), { S::FromTraversal }, { M::Moving }, {}, { G::Run, G::Sprint },
			{ TEXT("Dense/PSD_Dense_Stand_Run_FromTraversal"), TEXT("Sparse/PSD_Sparse_Stand_Run_Loops"), TEXT("Sparse/PSD_Sparse_Stand_Sprint_Loops") }));

		// 착지
		Set->Rules.Add(MakeRule(TEXT("LandHeavy_Idle"), { S::LandHeavy }, { M::Idle }, { St::Stand }, {}, { TEXT("Dense/PSD_Dense_Stand_Idle_Lands_Heavy") }));
		Set->Rules.Add(MakeRule(TEXT("LandLight_Idle"), { S::LandLight }, { M::Idle }, { St::Stand }, {}, { TEXT("Dense/PSD_Dense_Stand_Idle_Lands_Light") }));
		Set->Rules.Add(MakeRule(TEXT("LandHeavy_Walk"), { S::LandHeavy }, { M::Moving }, { St::Stand }, { G::Walk }, { TEXT("Dense/PSD_Dense_Stand_Walk_Lands_Heavy") }));
		Set->Rules.Add(MakeRule(TEXT("LandLight_Walk"), { S::LandLight }, { M::Moving }, { St::Stand }, { G::Walk }, { TEXT("Dense/PSD_Dense_Stand_Walk_Lands_Light") }));
		Set->Rules.Add(MakeRule(TEXT("LandHeavy_Run"), { S::LandHeavy }, { M::Moving }, { St::Stand }, { G::Run }, { TEXT("Dense/PSD_Dense_Stand_Run_Lands_Heavy") }));
		Set->Rules.Add(MakeRule(TEXT("LandLight_Run"), { S::LandLight }, { M::Moving }, { St::Stand }, { G::Run }, { TEXT("Dense/PSD_Dense_Stand_Run_Lands_Light") }));
		Set->Rules.Add(MakeRule(TEXT("LandHeavy_Sprint"), { S::LandHeavy }, { M::Moving }, { St::Stand }, { G::Sprint }, { TEXT("Dense/PSD_Dense_Stand_Sprint_Lands_Heavy") }));
		Set->Rules.Add(MakeRule(TEXT("LandLight_Sprint"), { S::LandLight }, { M::Moving }, { St::Stand }, { G::Sprint }, { TEXT("Dense/PSD_Dense_Stand_Sprint_Lands_Light") }));

		// 앉기
		Set->Rules.Add(MakeRule(TEXT("Crouch_Idle"), {}, { M::Idle }, { St::Crouch }, {},
			{ TEXT("Dense/PSD_Dense_Crouch_Idles"), TEXT("Sparse/PSD_Sparse_Crouch_Walk_Stops") }));
		Set->Rules.Add(MakeRule(TEXT("Crouch_Move"), {}, { M::Moving }, { St::Crouch }, {},
			{ TEXT("Sparse/PSD_Sparse_Crouch_Walk_Loops"), TEXT("Sparse/PSD_Sparse_Crouch_Walk_Starts"), TEXT("Sparse/PSD_Sparse_Crouch_Walk_Pivots") }));

		// 서기: 정지(대기 + 각 Gait 의 멈춤 동작), 걷기 / 달리기 / 전력질주
		// 제자리 회전은 PT Turn In Place 노드가 담당하므로 TurnInPlace 데이터베이스는 넣지 않음
		Set->Rules.Add(MakeRule(TEXT("Stand_Idle"), {}, { M::Idle }, {}, {},
			{ TEXT("Dense/PSD_Dense_Stand_Idles"), TEXT("Sparse/PSD_Sparse_Stand_Walk_Stops"), TEXT("Sparse/PSD_Sparse_Stand_Run_Stops"), TEXT("Sparse/PSD_Sparse_Stand_Sprint_Stops") }));
		Set->Rules.Add(MakeRule(TEXT("Walk"), {}, { M::Moving }, {}, { G::Walk },
			{ TEXT("Sparse/PSD_Sparse_Stand_Walk_Loops"), TEXT("Sparse/PSD_Sparse_Stand_Walk_Starts"), TEXT("Sparse/PSD_Sparse_Stand_Walk_Pivots"), TEXT("Dense/PSD_Dense_Stand_Walk_SpinTransition") }));
		Set->Rules.Add(MakeRule(TEXT("Run"), {}, { M::Moving }, {}, { G::Run },
			{ TEXT("Sparse/PSD_Sparse_Stand_Run_Loops"), TEXT("Sparse/PSD_Sparse_Stand_Run_Starts"), TEXT("Sparse/PSD_Sparse_Stand_Run_Pivots"), TEXT("Dense/PSD_Dense_Stand_Run_SpinTransition") }));
		Set->Rules.Add(MakeRule(TEXT("Sprint"), {}, { M::Moving }, {}, { G::Sprint },
			{ TEXT("Sparse/PSD_Sparse_Stand_Sprint_Loops"), TEXT("Sparse/PSD_Sparse_Stand_Sprint_Starts"), TEXT("Sparse/PSD_Sparse_Stand_Sprint_Pivots") }));

		Set->FallbackDatabases.Reset();
		if (UPoseSearchDatabase* Idles = DB(TEXT("Dense/PSD_Dense_Stand_Idles")))
		{
			Set->FallbackDatabases.Add(Idles);
		}
	}

	// ------------------------------------------------------------------------------------------
	// 2) 파쿠르 몽타주 세트
	// ------------------------------------------------------------------------------------------

	static void AddTraversal(UPTTraversalAnimSet* Set, EPTTraversalAction Action, const TCHAR* RelativePath,
		float MinHeight, float MaxHeight, float MinSpeed, float MaxSpeed, bool bInAir = false)
	{
		UAnimMontage* Montage = LoadAsset<UAnimMontage>(FString(TraversalRoot) + RelativePath);
		if (!Montage)
		{
			return;
		}
		FPTTraversalAnimEntry& Entry = Set->Entries.AddDefaulted_GetRef();
		Entry.Action = Action;
		Entry.Montage = Montage;
		Entry.MinHeight = MinHeight;
		Entry.MaxHeight = MaxHeight;
		Entry.MinSpeed = MinSpeed;
		Entry.MaxSpeed = MaxSpeed;
		Entry.bInAir = bInAir;
	}

	static void FillTraversalSet(UPTTraversalAnimSet* Set)
	{
		using A = EPTTraversalAction;
		Set->Entries.Reset();
		Set->bUseMotionMatching = true;   // Pose History("PoseHistory") 가 있는 ABP_PTMotionMatching 에서 시작 프레임까지 선택
		Set->PoseHistoryName = PoseHistoryTag;

		constexpr float Stand = 150.f;   // 이하: 제자리에서
		constexpr float Walk = 350.f;    // 이하: 걷다가, 초과: 달리다가
		constexpr float Any = 10000.f;

		// Hurdle (달리며 뛰어넘기)
		for (const TCHAR* Foot : { TEXT("Lfoot"), TEXT("Rfoot") })
		{
			AddTraversal(Set, A::Hurdle, *FString::Printf(TEXT("Hurdle/AM_M_Neutral_Traversal_Hurdle_1_0_run_F_%s"), Foot), 50.f, 130.f, Walk, Any);
			AddTraversal(Set, A::Hurdle, *FString::Printf(TEXT("Hurdle/AM_M_Neutral_Traversal_Hurdle_1_0_run_F_V2_%s"), Foot), 50.f, 130.f, Walk, Any);
			AddTraversal(Set, A::Hurdle, *FString::Printf(TEXT("Hurdle/AM_M_Neutral_Traversal_Hurdle_1_0_walk_F_%s"), Foot), 50.f, 130.f, Stand, Walk);
			AddTraversal(Set, A::Hurdle, *FString::Printf(TEXT("Hurdle/AM_M_Neutral_Traversal_Hurdle_1_0_walk_F_V2_%s"), Foot), 50.f, 130.f, Stand, Walk);
			AddTraversal(Set, A::Hurdle, *FString::Printf(TEXT("Hurdle/AM_M_Neutral_Traversal_Hurdle_1_0_stand_F_V2_%s"), Foot), 50.f, 130.f, 0.f, Stand);
		}

		// Vault (손 짚고 넘기)
		AddTraversal(Set, A::Vault, TEXT("Vault/AM_M_Neutral_Traversal_Vault_1_0_run_F_Lfoot"), 50.f, 130.f, Walk, Any);
		AddTraversal(Set, A::Vault, TEXT("Vault/AM_M_Neutral_Traversal_Vault_1_0_walk_F_Rfoot"), 50.f, 130.f, Stand, Walk);
		AddTraversal(Set, A::Vault, TEXT("Vault/AM_M_Neutral_Traversal_Vault_1_0_stand_F_Lfoot"), 50.f, 130.f, 0.f, Stand);

		// Mantle (낮은 곳 올라서기) / Climb (높은 곳 기어오르기)
		for (const TCHAR* Foot : { TEXT("Lfoot"), TEXT("Rfoot") })
		{
			AddTraversal(Set, A::Mantle, *FString::Printf(TEXT("Mantle/AM_M_Neutral_Traversal_Mantle_1_0_run_F_%s"), Foot), 50.f, 135.f, Walk, Any);
			AddTraversal(Set, A::Mantle, *FString::Printf(TEXT("Mantle/AM_M_Neutral_Traversal_Mantle_1_0_walk_F_%s"), Foot), 50.f, 135.f, Stand, Walk);
			AddTraversal(Set, A::Mantle, *FString::Printf(TEXT("Mantle/AM_M_Neutral_Traversal_Mantle_1_0_stand_F_%s"), Foot), 50.f, 135.f, 0.f, Stand);
			AddTraversal(Set, A::Mantle, *FString::Printf(TEXT("Climb/AM_M_Neutral_Traversal_Climb_Start_2_5_run_F_%s"), Foot), 135.f, 280.f, Walk, Any);
			AddTraversal(Set, A::Mantle, *FString::Printf(TEXT("Climb/AM_M_Neutral_Traversal_Climb_Start_2_5_walk_F_%s"), Foot), 135.f, 280.f, Stand, Walk);
			AddTraversal(Set, A::Mantle, *FString::Printf(TEXT("Climb/AM_M_Neutral_Traversal_Climb_Start_2_5_stand_F_%s"), Foot), 135.f, 280.f, 0.f, Stand);
		}

		// 공중에서 모서리 잡기 (낙하 중 Mantle)
		AddTraversal(Set, A::Mantle, TEXT("Catch/Mantle/AM_M_Neutral_Traversal_Catch_Mantle_low_stand"), 50.f, 110.f, 0.f, Any, true);
		AddTraversal(Set, A::Mantle, TEXT("Catch/Mantle/AM_M_Neutral_Traversal_Catch_Mantle_med_stand"), 110.f, 180.f, 0.f, Any, true);
		AddTraversal(Set, A::Mantle, TEXT("Catch/Cliff/AM_M_Neutral_Traversal_Catch_Cliff_high_standF"), 180.f, 280.f, 0.f, Any, true);
	}

	// ------------------------------------------------------------------------------------------
	// 3) 모션 매칭 AnimBP
	// ------------------------------------------------------------------------------------------

	static UAnimBlueprint* GetOrCreateAnimBP(USkeleton* Skeleton, USkeletalMesh* PreviewMesh, bool& bOutCreated)
	{
		bOutCreated = false;
		if (UAnimBlueprint* Existing = LoadAsset<UAnimBlueprint>(AnimBPPackage, false))
		{
			return Existing;
		}

		// UAnimBlueprintFactory 는 export 되지 않으므로 클래스 경로 + 리플렉션으로 설정
		UClass* FactoryClass = LoadObject<UClass>(nullptr, TEXT("/Script/UnrealEd.AnimBlueprintFactory"));
		UFactory* Factory = FactoryClass ? NewObject<UFactory>(GetTransientPackage(), FactoryClass) : nullptr;
		if (!Factory)
		{
			UE_LOG(LogPTGASP, Error, TEXT("AnimBlueprintFactory not found"));
			return nullptr;
		}
		if (FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(FactoryClass, TEXT("ParentClass")))
		{
			Prop->SetObjectPropertyValue_InContainer(Factory, UPTMotionMatchingAnimInstance::StaticClass());
		}
		if (FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(FactoryClass, TEXT("TargetSkeleton")))
		{
			Prop->SetObjectPropertyValue_InContainer(Factory, Skeleton);
		}
		if (FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(FactoryClass, TEXT("PreviewSkeletalMesh")))
		{
			Prop->SetObjectPropertyValue_InContainer(Factory, PreviewMesh);
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(AssetTools.CreateAsset(
			FPackageName::GetShortName(AnimBPPackage), FPackageName::GetLongPackagePath(AnimBPPackage), UAnimBlueprint::StaticClass(), Factory));
		bOutCreated = (AnimBP != nullptr);
		return AnimBP;
	}

	static UEdGraph* FindAnimGraph(UAnimBlueprint* AnimBP)
	{
		for (UEdGraph* Graph : AnimBP->FunctionGraphs)
		{
			if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	static bool BuildGraph(UAnimBlueprint* AnimBP)
	{
		UEdGraph* AnimGraph = FindAnimGraph(AnimBP);
		if (!AnimGraph)
		{
			UE_LOG(LogPTGASP, Error, TEXT("AnimGraph not found in %s"), *AnimBP->GetName());
			return false;
		}

		UClass* MMClass = LoadObject<UClass>(nullptr, TEXT("/Script/PoseSearchEditor.AnimGraphNode_MotionMatching"));
		UClass* HistoryClass = LoadObject<UClass>(nullptr, TEXT("/Script/PoseSearchEditor.AnimGraphNode_PoseSearchHistoryCollector"));
		if (!MMClass || !HistoryClass)
		{
			UE_LOG(LogPTGASP, Error, TEXT("PoseSearch editor node classes not found (is the PoseSearch plugin enabled?)"));
			return false;
		}

		const UEdGraphSchema* Schema = AnimGraph->GetSchema();
		UAnimGraphNode_Root* Root = FindFirstNode<UAnimGraphNode_Root>(AnimGraph);
		UEdGraphPin* RootPin = FindPosePin(Root, EGPD_Input, false);
		if (!RootPin)
		{
			UE_LOG(LogPTGASP, Error, TEXT("Output Pose node not found"));
			return false;
		}

		UAnimGraphNode_Base* MMNode = FindFirstNodeOfClass(AnimGraph, MMClass);
		UAnimGraphNode_Base* HistoryNode = FindFirstNodeOfClass(AnimGraph, HistoryClass);
		UAnimGraphNode_PTTurnInPlace* TurnNode = FindFirstNode<UAnimGraphNode_PTTurnInPlace>(AnimGraph);
		UAnimGraphNode_PTAimOffset* AimNode = FindFirstNode<UAnimGraphNode_PTAimOffset>(AnimGraph);

		if (!MMNode)
		{
			// 새 그래프 구성
			RootPin->BreakAllPinLinks();
			const int32 X = Root->NodePosX - 2400;
			const int32 Y = Root->NodePosY;

			MMNode = SpawnNodeOfClass(AnimGraph, MMClass, X, Y);
			HistoryNode = SpawnNodeOfClass(AnimGraph, HistoryClass, X + 400, Y);
			UAnimGraphNode_Slot* SlotNode = SpawnNode<UAnimGraphNode_Slot>(AnimGraph, X + 750, Y);
			UAnimGraphNode_LocalToComponentSpace* ToComponent = SpawnNode<UAnimGraphNode_LocalToComponentSpace>(AnimGraph, X + 1050, Y);
			TurnNode = SpawnNode<UAnimGraphNode_PTTurnInPlace>(AnimGraph, X + 1300, Y);
			AimNode = SpawnNode<UAnimGraphNode_PTAimOffset>(AnimGraph, X + 1700, Y);
			UAnimGraphNode_ComponentToLocalSpace* ToLocal = SpawnNode<UAnimGraphNode_ComponentToLocalSpace>(AnimGraph, X + 2100, Y);
			if (!MMNode || !HistoryNode || !SlotNode || !ToComponent || !TurnNode || !AimNode || !ToLocal)
			{
				UE_LOG(LogPTGASP, Error, TEXT("Failed to spawn anim graph nodes"));
				return false;
			}
			SlotNode->Node.SlotName = FName(TEXT("DefaultSlot"));

			UEdGraphPin* Chain[] = {
				FindPosePin(MMNode, EGPD_Output, false),       FindPosePin(HistoryNode, EGPD_Input, false),
				FindPosePin(HistoryNode, EGPD_Output, false),  FindPosePin(SlotNode, EGPD_Input, false),
				FindPosePin(SlotNode, EGPD_Output, false),     FindPosePin(ToComponent, EGPD_Input, false),
				FindPosePin(ToComponent, EGPD_Output, true),   FindPosePin(TurnNode, EGPD_Input, true),
				FindPosePin(TurnNode, EGPD_Output, true),      FindPosePin(AimNode, EGPD_Input, true),
				FindPosePin(AimNode, EGPD_Output, true),       FindPosePin(ToLocal, EGPD_Input, true),
				FindPosePin(ToLocal, EGPD_Output, false),      RootPin };
			for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Chain); i += 2)
			{
				if (!Chain[i] || !Chain[i + 1] || !Schema->TryCreateConnection(Chain[i], Chain[i + 1]))
				{
					UE_LOG(LogPTGASP, Error, TEXT("Failed to connect pose pins (link %d)"), i / 2);
					return false;
				}
			}
		}

		// Motion Matching: 데이터베이스는 C++ 이 매 프레임 지정, 블렌드 시간은 변수
		MMNode->UpdateFunction.SetSelfMember(GET_FUNCTION_NAME_CHECKED(UPTMotionMatchingAnimInstance, UpdateMotionMatching));
		SetFunctionBinding(MMNode, TEXT("OnMotionMatchingStateUpdatedFunction"), GET_FUNCTION_NAME_CHECKED(UPTMotionMatchingAnimInstance, UpdateMotionMatchingPostSelection));
		ShowPin(MMNode, TEXT("BlendTime"));
		BindVariable(AnimGraph, MMNode, TEXT("BlendTime"), TEXT("MotionMatchingBlendTime"));

		// Pose History: C++ 궤적 사용 + 태그 (파쿠르 몽타주 모션 매칭에서 사용)
		if (HistoryNode)
		{
			SetNodeTag(HistoryNode, PoseHistoryTag);
			BindVariable(AnimGraph, HistoryNode, TEXT("TransformTrajectory"), TEXT("Trajectory"));
		}

		if (TurnNode)
		{
			BindVariable(AnimGraph, TurnNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTTurnInPlace, RootYawOffset), TEXT("TurnRootYawOffset"));
			BindVariable(AnimGraph, TurnNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTTurnInPlace, StepFoot), TEXT("TurnStepFoot"));
			BindVariable(AnimGraph, TurnNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTTurnInPlace, StepAlpha), TEXT("TurnStepAlpha"));
			BindVariable(AnimGraph, TurnNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTTurnInPlace, StepEndYawOffset), TEXT("TurnStepEndYawOffset"));
			BindVariable(AnimGraph, TurnNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTTurnInPlace, FootLockAlpha), TEXT("TurnFootLockAlpha"));
		}
		if (AimNode)
		{
			BindVariable(AnimGraph, AimNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTAimOffset, AimYaw), TEXT("AimYaw"));
			BindVariable(AnimGraph, AimNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTAimOffset, AimPitch), TEXT("AimPitch"));
			BindVariable(AnimGraph, AimNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTAimOffset, ForwardYawOffset), TEXT("TurnRootYawOffset"));
			BindVariable(AnimGraph, AimNode, GET_MEMBER_NAME_CHECKED(FAnimNode_PTAimOffset, LeanPitch), TEXT("BodyLeanPitch"));
			BindVariable(AnimGraph, AimNode, TEXT("Alpha"), TEXT("AimAlpha"));
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------

	static void Run(const TArray<FString>& Args)
	{
		const bool bRebuild = Args.Contains(TEXT("rebuild"));

		USkeleton* Skeleton = LoadAsset<USkeleton>(SkeletonPath);
		USkeletalMesh* Mesh = LoadAsset<USkeletalMesh>(MeshPath);
		if (!Skeleton || !Mesh)
		{
			UE_LOG(LogPTGASP, Error, TEXT("SetupGASP: UEFN mannequin not found - copy the GASP assets into PT/Content first (see Docs/GASP.md)"));
			return;
		}

		TArray<UPackage*> ToSave;

		// 1) 데이터베이스 규칙
		bool bCreated = false;
		UPTLocomotionDatabaseSet* DatabaseSet = GetOrCreateDataAsset<UPTLocomotionDatabaseSet>(DatabaseSetPackage, bCreated);
		if (DatabaseSet && (bCreated || bRebuild))
		{
			DatabaseSet->Modify();
			FillDatabaseSet(DatabaseSet);
			ToSave.Add(DatabaseSet->GetOutermost());
			UE_LOG(LogPTGASP, Display, TEXT("SetupGASP: %s - %d rules"), DatabaseSetPackage, DatabaseSet->Rules.Num());
		}

		// 2) 파쿠르 몽타주 세트
		UPTTraversalAnimSet* TraversalSet = GetOrCreateDataAsset<UPTTraversalAnimSet>(TraversalSetPackage, bCreated);
		if (TraversalSet && (bCreated || bRebuild))
		{
			TraversalSet->Modify();
			FillTraversalSet(TraversalSet);
			ToSave.Add(TraversalSet->GetOutermost());
			UE_LOG(LogPTGASP, Display, TEXT("SetupGASP: %s - %d montages"), TraversalSetPackage, TraversalSet->Entries.Num());
		}

		// 3) AnimBP
		UAnimBlueprint* AnimBP = GetOrCreateAnimBP(Skeleton, Mesh, bCreated);
		if (!AnimBP)
		{
			UE_LOG(LogPTGASP, Error, TEXT("SetupGASP: could not create %s"), AnimBPPackage);
		}
		else
		{
			if (!AnimBP->ParentClass || !AnimBP->ParentClass->IsChildOf(UPTMotionMatchingAnimInstance::StaticClass()))
			{
				AnimBP->ParentClass = UPTMotionMatchingAnimInstance::StaticClass();
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
			}
			FBlueprintEditorUtils::RefreshAllNodes(AnimBP);
			FKismetEditorUtilities::CompileBlueprint(AnimBP); // 부모 변수/함수를 그래프에서 쓸 수 있도록 먼저 컴파일

			if (BuildGraph(AnimBP))
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);
				FKismetEditorUtilities::CompileBlueprint(AnimBP);
				if (AnimBP->Status == BS_Error)
				{
					UE_LOG(LogPTGASP, Error, TEXT("SetupGASP: %s compiled with errors - open it and check the compiler results"), AnimBPPackage);
				}
			}

			// 클래스 디폴트: 데이터베이스 규칙
			if (UPTMotionMatchingAnimInstance* CDO = AnimBP->GeneratedClass ? Cast<UPTMotionMatchingAnimInstance>(AnimBP->GeneratedClass->GetDefaultObject()) : nullptr)
			{
				if (CDO->DatabaseSet != DatabaseSet)
				{
					CDO->Modify();
					CDO->DatabaseSet = DatabaseSet;
				}
			}
			AnimBP->MarkPackageDirty();
			ToSave.Add(AnimBP->GetOutermost());
		}

		if (ToSave.Num() > 0)
		{
			UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
		}
		UE_LOG(LogPTGASP, Display, TEXT("SetupGASP: done. Press Play - PTCharacter uses SKM_UEFN_Mannequin + ABP_PTMotionMatching automatically."));
	}

	static FAutoConsoleCommand SetupCommand(
		TEXT("pt.Editor.SetupGASP"),
		TEXT("Create/update the motion matching AnimBP, locomotion database rules and traversal montage set from migrated GASP assets. Usage: pt.Editor.SetupGASP [rebuild]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
