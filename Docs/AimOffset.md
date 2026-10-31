# C++ 절차적 에임 오프셋

에임 오프셋 에셋(블렌드 스페이스) 없이, 조준 방향(Yaw/Pitch)을 **척추 → 목 → 머리** 본 체인에 가중치대로 나눠 회전시키는 방식입니다.
재생 중인 포즈와 상관없이 그 위에 얹히므로 모션 매칭과 잘 맞습니다.

## 구성

| 위치 | 역할 |
|---|---|
| `APTCharacter::GetAimRotation()` | 조준 회전. 로컬/서버는 컨트롤러 회전, 원격(시뮬레이티드) 캐릭터는 엔진이 복제하는 Pitch + **직접 복제하는 Yaw**(`ReplicatedAimYaw`, 16bit, SimulatedOnly) |
| `UPTMotionMatchingAnimInstance::UpdateAimOffset()` (워커 스레드) | 액터 정면 기준 각도 계산 → 한계 클램프 → 임계 감쇠 스프링 스무딩 → 상황별 가중치를 **각도에 곱해서** `AimYaw / AimPitch` 출력 (`AimAlpha` 는 1 고정) |
| `UPTMotionMatchingAnimInstance::UpdateBodyLean()` | 몸 앞뒤 기울기 `BodyLeanPitch` (전력질주 시 앞으로 12°, 정지 슬라이드 시 뒤로 14°) |
| `FAnimNode_PTAimOffset` (PT 모듈) | 런타임 애님 노드. 액터 기준 축으로 본 체인을 회전 (부모 회전이 자식에 누적) |
| `UAnimGraphNode_PTAimOffset` (PTEditor 모듈) | AnimBP 그래프에 보이는 **"PT Aim Offset (Procedural)"** 노드 |

### 가중치 규칙 (기본값, 클래스 디폴트에서 조정)

| 상황 | 가중치 |
|---|---|
| 스트레이프 모드 | 1.0 |
| 이동 방향 회전 모드 - 정지 | 1.0 (`AimWeightOrientToMovement`) |
| 이동 방향 회전 모드 - 달리는 중 | × 0.65 (`AimWeightMoving`, 속도에 따라 서서히) |
| 스프린트 중 | × 0.25 |
| 공중 | × 0.5 |
| 정지 슬라이드 중 | × 0.3 |
| 파쿠르 / 루트 모션 몽타주 중 | 0 |
| 카메라가 뒤쪽(110°~160°)을 볼 때 | 1 → 0 으로 페이드 |

각도 한계: Yaw ±90° (달리는 중 ±65°), Pitch 위 70° / 아래 60°.

### 자연스러운 움직임

- 스무딩: `AimSmoothingTime`(0.08초), 가중치 `AimWeightSmoothingTime`(0.25초) - 임계 감쇠 스프링이라 출발/정지가 부드럽고 튀지 않음
- 노드의 **본별 지연**: `ChainRootSmoothingTime`(허리 0.18초) → `ChainTipSmoothingTime`(머리 0.05초). 머리가 먼저 돌고 허리가 늦게 따라옵니다
- **Lean Pitch** 핀: 척추 본들이 기울기를 나눠 갖고 목/머리가 반대로 돌려 시선을 수평으로 유지 (`bKeepHeadLevelOnLean`)

> 기존 `ABP_PTUnarmed` 에 Lean Pitch 핀을 연결하려면 `pt.Editor.SetupLocomotionABP` 를 한 번 더 실행하세요 (연결 안 된 핀만 추가로 연결).

## AnimBP 자동 설정 (권장)

에디터 콘솔(`~`)에서:

```
pt.Editor.SetupLocomotionABP
```

- `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed` 를 **복제**해서 `/Game/PT/Animation/ABP_PTUnarmed` 생성 (원본은 그대로)
- 부모 클래스 → `PTMotionMatchingAnimInstance`, Output Pose 앞에 Local To Component → PT Turn In Place → PT Aim Offset → Component To Local 삽입, AimYaw/AimPitch/AimAlpha/BodyLeanPitch 핀 연결, 컴파일·저장
- `PTCharacter` 는 메시가 없을 때 `ABP_PTUnarmed` 를 우선 사용 (없으면 원본 ABP_Unarmed)
- 다른 AnimBP 에 적용: `pt.Editor.SetupLocomotionABP <원본경로> <새경로>`

> 주의: 원본 AnimBP 의 부모 클래스를 직접 바꾸면, 템플릿 ABP 가 가진 `Character` / `MovementComponent` 변수와 이름이 겹쳐 컴파일 오류가 났었습니다 (C++ 쪽 이름을 `PTOwnerCharacter` / `PTMovementComponent` 로 변경해 해결). 그래도 원본은 건드리지 않고 복제본을 쓰는 것을 권장합니다.

## AnimBP 수동 연결

1. 애님 BP 의 부모 클래스가 `PTMotionMatchingAnimInstance` 인지 확인
   - 임시 마네킹의 `ABP_Unarmed` 로 먼저 테스트하려면: ABP_Unarmed 열기 → **Class Settings → Parent Class = PTMotionMatchingAnimInstance** 로 변경 (기존 그래프는 그대로 동작)
2. AnimGraph 의 Output Pose 직전에 다음을 추가
   ```
   (기존 포즈) → [Local To Component] → [PT Aim Offset (Procedural)] → [Component To Local] → Output Pose
   ```
3. 노드 핀 연결
   - **Aim Yaw** ← 변수 `AimYaw`
   - **Aim Pitch** ← 변수 `AimPitch`
   - **Alpha** ← 변수 `AimAlpha` (Alpha 핀이 안 보이면 노드 디테일에서 Alpha 를 핀으로 노출)
   - **Lean Pitch** ← 변수 `BodyLeanPitch`
4. 컴파일 → Play. 마우스를 위/아래/좌우로 움직이면 상체와 머리가 따라 돌아갑니다. Tab(스트레이프)을 켜면 더 강하게 적용됩니다.

## 본 체인 수정

노드 디테일 → **Bones** 배열. 기본값(UE5 마네킹 / GASP 공용):

| 본 | Yaw | Pitch |
|---|---|---|
| spine_01 | 0.10 | 0.10 |
| spine_02 | 0.10 | 0.10 |
| spine_03 | 0.15 | 0.15 |
| spine_04 | 0.15 | 0.15 |
| spine_05 | 0.20 | 0.15 |
| neck_01 | 0.15 | 0.15 |
| head | 0.15 | 0.20 |

- Yaw / Pitch 각각 합이 1.0 이면 체인 끝(머리)이 정확히 조준 방향을 봅니다.
- 스켈레톤에 없는 본은 자동으로 건너뜁니다 (그 본의 비율만큼 덜 돌아감).
- 무기를 들면 머리 비중을 줄이고 spine_04/05 비중을 늘리면 총구가 더 안정적으로 따라옵니다.

## 디버그

- 애님 BP 에디터 프리뷰 또는 `showdebug animation` 에서 노드의 Alpha / Yaw / Pitch 확인
- 변수 `AimYaw`, `AimPitch`, `AimAlpha` 를 AnimBP 디버그 창에서 실시간 확인

## 나중에 무기 전용 포즈가 필요하면

마네킹 팩의 `AO_Rifle`, `AO_Pistol` 같은 에임 오프셋 에셋을 무기 장착 상태에서만 블렌드하고, 이 노드는 그 위에서 보정용(Alpha 낮춤)으로 함께 쓸 수 있습니다 (C안).
