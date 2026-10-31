# C++ 모션 매칭 이동

## 구성 요소

| 클래스 | 역할 |
|---|---|
| `UPTCharacterMovementComponent` | Gait(Walk/Run/Sprint), Stance(Stand/Crouch), RotationMode(OrientToMovement/Strafe)를 **SavedMove 플래그**(`FLAG_Custom_0~2`)로 클라이언트 예측 + 서버 권한 처리. 방향별 최대 속도·가속·감속·마찰을 Gait 별로 적용 |
| `APTCharacter` | 입력 → 무브먼트 의도 설정. 시뮬레이티드 프록시용 **가속도/의도 복제**(Lyra 방식 압축) → 원격 캐릭터도 미래 궤적 예측이 정확함 |
| `UPTMotionMatchingAnimInstance` | 게임 스레드에서 상태 수집 → 워커 스레드에서 궤적(`FTransformTrajectory`) 생성, 이동 상태 판정, 검색 조건 계산 → MM 노드 콜백에서 데이터베이스 선택/인터럽트/블렌드 시간 결정 |
| `UPTLocomotionDatabaseSet` | 검색 조건 → PoseSearch Database 규칙 (GASP 의 Chooser Table 을 C++ 데이터로 대체) |

### 프레임 흐름

```
[GT]     NativeUpdateAnimation        : 위치/속도/가속/Gait/Stance/RotationMode/낙하 여부 수집
[Worker] NativeThreadSafeUpdate       : PoseSearchGenerateTransformTrajectory (CMC 이동 공식 시뮬레이션)
                                        FutureVelocity, Moving/Idle, InAir/OnGround, 착지 판정 → FPTLocomotionQuery
[Worker] MM 노드 OnUpdate             : UpdateMotionMatching()
                                        DatabaseSet 규칙으로 DB 목록 → SetDatabasesToSearch
                                        조건이 바뀌면 InterruptOnDatabaseChange, 아니면 DoNotInterrupt
                                        (블렌드 시간은 MotionMatchingBlendTime 변수 → 노드 핀 바인딩)
```

## GASP 에셋 연결 절차

1. **GASP 콘텐츠 가져오기**: Fab/런처에서 *Game Animation Sample* 을 5.8 용으로 받아 별도 프로젝트로 생성 → `Content/Characters`, `Content/Blueprints/Data` 등 필요한 폴더를 PT 로 **Migrate**. (Migrate 시 요구하는 플러그인은 `PT.uproject` 에 추가하세요. 기본으로 PoseSearch / AnimationWarping / AnimationLocomotionLibrary / BlendStack / Chooser / MotionWarping 이 켜져 있습니다.)
2. **캐릭터 BP**: `BP_PTCharacter` 생성 (부모: `PTCharacter`)
   - Mesh: GASP 캐릭터(예: UEFN Mannequin) 지정, Anim Class: 아래 ABP
   - 이동 수치: `CharacterMovement > PT|Gait` 에서 GASP 애니메이션 속도에 맞게 조정 (기본값은 GASP 근사치)
3. **데이터베이스 세트**: `DA_LocomotionDatabases` 생성 (클래스: `PTLocomotionDatabaseSet`)
   - 규칙 예시 (위에서부터 평가, 맞는 규칙의 DB 를 합쳐 검색):

| RuleName | 필터 | Databases (GASP 예시 이름) | Stop |
|---|---|---|---|
| InAir | Situations=InAir | Jumps / Falling 계열 DB | ✔ |
| CrouchIdle | Stances=Crouch, MovementStates=Idle | Crouch Idles / Stops | ✔ |
| CrouchMove | Stances=Crouch, MovementStates=Moving | Crouch Starts/Loops/Pivots | ✔ |
| StandIdle | MovementStates=Idle | Stand Idles / Stops / Turn-in-place | ✔ |
| Walk | Gaits=Walk | Walk Starts/Loops/Pivots/Stops | ✔ |
| Run | Gaits=Run | Run Starts/Loops/Pivots/Stops | ✔ |
| Sprint | Gaits=Sprint | Sprint 계열 + Run Pivots | ✔ |

   - 스트레이프 전용 DB 가 있다면 `RotationModes=Strafe` 규칙을 위쪽에 추가
