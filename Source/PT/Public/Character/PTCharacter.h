// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/PTAbilitySet.h"
#include "Character/PTLocomotionTypes.h"
#include "PTCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UAnimInstance;
class UInputMappingContext;
class UMotionWarpingComponent;
class UPTTraversalComponent;
class USkeletalMesh;
class UPTAbilitySystemComponent;
class UPTCharacterMovementComponent;
class UPTInputConfig;
struct FInputActionValue;

/** 시뮬레이티드 프록시에 보낼 압축 가속도 (Lyra 방식) */
USTRUCT()
struct FPTReplicatedAcceleration
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 AccelXYRadians = 0;   // XY 방향 [0, 2PI]

	UPROPERTY()
	uint8 AccelXYMagnitude = 0; // [0, MaxAcceleration]

	UPROPERTY()
	int8 AccelZ = 0;            // [-MaxAcceleration, MaxAcceleration]
};

/**
 * PT 플레이어 캐릭터.
 *  - 이동: UPTCharacterMovementComponent (Gait/Stance/RotationMode 예측)
 *  - 애니메이션: UPTMotionMatchingAnimInstance (C++ 모션 매칭)
 *  - 어빌리티: ASC 는 APTPlayerState 소유, 이 캐릭터는 Avatar
 *
 * 사용법: 이 클래스를 상속한 BP 를 만들고 Mesh(GASP 캐릭터) / AnimClass(ABP, 부모 = PTMotionMatchingAnimInstance) 를 지정하세요.
 */
UCLASS(Config = Game)
class PT_API APTCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APTCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "PT|Character")
	UPTAbilitySystemComponent* GetPTAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "PT|Character")
	UPTCharacterMovementComponent* GetPTMovementComponent() const;

	UFUNCTION(BlueprintPure, Category = "PT|Character")
	EPTGait GetGait() const;

	UFUNCTION(BlueprintPure, Category = "PT|Character")
	EPTStance GetStance() const;

	UFUNCTION(BlueprintPure, Category = "PT|Character")
	EPTRotationMode GetRotationMode() const;

	/** 서버: PlayerState ASC 에 부여할 어빌리티 세트 목록 */
	const TArray<TObjectPtr<UPTAbilitySet>>& GetAbilitySets() const { return AbilitySets; }
	const TArray<FPTAbilitySet_GameplayAbility>& GetDefaultAbilities() const { return DefaultAbilities; }

	/**
	 * 조준(카메라) 회전.
	 *  - 로컬/서버: 컨트롤러 회전
	 *  - 시뮬레이티드 프록시: 엔진이 복제하는 Pitch(RemoteViewPitch) + 이 클래스가 복제하는 Yaw
	 */
	UFUNCTION(BlueprintPure, Category = "PT|Character")
	FRotator GetAimRotation() const;

	/** 호스트 인계 복원 시 위치/회전 강제 적용 (서버) */
	void TeleportForMigrationRestore(const FTransform& InTransform, const FRotator& InControlRotation);

	// ACharacter / APawn
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

protected:
	virtual void BeginPlay() override;

	void InitAbilityActorInfo();
	void AddInputMappingContext();
	void SetLooseStatusTag(const FGameplayTag& Tag, bool bEnabled);

	// ---- Input ----
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_Crouch(const FInputActionValue& Value);
	void Input_ToggleWalk(const FInputActionValue& Value);
	void Input_ToggleStrafe(const FInputActionValue& Value);
	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	UFUNCTION()
	void OnRep_ReplicatedAcceleration();

	UFUNCTION()
	void OnRep_ReplicatedIntentFlags();

	/** 체력 0 (서버) */
	void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float OldValue, float NewValue);

	UFUNCTION(BlueprintNativeEvent, Category = "PT|Character")
	void OnOutOfHealth();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PT|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PT|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	/** 파쿠르 몽타주의 모션 워핑 (FrontLedge / BackLedge / BackFloor) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PT|Traversal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	/** 파쿠르 판정 (점프 입력 시) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PT|Traversal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPTTraversalComponent> Traversal;

	/** 비워두면 기본 런타임 입력 구성(UPTInputConfig::CreateDefaultRuntimeConfig) 사용 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Input")
	TObjectPtr<UPTInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Input")
	int32 MappingContextPriority = 0;

	/** 서버에서 PlayerState 의 ASC 로 부여 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Abilities")
	TArray<TObjectPtr<UPTAbilitySet>> AbilitySets;

	/** AbilitySets 외에 추가로 부여할 단일 어빌리티 (기본: Jump / Sprint) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PT|Abilities")
	TArray<FPTAbilitySet_GameplayAbility> DefaultAbilities;

	/**
	 * 임시 외형: Mesh 가 지정되지 않았을 때 사용할 메시 / AnimBP (UE 기본 마네킹 + 템플릿 ABP).
	 * GASP 에셋으로 BP_PTCharacter 를 만들면 그 Mesh 가 우선합니다.
	 */
	/**
	 * 메시가 지정되지 않았을 때 우선 사용할 GASP 캐릭터 (UEFN 마네킹 + C++ 모션 매칭 AnimBP).
	 * 두 에셋이 모두 있을 때만 사용 (pt.Editor.SetupGASP 로 AnimBP 생성)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "PT|Placeholder")
	TSoftObjectPtr<USkeletalMesh> MotionMatchingMesh;

	UPROPERTY(EditDefaultsOnly, Category = "PT|Placeholder")
	TSoftClassPtr<UAnimInstance> MotionMatchingAnimClass;

	UPROPERTY(EditDefaultsOnly, Category = "PT|Placeholder")
	TSoftObjectPtr<USkeletalMesh> PlaceholderMesh;

	/** 위에서부터 로드되는 첫 번째 AnimBP 사용 (기본: 에임 오프셋이 들어간 ABP_PTUnarmed → 원본 ABP_Unarmed) */
	UPROPERTY(EditDefaultsOnly, Category = "PT|Placeholder")
	TArray<TSoftClassPtr<UAnimInstance>> PlaceholderAnimClasses;

	UPROPERTY(Transient)
	TObjectPtr<UPTInputConfig> RuntimeInputConfig;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeMappingContext;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedAcceleration)
	FPTReplicatedAcceleration ReplicatedAcceleration;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedIntentFlags)
	uint8 ReplicatedIntentFlags = 0;

	/** 시뮬레이티드 프록시용 조준 Yaw (엔진은 Pitch 만 복제하므로 별도 복제) */
	UPROPERTY(Transient, Replicated)
	uint16 ReplicatedAimYaw = 0;

	TArray<uint32> AbilityInputBindHandles;
	FDelegateHandle OutOfHealthHandle;
};
