# PT

리슨 서버 기반 네트워크 협동 게임 프로젝트 (Unreal Engine 5.8 소스 빌드).

| 기능 | 위치 | 문서 |
|---|---|---|
| Gameplay Ability System 기반 | `Source/PT/*/AbilitySystem`, `Input` | [Docs/GAS.md](Docs/GAS.md) |
| C++ 모션 매칭 이동 | `Character`, `Animation` | [Docs/MotionMatching.md](Docs/MotionMatching.md) |
| 파쿠르 (Hurdle/Vault/Mantle, GAS 어빌리티) | `Traversal`, `AbilitySystem/Abilities/PTGameplayAbility_Traversal` | [Docs/Traversal.md](Docs/Traversal.md) |
| 전력질주 / 정지 슬라이드 | `Character/PTCharacterMovementComponent`, `Animation/PTMotionMatchingAnimInstance` | [Docs/Sprint.md](Docs/Sprint.md) |
| GASP 연동 (모션 매칭 로코모션 + 파쿠르 몽타주) | `PTEditor/PTGASPSetup` (`pt.Editor.SetupGASP`) | [Docs/GASP.md](Docs/GASP.md) |
| 발 IK (경사면 / 계단) | `Animation/AnimNode_PTFootIK`, `PTMotionMatchingAnimInstance::UpdateFootIK` | [Docs/FootIK.md](Docs/FootIK.md) |
| C++ 절차적 제자리 회전 | `CharacterMovement`, `Animation/AnimNode_PTTurnInPlace` | [Docs/TurnInPlace.md](Docs/TurnInPlace.md) |
| C++ 절차적 에임 오프셋 | `Animation/AnimNode_PTAimOffset`, `Source/PTEditor` | [Docs/AimOffset.md](Docs/AimOffset.md) |
| 리슨 서버 세션 + 호스트 인계 | `Online`, `Core` | [Docs/HostMigration.md](Docs/HostMigration.md) |
| 최초 실행/콘텐츠 변경 시 셰이더(PSO) 컴파일 | `Shader`, `UI` | [Docs/ShaderWarmup.md](Docs/ShaderWarmup.md) |

## 빌드

프로젝트는 엔진 루트(`E:\UnrealEngine-5.8\PT`)에 있으므로 `Default.uprojectdirs` 에 의해 엔진 솔루션에 함께 포함됩니다.

1. `E:\UnrealEngine-5.8\GenerateProjectFiles.bat` 실행
2. `UE5.sln` 열기 → 솔루션 탐색기에서 **Games/PT** 를 시작 프로젝트로 설정
3. 구성 `Development Editor` / `Win64` 로 빌드 → 실행 (`PTEditor`)

> 첫 빌드는 PoseSearch / GameplayAbilities / OnlineSubsystemSteam 등 플러그인이 함께 컴파일됩니다.

## 소스 구조

```
Source/PT
├─ PTGameplayTags        네이티브 게임플레이 태그 (InputTag.*, Status.*, Ability.* ...)
├─ AbilitySystem         ASC, GameplayAbility 베이스, AbilitySet, Health/Stamina AttributeSet, Jump/Sprint 어빌리티
├─ Input                 InputConfig(InputAction↔Tag), 태그 바인딩 InputComponent, 에셋 없는 기본 키 배치
├─ Character             PTCharacter, PTCharacterMovementComponent(Gait/Stance/Strafe 예측), 로코모션 타입
├─ Animation             PTMotionMatchingAnimInstance(C++ 궤적/DB 선택), PTLocomotionDatabaseSet(Chooser 대체)
├─ Core                  GameMode / GameState / PlayerState(ASC 소유) / PlayerController / GameInstance
├─ Online                PTSessionSubsystem, PTHostMigrationSubsystem, 스냅샷 타입/라이브러리, Migratable 인터페이스
├─ Shader                PTShaderWarmupSubsystem, 설정
└─ UI                    SPTStatusOverlay (에셋 없이 동작하는 진행 화면)

Source/PTEditor           에디터 전용(UncookedOnly): PT Aim Offset 애님 그래프 노드
```

## 빠른 테스트 (에디터)

1. 기본 테스트 맵: `/Game/ThirdPerson/Lvl_ThirdPerson` (3인칭 템플릿 맵 - 경사로/계단/큐브, 에디터·게임 시작 맵으로 지정됨)
2. 에디터 Play 옵션: **Number of Players = 2~3**, **Net Mode = Play Standalone** (각 인스턴스가 독립 프로세스처럼 동작)
3. 인스턴스 1 콘솔(`~`): `pt.Host /Game/ThirdPerson/Lvl_ThirdPerson 4`
4. 인스턴스 2,3 콘솔: `pt.Find` → `pt.Join 0`
5. 호스트에서 `pt.Leave` → 다른 인스턴스 중 한 명이 새 호스트가 되고 나머지가 자동 재접속
   - 상태 확인: `pt.Migration.Status`
   - 호스트 강제 종료(창 닫기)도 `ConnectionTimeout`(기본 10초) 후 동일하게 인계됩니다.

> 별도 프로세스 테스트: `UnrealEditor.exe PT.uproject -game -log` 를 여러 개 실행해도 됩니다 (LAN/Null 서브시스템).

## 콘솔 명령

| 명령 | 설명 |
|---|---|
| `pt.Host [MapPath] [MaxPlayers]` | 세션 생성 후 맵을 listen 으로 엶 |
| `pt.Find` / `pt.Join <i>` | 세션 검색 / 참가 |
| `pt.Leave` | 나가기 (호스트면 인계) |
| `pt.EndForAll` | 호스트: 인계 없이 모두 종료 |
| `pt.Migration.Status` / `pt.Migration.Force` | 인계 정보 출력 / (디버그) 연결 끊어 인계 흐름 테스트 |
| `pt.ShaderWarmup.Run` | 셰이더 워밍업 화면 강제 실행 |
| `pt.MotionMatching.DrawTrajectory 1` | 모션 매칭 궤적 디버그 표시 |
| `pt.MotionMatching.LogDatabaseChanges 1` | 검색 조건 변경 로그 |

## 다음 할 일 (에셋 작업)

- [x] GASP(Game Animation Sample) 콘텐츠 마이그레이션 → `Docs/GASP.md` (에디터에서 `pt.Editor.SetupGASP` 1회 실행)
- [ ] `BP_PTCharacter`(부모 PTCharacter) + `ABP_PTMotionMatching`(부모 PTMotionMatchingAnimInstance)
- [ ] `DA_LocomotionDatabases`(PTLocomotionDatabaseSet) 규칙 작성
- [ ] 메인 메뉴 맵 / UI (Host·Find·Join 은 `PTSessionSubsystem` BP 노드 사용) → `DefaultEngine.ini` 의 `GameDefaultMap` 교체
- [ ] 패키징 전 PSO 캐시 수집 → `Docs/ShaderWarmup.md`

## GitHub 업로드

`GitPush.bat` 더블클릭 (또는 `GitPush.bat 커밋 메시지`) → 변경 사항 커밋 후 https://github.com/yooms/PT 로 푸시. 에셋은 Git LFS. 푸시가 거부되면 합치기/덮어쓰기 중 선택.
