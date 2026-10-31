// Copyright NeedsGames. All Rights Reserved.

#include "Character/PTCharacter.h"
#include "Character/PTCharacterMovementComponent.h"
#include "AbilitySystem/PTAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/PTHealthSet.h"
#include "AbilitySystem/Abilities/PTGameplayAbility_Jump.h"
#include "AbilitySystem/Abilities/PTGameplayAbility_Sprint.h"
#include "AbilitySystem/Abilities/PTGameplayAbility_Traversal.h"
#include "Traversal/PTTraversalComponent.h"
#include "MotionWarpingComponent.h"
#include "Core/PTPlayerState.h"
#include "Input/PTInputComponent.h"
#include "Input/PTInputConfig.h"
#include "PTGameplayTags.h"
#include "PTLogChannels.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTCharacter)

APTCharacter::APTCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPTCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(100.f);
	SetMinNetUpdateFrequency(33.f);

	GetCapsuleComponent()->InitCapsuleSize(30.f, 86.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// GASP 메시 기준 (-90 Z, 루트가 발 위치)
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -86.f), FRotator(0.f, -90.f, 0.f));
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->SocketOffset = FVector(0.f, 40.f, 50.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));
	Traversal = CreateDefaultSubobject<UPTTraversalComponent>(TEXT("Traversal"));

	// 에셋 연결 전 임시 외형 (Content/Characters/Mannequins)
	MotionMatchingMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin")));
	MotionMatchingAnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/PT/Animation/ABP_PTMotionMatching.ABP_PTMotionMatching_C")));
	PlaceholderMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")));
	PlaceholderAnimClasses.Add(TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/PT/Animation/ABP_PTUnarmed.ABP_PTUnarmed_C"))));
	PlaceholderAnimClasses.Add(TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"))));

	// 기본 어빌리티 (AbilitySet 없이도 동작)
	FPTAbilitySet_GameplayAbility Jump;
	Jump.Ability = UPTGameplayAbility_Jump::StaticClass();
	Jump.InputTag = PTGameplayTags::InputTag_Jump;
	DefaultAbilities.Add(Jump);

	FPTAbilitySet_GameplayAbility Sprint;
	Sprint.Ability = UPTGameplayAbility_Sprint::StaticClass();
	Sprint.InputTag = PTGameplayTags::InputTag_Sprint;
	DefaultAbilities.Add(Sprint);

	// 파쿠르: 입력 태그 없이 Event.Traversal 로 활성화 (점프 입력 시 판정)
	FPTAbilitySet_GameplayAbility TraversalAbility;
	TraversalAbility.Ability = UPTGameplayAbility_Traversal::StaticClass();
	DefaultAbilities.Add(TraversalAbility);
}

void APTCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 메시가 아직 지정되지 않은 경우(에셋 연결 전) 임시 마네킹을 사용, 그것도 없으면 캡슐 표시
	auto PackageExists = [](const FSoftObjectPath& Path)
	{
		return !Path.IsNull() && FPackageName::DoesPackageExist(Path.GetLongPackageName());
	};

	// 1순위: GASP UEFN 마네킹 + 모션 매칭 AnimBP
	if (GetMesh() && !GetMesh()->GetSkeletalMeshAsset()
		&& PackageExists(MotionMatchingMesh.ToSoftObjectPath()) && PackageExists(MotionMatchingAnimClass.ToSoftObjectPath()))
	{
		USkeletalMesh* MMMesh = MotionMatchingMesh.LoadSynchronous();
		UClass* MMAnimClass = MotionMatchingAnimClass.LoadSynchronous();
		if (MMMesh && MMAnimClass)
		{
			GetMesh()->SetSkeletalMeshAsset(MMMesh);
			GetMesh()->SetAnimInstanceClass(MMAnimClass);
			UE_LOG(LogPT, Log, TEXT("%s: using motion matching character %s / %s"), *GetName(), *GetNameSafe(MMMesh), *GetNameSafe(MMAnimClass));
		}
	}

	if (GetMesh() && !GetMesh()->GetSkeletalMeshAsset())
	{
		USkeletalMesh* LoadedMesh = PlaceholderMesh.IsNull() ? nullptr : PlaceholderMesh.LoadSynchronous();
		if (LoadedMesh)
		{
			GetMesh()->SetSkeletalMeshAsset(LoadedMesh);
			for (const TSoftClassPtr<UAnimInstance>& Candidate : PlaceholderAnimClasses)
			{
				if (Candidate.IsNull() || !FPackageName::DoesPackageExist(Candidate.ToSoftObjectPath().GetLongPackageName()))
				{
					continue;
				}
				if (UClass* AnimClass = Candidate.LoadSynchronous())
				{
					GetMesh()->SetAnimInstanceClass(AnimClass);
					break;
				}
			}
			UE_LOG(LogPT, Log, TEXT("%s: using placeholder mesh %s"), *GetName(), *GetNameSafe(LoadedMesh));
		}
		else
		{
			GetCapsuleComponent()->SetHiddenInGame(false);
			UE_LOG(LogPT, Warning, TEXT("%s has no skeletal mesh - showing capsule. Create a BP child of PTCharacter and assign a mesh/AnimBP."), *GetName());
		}
	}
}

