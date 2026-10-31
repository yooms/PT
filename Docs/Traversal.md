# 파쿠르 (Traversal) - Hurdle / Vault / Mantle

Game Animation Sample(GASP)의 Traversal 과 같은 3가지 동작을 **게임플레이 어빌리티 시스템 + C++** 로 구현했습니다.
애니메이션(몽타주)은 GASP 에셋을 연결해서 사용합니다.

> **GASP 연결됨**: `pt.Editor.SetupGASP` 가 `DA_PTTraversalAnimSet`(Hurdle/Vault/Mantle/Climb/Catch, stand·walk·run 별)을 만들고 캐릭터가 자동 로드합니다. ABP_PTMotionMatching 의 Pose History 로 몽타주와 시작 프레임을 모션 매칭해 고릅니다. 자세한 내용은 [GASP.md](GASP.md).
>
> **GASP 몽타주가 없을 때**: `bUseProceduralFallback`(기본 켜짐)으로 **절차적 모드**로 동작합니다.
> 몽타주 대신 구간별 루트 모션(`ApplyRootMotionMoveToForce`)으로 캡슐을 옮기고, 템플릿 마네킹의 `MM_Jump` 를 DefaultSlot 에 재생합니다.
> - Hurdle: 윗면 위(다리 접은 높이) → 뒤쪽 바닥, 속도 유지
> - Vault: 앞 모서리 → 윗면 가로지르기 → 뒤쪽 바닥, 속도 60% 유지
> - Mantle: 벽 앞에서 윗면 높이까지 상승 → 윗면 위로 올라서기, 정지
> - 설정: Traversal 컴포넌트 `PT|Traversal|Procedural` (애니메이션, 슬롯, 속도, `ProceduralFeetTuck` 등)
> - AnimSet 에 맞는 몽타주가 있으면 항상 몽타주 쪽이 우선합니다.

| 동작 | 조건 (기본값) |
|---|---|
| **Hurdle** | 지상, 얇은 장애물(깊이 ≤ 60), 높이 50~125, 뒤쪽 바닥 있음, 속도 ≥ 300, 뒤쪽 바닥이 50cm 이상 낮지 않음 |
| **Vault** | 위와 같지만 느리거나 뒤쪽 바닥이 많이 낮을 때 (손 짚고 넘기) |
| **Mantle** | 높이 50~275, 두껍거나 뒤쪽 끝이 없는 장애물 위에 캡슐이 들어갈 자리가 있을 때 (공중에서도 가능) |

## 흐름

```
점프 키 (로컬)
 └ APTCharacter::Input_AbilityInputTagPressed
    └ UPTTraversalComponent::TryTraversal
       ├ PerformTraversalCheck  : 앞쪽 캡슐 스윕 → 윗면 높이 → 윗면을 따라 뒤쪽 끝(깊이) → 뒤쪽 바닥 → 공간 확인 → 동작 결정
       ├ UPTTraversalAnimSet::SelectMontage : 동작/높이/Gait/공중 조건 + (Pose History 있으면) 모션 매칭으로 몽타주·시작 시간 선택
       └ Event.Traversal + TargetData(FPTGameplayAbilityTargetData_Traversal) 발송
          └ UPTGameplayAbility_Traversal (LocalPredicted)
             ├ 클라이언트 즉시 실행 / 서버는 같은 데이터를 검증(거리·높이·몽타주) 후 실행
             ├ 이동 모드 Flying, 장애물과 캡슐 충돌 무시
             ├ 모션 워핑 타겟: FrontLedge / BackLedge / BackFloor
             └ 몽타주 재생 → 끝나면 Walking 복귀
 (파쿠르 조건이 아니면 일반 점프)
```

- 장애물에 전용 액터/인터페이스가 필요 없습니다. 일반 스태틱 메시(템플릿 맵의 큐브 등)에서 동작합니다.
- 파쿠르 중에는 제자리 회전 / 에임 오프셋이 자동으로 꺼집니다 (루트 모션 몽타주 재생 감지).
- 몽타주는 어빌리티 시스템이 다른 플레이어에게 복제합니다.
- 디버그: `pt.Traversal.Debug 1` (트레이스와 앞 모서리/뒤 모서리/뒤 바닥을 표시하고 판정 결과를 로그로 출력)

## GASP 연결 (에셋 작업)

1. Fab 에서 **Game Animation Sample** 을 5.8 버전으로 받아 별도 프로젝트로 생성
2. GASP 의 Traversal 몽타주들을 PT 로 Migrate (FrontLedge / BackLedge / BackFloor 모션 워핑 노티파이가 들어 있음)
3. `DA_TraversalAnimSet` 생성 (클래스: `PTTraversalAnimSet`)
   - Entries: 몽타주마다 Action(Hurdle/Vault/Mantle), 높이 범위, Gait, bInAir 지정
   - 워프 타겟 이름이 다르면 FrontLedge/BackLedge/BackFloor Warp Name 수정
   - 모션 매칭 시작 포즈 선택을 쓰려면 애님 BP 에 Pose History 노드 (태그 `PoseHistory`)
4. 캐릭터 BP → Traversal 컴포넌트 → **Anim Set** 에 `DA_TraversalAnimSet` 지정
5. 애님 BP 에 `DefaultSlot` 슬롯 노드가 있어야 몽타주가 재생됩니다

> 주의: GASP 애니메이션은 UEFN 마네킹 스켈레톤 기준입니다. 현재 임시 캐릭터(UE5 Quinn)와 스켈레톤이 다르므로,
> GASP 캐릭터 메시로 바꾸거나 두 스켈레톤을 호환(Compatible Skeletons / 리타깃) 처리해야 몽타주가 재생됩니다.

## 튜닝 (캐릭터 BP > Traversal 컴포넌트)

| 항목 | 기본 | 설명 |
|---|---|---|
| Min / Max Trace Distance, Trace Distance Speed | 75 / 350cm, 500cm/s | 빠를수록 멀리서 반응 |
| Min Obstacle Height / Max Vault Height / Max Mantle Height | 50 / 125 / 275cm | 동작별 높이 한계 |
| Max Vault Depth | 60cm | 이하면 넘기, 초과면 올라타기 |
| Min Hurdle Speed / Max Hurdle Drop | 300cm/s / -50cm | Hurdle ↔ Vault 구분 |
| Trace Channel | Visibility | 장애물 판정 채널 |
