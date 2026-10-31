# 발 IK (경사면 / 계단)

| 위치 | 역할 |
|---|---|
| `UPTMotionMatchingAnimInstance::UpdateFootIK()` (게임 스레드) | 각 발(foot_l / foot_r)의 수평 위치에서 캡슐 바닥 기준 위 50 / 아래 60cm 를 라인 트레이스 → 바닥 높이·법선. 낮은 쪽 발에 맞춰 골반 하강량 계산, 임계 감쇠 스프링으로 스무딩 |
| `FAnimNode_PTFootIK` | 골반을 내리고 각 발을 바닥 높이만큼 올리거나 내려 Two Bone IK, 발바닥을 바닥 경사에 맞춰 회전 (최대 35°) |

- 지상에서만 동작하고, 공중 / 파쿠르 몽타주 중에는 0 으로 서서히 돌아갑니다.
- AnimGraph 순서: … → PT Turn In Place → **PT Foot IK** → PT Aim Offset → …
- `pt.Editor.SetupGASP` 를 다시 실행하면 기존 `ABP_PTMotionMatching` 에 노드를 삽입하고 핀(FootIK* 변수)을 연결합니다.

## 조정 (AnimBP 클래스 디폴트 `PT|FootIK`)

| 항목 | 기본 | 설명 |
|---|---|---|
| bEnableFootIK | true | 사용 여부 |
| FootIKTraceUp / Down | 50 / 60 | 바닥을 찾는 범위 (cm) |
| FootIKMaxPelvisDrop / MaxFootRaise | 40 / 45 | 골반을 내릴 / 발을 올릴 최대 거리 |
| FootIKSmoothingTime / PelvisSmoothingTime | 0.06 / 0.1 | 높이 변화 스무딩 (초) |
| (노드) MaxFootRotation | 35° | 발바닥 경사 맞춤 최대 각도 |