UAbilitySystemComponent* APTCharacter::GetAbilitySystemComponent() const
{
	return GetPTAbilitySystemComponent();
}

UPTAbilitySystemComponent* APTCharacter::GetPTAbilitySystemComponent() const
{
	const APTPlayerState* PS = GetPlayerState<APTPlayerState>();
	return PS ? PS->GetPTAbilitySystemComponent() : nullptr;
}

UPTCharacterMovementComponent* APTCharacter::GetPTMovementComponent() const
{
	return CastChecked<UPTCharacterMovementComponent>(GetCharacterMovement(), ECastCheckedType::NullAllowed);
}

EPTGait APTCharacter::GetGait() const
{
	const UPTCharacterMovementComponent* MoveComp = GetPTMovementComponent();
	return MoveComp ? MoveComp->GetGait() : EPTGait::Run;
}

EPTStance APTCharacter::GetStance() const
{
	const UPTCharacterMovementComponent* MoveComp = GetPTMovementComponent();
	return MoveComp ? MoveComp->GetStance() : EPTStance::Stand;
}

EPTRotationMode APTCharacter::GetRotationMode() const
{
	const UPTCharacterMovementComponent* MoveComp = GetPTMovementComponent();
	return MoveComp ? MoveComp->GetRotationMode() : EPTRotationMode::OrientToMovement;
}

// ---------------------------------------------------------------------------------------------
// Ability System
// ---------------------------------------------------------------------------------------------

void APTCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버: ASC 초기화 + 어빌리티 부여
	if (APTPlayerState* PS = GetPlayerState<APTPlayerState>())
	{
		PS->GrantAbilitiesFromPawn(this);
		PS->ResetForRespawn();
	}
	InitAbilityActorInfo();
}

void APTCharacter::UnPossessed()
{
	if (UPTAbilitySystemComponent* ASC = GetPTAbilitySystemComponent())
	{
		ASC->ClearAbilityInput();
	}
	Super::UnPossessed();
}

void APTCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트: ASC 아바타 설정
	InitAbilityActorInfo();
}

void APTCharacter::InitAbilityActorInfo()
{
	APTPlayerState* PS = GetPlayerState<APTPlayerState>();
	UPTAbilitySystemComponent* ASC = PS ? PS->GetPTAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	ASC->InitAbilityActorInfo(PS, this);

	if (HasAuthority())
	{
		if (const UPTHealthSet* HealthSet = ASC->GetSet<UPTHealthSet>())
		{
			HealthSet->OnOutOfHealth.Remove(OutOfHealthHandle);
			OutOfHealthHandle = HealthSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
		}
	}
}

void APTCharacter::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float OldValue, float NewValue)
{
	if (UPTAbilitySystemComponent* ASC = GetPTAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(PTGameplayTags::Status_Dead, 1, EGameplayTagReplicationState::TagOnly);
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(PTGameplayTags::Ability_Movement_Sprint);
		ASC->CancelAbilitiesWithTag(CancelTags);
	}
	OnOutOfHealth();
}

void APTCharacter::OnOutOfHealth_Implementation()
{
	// 기본 동작: 이동 정지. 사망 연출/리스폰은 BP 또는 GameMode 에서 확장하세요.
	if (UPTCharacterMovementComponent* MoveComp = GetPTMovementComponent())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
}

