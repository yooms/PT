# Game Animation Sample(GASP) 연동

`E:\GameAnimationSample` 에서 **필요한 에셋만** PT 로 이주해 C++ 모션 매칭 로코모션 + 파쿠르를 구성했습니다.

## 이주한 에셋 (약 1,020개 파일 / 1.05GB, 원래 경로 그대로)

| 경로 | 내용 |
|---|---|
| `Characters/UEFN_Mannequin/Meshes, Materials, Textures, Rigs` | UEFN 마네킹 메시·스켈레톤·포스트 프로세스 ABP |
| `Characters/UEFN_Mannequin/Animations/{Idle,Walk,Run,Sprint,Jump,Crouch,AimOffset,Poses}` | 모션 매칭 데이터베이스가 사용하는 Neutral 애니메이션 |
| `Characters/UEFN_Mannequin/Animations/MotionMatchingData` | Dense / Sparse 데이터베이스, 스키마, 채널, PSD_Traversal |
| `Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData` | 애니메이션이 참조하는 DB 6개 (참조 누락 경고 방지용) |
| `Characters/UEFN_Mannequin/Animations/Traversal` | Hurdle / Vault / Mantle / Climb / Catch 의 Neutral 몽타주 + 시퀀스 |
| `Blueprints/AnimNotifies, Blueprints/Data, Audio/Foley, Misc` | 몽타주/애니메이션의 노티파이(발소리 등)와 그 의존 에셋 |

**제외**: Relaxed 스타일, Extreme Sparse / Relaxed DB, Ragdoll, Chooser 테이블(C++ 규칙으로 대체), Mover, SandboxCharacter BP, 레벨, MetaHuman 등.
`BP_NotifyState_EarlyTransition`(GASP 전용 ABP 참조)도 제외 → 일부 피벗 애니메이션 10개에 "노티파이 클래스 없음" 경고가 날 수 있지만 동작에는 영향 없습니다.

## 설정 (에디터에서 1회)

에디터 콘솔(`~`):

```
pt.Editor.SetupGASP
```

| 생성 에셋 | 설명 |
|---|---|
| `/Game/PT/Animation/DA_PTLocomotionDatabases` | 이동 상태 → 데이터베이스 규칙 (GASP Chooser 대체, 아래 표) |
| `/Game/PT/Traversal/DA_PTTraversalAnimSet` | 파쿠르 몽타주 후보 30여 개 (높이 / 속도 / 공중 조건) |
| `/Game/PT/Animation/ABP_PTMotionMatching` | UEFN 스켈레톤 AnimBP: Motion Matching → Pose History("PoseHistory") → Slot(DefaultSlot) → PT Turn In Place → PT Aim Offset → Output |

- 데이터 에셋은 이미 있으면 그대로 둡니다 (직접 수정 보존). 기본값으로 다시 만들려면 `pt.Editor.SetupGASP rebuild`
- `PTCharacter` 는 메시가 비어 있으면 **SKM_UEFN_Mannequin + ABP_PTMotionMatching** 을 자동 사용 (없으면 이전 Quinn + ABP_PTUnarmed)
- Traversal 컴포넌트는 `DA_PTTraversalAnimSet` 을 자동 로드

## 데이터베이스 규칙 (위에서부터, 처음 맞는 규칙 사용)

| 규칙 | 조건 | 데이터베이스 |
|---|---|---|
| InAir | 공중 | Dense Jumps / Jumps_Far / Jumps_FromTraversal |
| FromTraversal_* | 파쿠르 끝난 뒤 0.5초 | Walk/Run_FromTraversal + 이어질 Loops/Starts/Stops |
| Land(Light/Heavy)_* | 착지 후 0.3초, 하강 속도 700 기준 | Idle/Walk/Run/Sprint Lands Light/Heavy |
| Crouch_Idle / Crouch_Move | 앉기 | Crouch Idles, Walk Stops / Loops, Starts, Pivots |
| Stand_Idle | 정지 | Stand Idles + Walk/Run/Sprint Stops |
| Walk / Run / Sprint | 이동 Gait | Sparse Loops / Starts / Pivots (+ SpinTransition) |

제자리 회전은 PT Turn In Place 노드가 담당하므로 TurnInPlace DB 는 넣지 않았습니다.
모션 매칭 DB 를 쓰면 템플릿용 보정(질주 애니 속도 스케일, 몸 기울기, MM_Land 슬라이드 몽타주)은 자동으로 꺼지고 GASP 애니메이션이 대신합니다.

## 디버그

- `pt.MotionMatching.LogDatabaseChanges 1` - 상태 변경 시 선택된 DB 수 로그
- `pt.MotionMatching.DrawTrajectory 1` - 궤적
- `pt.Traversal.Debug 1` - 파쿠르 판정
- Rewind Debugger → Pose Search 트랙
