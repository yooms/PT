# C++ 절차적 제자리 회전 (Turn In Place)

회전 애니메이션 에셋 없이, 정지 상태에서 카메라를 돌리면 **발을 번갈아 디디며** 몸 방향을 바꿉니다.

## 구성

| 위치 | 역할 |
|---|---|
| `UPTCharacterMovementComponent::PhysicsRotation` | 정지 + 지면일 때 카메라와 몸 각도 차이가 `TurnInPlaceStartAngle`(70°)를 넘으면 캡슐을 `TurnInPlaceRate`(300°/s)로 회전, `TurnInPlaceStopAngle`(3°) 안에 들어오면 정지. 이동 예측 경로 안에서 처리되므로 서버/클라이언트 동기화 |
| `UPTMotionMatchingAnimInstance::UpdateTurnInPlace` | 몸(메시)은 캡슐 방향을 `TurnBodyFollowSpeed` 로 부드럽게 따라가고, 발은 디딘 자리에 고정된 채 다리가 살짝 꼬임 → 발 방향과 몸 방향 차이가 `TurnStepTriggerAngle`(35°)를 넘으면 그 발만 스텝 (항상 돌아가는 방향 쪽 발이 먼저, 반대 발이 바로 뒤따름). 회전이 멈춘 뒤 발이 `TurnSettleAngle`(12°) 이상 틀어져 있으면 정리 스텝. 한 번에 한 발, 발당 `TurnStepDuration`(0.28초) |
| `FAnimNode_PTTurnInPlace` | 루트 본을 오프셋만큼 회전, 디딘 발은 월드 위치·방향 고정, 스텝 중인 발은 호를 그리며 이동(높이 `StepHeight` 5cm), 골반을 살짝 낮춤, Two Bone IK 로 다리 해결 |
| `UAnimGraphNode_PTTurnInPlace` (PTEditor) | AnimBP 그래프의 **"PT Turn In Place (Procedural)"** 노드 |

- 이동을 시작하면 발 고정이 풀리고 메시 오프셋은 부드럽게 0 으로 돌아갑니다.
- 에임 오프셋은 메시 정면 기준으로 자동 보정됩니다 (`ForwardYawOffset` ← `TurnRootYawOffset`).
- 스트레이프 모드에서도 정지 중에는 같은 규칙으로 돕니다 (이동 중에는 기존처럼 카메라 방향으로 연속 회전).

## AnimBP 설정

에디터 콘솔(`~`):

```
pt.Editor.SetupLocomotionABP
```

- `/Game/PT/Animation/ABP_PTUnarmed` 가 **없으면** ABP_Unarmed 를 복제해 새로 만들고, **있으면** 그 ABP 에 빠진 노드만 추가합니다.
- 결과 체인: `(기존 포즈) → Local To Component → PT Turn In Place → PT Aim Offset → Component To Local → Output Pose`
- 연결되는 변수: `TurnRootYawOffset, TurnStepFoot, TurnStepAlpha, TurnStepEndYawOffset, TurnFootLockAlpha` / `AimYaw, AimPitch, AimAlpha`

## 튜닝

| 항목 | 위치 | 기본 | 효과 |
|---|---|---|---|
| TurnInPlaceStartAngle | 캐릭터 BP > Character Movement > PT\|TurnInPlace | 70° | 몸이 돌기 시작하는 카메라 각도 |
| TurnInPlaceRate | 〃 | 300°/s | 캡슐 회전 속도 (빠르면 스텝 수가 늘어남) |
| TurnBodyFollowSpeed | AnimBP Class Defaults > PT\|TurnInPlace | 6 | 몸이 따라 도는 속도 (낮을수록 천천히, 다리 꼬임↑) |
| TurnStepTriggerAngle | 〃 | 35° | 발과 몸 각도 차이가 이만큼 벌어지면 스텝 (크면 스텝 수↓, 보폭↑) |
| TurnSettleAngle / TurnSettleDelay | 〃 | 12° / 0.2초 | 회전 후 자세 정리 스텝 |
| TurnStepDuration | 〃 | 0.28초 | 발 하나 스텝 시간 |
| TurnInPlaceStartDelay | 캐릭터 BP > Character Movement > PT\|TurnInPlace | 0.35초 | 멈춤/착지 후 회전 시작까지 대기 |
| TurnEngageDelay | AnimBP Class Defaults > PT\|TurnInPlace | 0.35초 | 멈춤/착지 후 발 고정 시작까지 대기 (애니메이션 포즈 안정화) |
| MaxPlantedDrift / RecoveryStepDuration | 노드 디테일 | 12cm / 0.25초 | 디딘 발이 제자리에서 이만큼 벗어나면 자동 복구 스텝 (다리 꼬임 방지, 축에서 먼 앞발이 늦게 따라오는 것 방지) |
| OverlapStepStart | 노드 디테일 | 0.45 | 다른 발 스텝이 이만큼 진행된 뒤부터는 복구 스텝을 겹쳐서 시작 |
| MinStepDistance | 노드 디테일 | 6cm | 발이 이미 목표 근처면 지시된 스텝을 생략 (복구 스텝 직후 같은 발을 다시 드는 "탭댄스" 방지). 복구 스텝은 몸이 회전을 마칠 위치로 한 번에 이동 |
| MaxLegExtension | 노드 디테일 | 0.96 | 고정된 발까지 다리를 최대 이 비율까지만 폄 - 더 멀면 발이 살짝 끌려와 무릎이 쭉 펴지지 않음 |
| StepHeight / PelvisDropOnStep | 노드 디테일 | 5cm / 1cm | 발 드는 높이 / 골반 내려감 |
| PushOffPitch / SwingToePitch | 노드 디테일 > Foot | 18° / 8° | 발 뗄 때 뒤꿈치 드는 각도(발볼 기준) / 공중에서 발끝 드는 각도 |
| BallForwardOffset | 〃 | 12cm | 발목~발볼 거리 (뒤꿈치 들 때 회전 중심) |
| PlantedFootSwivel | 〃 | 0.2 | 디딘 발이 몸 회전을 따라 비트는 비율 |
| KneeFollowFoot | 〃 | 0.5 | 무릎이 발 방향을 따라가는 비율 (발목 비틀림 완화) |
| 본 이름 | 노드 디테일 | root, pelvis, thigh/calf/foot_l·r | 다른 스켈레톤이면 변경 |

## 나중에 회전 애니메이션(GASP/Lyra)을 쓰게 되면

모션 매칭 데이터베이스에 회전 애니메이션을 넣고, AnimBP 의 `bEnableTurnInPlace` 를 끄면 캡슐 회전 판단(무브먼트)은 그대로 두고 절차적 스텝만 빠집니다.