void APTCharacter::SetLooseStatusTag(const FGameplayTag& Tag, bool bEnabled)
{
	if (UPTAbilitySystemComponent* ASC = GetPTAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(Tag, bEnabled ? 1 : 0);
	}
}

void APTCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
	SetLooseStatusTag(PTGameplayTags::Status_InAir, GetCharacterMovement() && GetCharacterMovement()->IsFalling());
}

void APTCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	SetLooseStatusTag(PTGameplayTags::Status_Crouching, true);
}

void APTCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	SetLooseStatusTag(PTGameplayTags::Status_Crouching, false);
}

FRotator APTCharacter::GetAimRotation() const
{
	if (const AController* C = GetController())
	{
		// 로컬 컨트롤러 또는 서버(모든 플레이어의 컨트롤러 보유)
		return C->GetControlRotation();
	}

	// 시뮬레이티드 프록시
	FRotator Aim = GetBaseAimRotation(); // Pitch = RemoteViewPitch
	Aim.Yaw = FRotator::DecompressAxisFromShort(ReplicatedAimYaw);
	Aim.Roll = 0.f;
	return Aim;
}

void APTCharacter::TeleportForMigrationRestore(const FTransform& InTransform, const FRotator& InControlRotation)
{
	if (!HasAuthority())
	{
		return;
	}

	TeleportTo(InTransform.GetLocation(), InTransform.Rotator(), false, true);
	if (AController* C = GetController())
	{
		C->SetControlRotation(InControlRotation);
		if (APlayerController* PC = Cast<APlayerController>(C))
		{
			PC->ClientSetRotation(InControlRotation, true);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Replication
// ---------------------------------------------------------------------------------------------

void APTCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, ReplicatedAcceleration, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION(ThisClass, ReplicatedIntentFlags, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION(ThisClass, ReplicatedAimYaw, COND_SimulatedOnly);
}

void APTCharacter::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	if (UPTCharacterMovementComponent* MoveComp = GetPTMovementComponent())
	{
		// 가속도 압축 (Lyra 방식)
		const double MaxAccel = MoveComp->MaxAcceleration > 0.f ? MoveComp->MaxAcceleration : 1.0;
		const FVector CurrentAccel = MoveComp->GetCurrentAcceleration();
		double AccelXYRadians, AccelXYMagnitude;
		FMath::CartesianToPolar(CurrentAccel.X, CurrentAccel.Y, AccelXYMagnitude, AccelXYRadians);

		ReplicatedAcceleration.AccelXYRadians = static_cast<uint8>(FMath::FloorToInt((AccelXYRadians / TWO_PI) * 255.0) & 0xFF); // 음수 각도는 mod 256 으로 감김
		ReplicatedAcceleration.AccelXYMagnitude = (uint8)FMath::FloorToInt(FMath::Clamp(AccelXYMagnitude / MaxAccel, 0.0, 1.0) * 255.0);
		ReplicatedAcceleration.AccelZ = (int8)FMath::FloorToInt(FMath::Clamp(CurrentAccel.Z / MaxAccel, -1.0, 1.0) * 127.0);

		ReplicatedIntentFlags = MoveComp->GetIntentFlags();
	}

	if (const AController* C = GetController())
	{
		ReplicatedAimYaw = FRotator::CompressAxisToShort(C->GetControlRotation().Yaw);
	}
}

void APTCharacter::OnRep_ReplicatedAcceleration()
{
	if (UPTCharacterMovementComponent* MoveComp = GetPTMovementComponent())
	{
		const double MaxAccel = MoveComp->MaxAcceleration > 0.f ? MoveComp->MaxAcceleration : 1.0;
		const double AccelXYMagnitude = double(ReplicatedAcceleration.AccelXYMagnitude) * MaxAccel / 255.0;
		const double AccelXYRadians = double(ReplicatedAcceleration.AccelXYRadians) * TWO_PI / 255.0;

		FVector UnpackedAcceleration(FVector::ZeroVector);
		FMath::PolarToCartesian(AccelXYMagnitude, AccelXYRadians, UnpackedAcceleration.X, UnpackedAcceleration.Y);
		UnpackedAcceleration.Z = double(ReplicatedAcceleration.AccelZ) * MaxAccel / 127.0;

		MoveComp->SetReplicatedAcceleration(UnpackedAcceleration);
	}
}

void APTCharacter::OnRep_ReplicatedIntentFlags()
{
	if (UPTCharacterMovementComponent* MoveComp = GetPTMovementComponent())
	{
		MoveComp->SetReplicatedIntentFlags(ReplicatedIntentFlags);
	}
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

void APTCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	AddInputMappingContext();
}

void APTCharacter::AddInputMappingContext()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	if (!InputConfig && !RuntimeInputConfig)
	{
		UInputMappingContext* IMC = nullptr;
		RuntimeInputConfig = UPTInputConfig::CreateDefaultRuntimeConfig(this, IMC);
		RuntimeMappingContext = IMC;
	}

	if (UInputMappingContext* IMC = DefaultMappingContext ? DefaultMappingContext.Get() : RuntimeMappingContext.Get())
	{
		Subsystem->RemoveMappingContext(IMC);
		Subsystem->AddMappingContext(IMC, MappingContextPriority);
	}
}

void APTCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UPTInputComponent* PTInput = Cast<UPTInputComponent>(PlayerInputComponent);
	if (!PTInput)
	{
		UE_LOG(LogPT, Error, TEXT("PTCharacter: InputComponent is not UPTInputComponent. Check DefaultInput.ini DefaultInputComponentClass."));
		return;
	}

	if (!InputConfig && !RuntimeInputConfig)
	{
		UInputMappingContext* IMC = nullptr;
		RuntimeInputConfig = UPTInputConfig::CreateDefaultRuntimeConfig(this, IMC);
		RuntimeMappingContext = IMC;
	}

	const UPTInputConfig* Config = InputConfig ? InputConfig.Get() : RuntimeInputConfig.Get();
	check(Config);

	PTInput->RemoveBinds(AbilityInputBindHandles);
	PTInput->BindAbilityActions(Config, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, AbilityInputBindHandles);

	PTInput->BindNativeAction(Config, PTGameplayTags::InputTag_Move,   ETriggerEvent::Triggered, this, &ThisClass::Input_Move, true);
	PTInput->BindNativeAction(Config, PTGameplayTags::InputTag_Look,   ETriggerEvent::Triggered, this, &ThisClass::Input_Look, true);
	PTInput->BindNativeAction(Config, PTGameplayTags::InputTag_Crouch, ETriggerEvent::Started,   this, &ThisClass::Input_Crouch, false);
	PTInput->BindNativeAction(Config, PTGameplayTags::InputTag_Walk,   ETriggerEvent::Started,   this, &ThisClass::Input_ToggleWalk, false);
	PTInput->BindNativeAction(Config, PTGameplayTags::InputTag_Strafe, ETriggerEvent::Started,   this, &ThisClass::Input_ToggleStrafe, false);

	AddInputMappingContext();
}

void APTCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D MoveValue = Value.Get<FVector2D>();
	if (!Controller || MoveValue.IsNearlyZero())
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, MoveValue.Y);
	AddMovementInput(Right, MoveValue.X);
}

void APTCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D LookValue = Value.Get<FVector2D>();
	AddControllerYawInput(LookValue.X);
	AddControllerPitchInput(-LookValue.Y);
}

void APTCharacter::Input_Crouch(const FInputActionValue& Value)
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else if (GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround())
	{
		Crouch();
	}
}

void APTCharacter::Input_ToggleWalk(const FInputActionValue& Value)
{
	if (UPTCharacterMovementComponent* MoveComp = GetPTMovementComponent())
	{
		MoveComp->SetWantsToWalk(!MoveComp->WantsToWalk());
	}
}

void APTCharacter::Input_ToggleStrafe(const FInputActionValue& Value)
{
	if (UPTCharacterMovementComponent* MoveComp = GetPTMovementComponent())
	{
		MoveComp->SetWantsToStrafe(!MoveComp->WantsToStrafe());
	}
}

void APTCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	// 점프 입력: 앞에 넘을/올라탈 장애물이 있으면 파쿠르, 없으면 일반 점프
	if (InputTag == PTGameplayTags::InputTag_Jump && Traversal && Traversal->TryTraversal())
	{
		return;
	}

	if (UPTAbilitySystemComponent* ASC = GetPTAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void APTCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (UPTAbilitySystemComponent* ASC = GetPTAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}