4. **AnimBP**: `ABP_PTMotionMatching` 생성 (부모 클래스: `PTMotionMatchingAnimInstance`, 스켈레톤: GASP 캐릭터)
   - Class Defaults: `Database Set = DA_LocomotionDatabases`, 궤적 파라미터(Idle/Moving) 확인
   - AnimGraph:
     ```
     Motion Matching ──► Pose History ──► (Offset Root / Steering 등 선택)
       ──► Local To Component ──► PT Aim Offset ──► Component To Local ──► Output Pose
     (에임 오프셋 연결은 Docs/AimOffset.md)
     ```
     - **Pose History** 노드: `Trajectory` 핀을 표시하고 변수 `Trajectory` 연결 (Generate Trajectory 옵션은 끔 — C++ 에서 생성)
     - **Motion Matching** 노드 디테일 > Functions:
       - `On Update` = `UpdateMotionMatching`
       - (선택) `On Update` 다음에 결과가 필요하면 별도 노드 함수로 `UpdateMotionMatchingPostSelection`
       - Database 는 비워둬도 됩니다 (C++ 이 매 프레임 지정)
       - **Blend Time** 핀을 표시하고 변수 `MotionMatchingBlendTime` 연결 (지면/공중/착지별 블렌드 시간)
     - 엔진 버전에 따라 부모 C++ 함수가 함수 목록에 보이지 않으면, AnimBP 안에 **Thread Safe** 함수를 만들고 그 안에서 `UpdateMotionMatching(Context, Node)` 를 호출해 바인딩하세요.
   - GASP 처럼 Offset Root / Orientation Warping / Steering 노드를 쓰려면 `CurrentDatabaseHasTag()`, `Query`, `FutureVelocity` 등을 사용하세요.
5. **GameMode**: `BP_PTGameMode`(부모 `PTGameMode`)의 Default Pawn 을 `BP_PTCharacter` 로 → 월드 세팅 또는 `GlobalDefaultGameMode` 에 지정

## 입력 (기본 키 배치)

`InputConfig` 를 비워두면 런타임에 기본 입력이 생성됩니다.

| 동작 | 키보드/마우스 | 게임패드 | 처리 |
|---|---|---|---|
| 이동 / 시점 | WASD / 마우스 | 좌/우 스틱 | 네이티브 |
| 점프 | Space | A | `UPTGameplayAbility_Jump` |
| 스프린트(누르는 동안) | Left Shift | 좌스틱 클릭 | `UPTGameplayAbility_Sprint` |
| 앉기 토글 | C | B | 네이티브 |
| 걷기 토글 | Left Ctrl | — | 네이티브 (아날로그 입력 50% 미만도 자동 Walk) |
| 스트레이프 토글 | Tab | — | 네이티브 |

## 디버그

- `pt.MotionMatching.DrawTrajectory 1` — 궤적 표시
- `pt.MotionMatching.LogDatabaseChanges 1` — 검색 조건 변경 로그
- Rewind Debugger 의 Pose Search 트랙으로 선택된 포즈/코스트 확인

## 네트워크 메모

- 로컬/서버: SavedMove 플래그로 Gait 가 예측되므로 스프린트 시작/정지 시 보정(correction)이 발생하지 않습니다.
- 시뮬레이티드 프록시: 서버가 확정한 Gait 와 압축 가속도를 `COND_SimulatedOnly` 로 복제 → `SimulateMovement` 중에도 가속도가 유지되어 원격 캐릭터 궤적이 로컬과 비슷하게 생성됩니다.
