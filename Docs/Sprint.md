# 전력질주 & 정지 슬라이드

## 조작

- **Left Shift + 이동** (게임패드: 왼쪽 스틱 클릭) - 누르고 있는 동안 전력질주 (`UPTGameplayAbility_Sprint`, 속도 700)
- 전력질주 중 이동 입력을 놓으면 **미끄러지며 정지** (약 0.4초, 1.3m)
- 일반 달리기(500)에서 멈추면 바로 정지 (약 0.15초, 30cm) - 발 미끄러짐 최소화

## 구현

| 위치 | 내용 |
|---|---|
| `UPTCharacterMovementComponent` | Gait 별 `BrakingDeceleration` + `BrakingFriction`(속도 비례 마찰). 공중에서는 마찰 0 (점프 관성 유지) |
| 〃 `UpdateSprintStopSlide()` | 지상 + 입력 없음 + 속도 ≥ `SprintStopSlideMinSpeed`(600) → 슬라이드 시작, `SprintStopSlideEndSpeed`(40) 미만이거나 다시 입력하면 종료. 슬라이드 중 감속 1400 / 마찰 1 |
| 〃 SavedMove `FLAG_Custom_3` | 슬라이드 상태를 클라이언트 예측 + 서버 동일 계산 (보정 없음) |
| 〃 IntentFlags `Intent_SprintStopSlide` | 시뮬레이티드 프록시(다른 플레이어 화면)에도 슬라이드 표시 |
| `UPTMotionMatchingAnimInstance::UpdateSprintPresentation()` | 슬라이드 시작 시 `SprintStopAnimation`(기본: 템플릿 `MM_Land`)을 DefaultSlot 에 재생. 다시 이동하면 블렌드 아웃 |
| 〃 전력질주 애니 속도 | 모션 매칭 DB 가 없을 때(템플릿 블렌드스페이스) 질주 속도 / 500 만큼 메시 애니 재생 속도 증가 (최대 1.35배) |
| 〃 `BodyLeanPitch` | 질주 시 앞으로 12°, 슬라이드 시 뒤로 14° 기울임 (PT Aim Offset 노드의 Lean Pitch 핀) |

## GASP 연결 후

- `SprintStopAnimation` 을 GASP 의 Sprint Stop 애니메이션으로 교체 (AnimBP 클래스 디폴트 `PT|Sprint`)
- 모션 매칭 DB(`DatabaseSet`)를 지정하면 애니 속도 스케일은 자동으로 꺼집니다
- 정지 거리는 GASP 정지 애니메이션의 이동 거리에 맞춰 `BrakingDeceleration / BrakingFriction`(Gait 설정)과 `SprintStopSlide*` 값을 조정하세요
