# Gameplay Ability System 기반

## 구조

```
APTPlayerState ── UPTAbilitySystemComponent (Owner = PlayerState, Avatar = Pawn, Mixed 복제)
                 ├─ UPTHealthSet  : Health, MaxHealth (복제) / Damage, Healing (메타)
                 └─ UPTStaminaSet : Stamina, MaxStamina, StaminaRegenRate
APTCharacter   ── PossessedBy(서버)  : AbilitySet + DefaultAbilities 부여(1회), ASC InitAbilityActorInfo
                └─ OnRep_PlayerState(클라): ASC InitAbilityActorInfo
APTPlayerController::PostProcessInput → ASC::ProcessAbilityInput (입력 태그 → 어빌리티 활성화)
```

ASC 를 PlayerState 에 두었기 때문에 리스폰해도 어트리뷰트/어빌리티가 유지되고, 호스트 인계 시 플레이어 단위로 저장/복원하기 쉽습니다.

## 클래스

| 클래스 | 설명 |
|---|---|
| `UPTAbilitySystemComponent` | 입력 태그 처리(Pressed/Held/Released), OnSpawn 어빌리티, 태그로 취소 |
| `UPTGameplayAbility` | 기본 `InstancedPerActor` + `LocalPredicted`. `ActivationPolicy` = OnInputTriggered / WhileInputActive / OnSpawn. `Status.Dead` 상태면 활성화 차단 |
| `UPTAbilitySet` (DataAsset) | 어빌리티(+InputTag)·이펙트·어트리뷰트셋 묶음 → 캐릭터 BP 의 `AbilitySets` 에 지정 |
| `UPTInputConfig` (DataAsset) | InputAction ↔ InputTag. 비워두면 기본 키 배치 런타임 생성 |
| `UPTGameplayAbility_Jump` | 점프 (CMC 점프 예측 그대로 사용) |
| `UPTGameplayAbility_Sprint` | 누르는 동안 스프린트, 선택적 스태미나 소모 (`StaminaCostPerSecond`) |
| `PTGameplayTags` | 네이티브 태그 (`InputTag.*`, `Ability.*`, `Status.*`, `SetByCaller.*`, `Event.*`) |

## 새 어빌리티 추가하기

1. C++ 또는 BP 로 `PTGameplayAbility` 상속 (예: `GA_Interact`)
   - Activation Policy 선택, Ability Tags / Blocked Tags 설정
   - `ActivateAbility` 에서 `CommitAbility` → 로직 → `EndAbility`
2. 입력 태그 추가: `PTGameplayTags.h/.cpp` 에 `InputTag_Interact` 선언/정의 (또는 프로젝트 설정 태그)
3. InputAction 에셋 `IA_Interact` 생성 → `DA_InputConfig` 의 **AbilityInputActions** 에 (IA_Interact, InputTag.Interact) 추가
   - 커스텀 InputConfig/MappingContext 를 쓰면 캐릭터 BP 의 `InputConfig`, `DefaultMappingContext` 에 지정
4. `DA_AbilitySet_Default` (PTAbilitySet) 에 (GA_Interact, Level 1, InputTag.Interact) 추가 → 캐릭터 BP `AbilitySets` 에 등록

## 데미지/회복

- 데미지 GE: Modifier 로 `PTHealthSet.Damage` 에 Additive (SetByCaller `SetByCaller.Damage` 권장)
- `Health` 가 0 이 되면 서버에서 `OnOutOfHealth` → 캐릭터에 `Status.Dead` 태그(복제) + `Event.Death` 게임플레이 이벤트
- 리스폰 시 `Status.Dead` 제거 및 체력 회복 (`APTPlayerState::ResetForRespawn`)

## 디버그

- `showdebug abilitysystem` — ASC 상태
- 로그 카테고리 `LogPTAbility`
