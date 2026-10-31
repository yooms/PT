# 리슨 서버 세션 & 호스트 인계

언리얼은 리슨 서버 호스트 인계를 기본 제공하지 않으므로 **"상태 스냅샷 복제 + 결정적 후보 선출 + 재접속"** 방식으로 구현했습니다.

## 동작 방식

```
[호스트]  APTGameMode ── SnapshotInterval(3초)마다 + 접속/퇴장 시 ──► FPTMigrationSnapshot 생성
          (플레이어 위치·회전·GAS 어트리뷰트·SaveGame 프로퍼티, Migratable 액터, GameState, 전역 데이터)
          → Zlib 압축 → APTGameState::MigrationData (청크 배열) 로 모든 클라이언트에 복제

[클라]    UPTHostMigrationSubsystem 이 최신 스냅샷 보관
          호스트 연결 끊김 (ConnectionLost/Timeout, 또는 호스트의 '나가기' 알림 후)
          ├─ 끊기는 순간 클라이언트가 가진 최신 복제 상태로 플레이어 위치/어트리뷰트 갱신
          ├─ 후보 순서 = JoinOrder 오름차순 (기존 호스트 제외) → 모든 클라이언트가 같은 결과
          ├─ 1순위  : 같은 맵을 listen 으로 열고 세션 생성 (Lineage 동일, Epoch+1, HostRank=0)
          │           → APTGameMode 가 스냅샷 복원 (액터 → GameState/전역 → 플레이어는 접속할 때마다)
          └─ 나머지 : 세션 검색 (Lineage 일치 & Epoch 증가) → 찾으면 접속 (?PTPID=<내 ID>)
                      N순위는 N × CandidatePromotionDelay(8초) 동안 못 찾으면 스스로 호스트가 됨
                      (상위 후보가 동시에 나간 경우 대비)
```

- **플레이어 식별**: `UPTSessionSubsystem::GetLocalPlayerId()` (프로세스마다 GUID, `-PTPlayerId=` 로 고정 가능). 접속 URL 의 `?PTPID=` 로 서버에 전달 → `APTPlayerState::PTPlayerId`.
- **중복 호스트 정리**: 하위 순위가 호스트가 된 뒤 `RedundantHostCheckDelay` 동안 아무도 오지 않으면, 더 높은 순위의 새 세션이 있는지 확인해 그쪽으로 합류합니다.
- **의도적 종료와 구분**: 클라이언트가 직접 나가기 / 킥 / 호스트의 `EndSessionForEveryone` 은 인계하지 않습니다.
- **진행 UI**: 에셋 없이 동작하는 Slate 오버레이(`SPTStatusOverlay`)가 표시됩니다. 커스텀 UI 는 `OnMigrationStateChanged` 델리게이트를 사용하세요.

## 게임 코드에서 해야 할 일

| 보존하고 싶은 것 | 방법 |
|---|---|
| 플레이어 위치/회전, GAS 어트리뷰트 | 자동 |
| PlayerState / Pawn / GameState 의 값 | 해당 `UPROPERTY` 에 `SaveGame` 지정 |
| 월드 액터 (문, 상자, 아이템, 적 …) | `IPTMigratableInterface` 구현 + 보존할 프로퍼티에 `SaveGame`. 레벨 배치 액터는 이름으로 매칭(인계 전에 파괴됐다면 새 호스트에서도 파괴), 런타임 스폰 액터는 클래스로 재생성 |
| 목표 진행도 등 전역 데이터 | `APTGameMode::CaptureMigrationGlobalData / RestoreMigrationGlobalData` (BP 오버라이드 가능) |
| 복원 직후 처리 | 액터: `OnMigrationRestored`, 플레이어: `OnPlayerRestoredFromMigration`, Pawn 에 `Event.Migration.Restored` 게임플레이 이벤트 |

> 스냅샷 크기: 압축 후 48KB 를 넘으면 경고 로그가 나옵니다. 큰 데이터(인벤토리 전체 등)는 ID 위주로 저장하세요.

## 메뉴/UI 연동 (Blueprint)

- `Get Game Instance → Get Subsystem(PTSessionSubsystem)`
  - `Host Session(Params)` — MapPath, MaxPlayers
  - `Find Sessions` → `OnSessionsFound(bSuccess, Sessions[])` → `Join Session By Index`
  - `Leave Session And Return To Menu`
- `APTPlayerController::LeaveGame()` — 호스트면 마지막 스냅샷 전송 후 인계, 클라이언트면 퇴장
- `APTPlayerController::EndSessionForEveryone()` — 호스트가 모두 종료

## 설정

`Project Settings > Game > PT Host Migration` (DefaultGame.ini `[/Script/PT.PTHostMigrationSettings]`)

| 항목 | 기본 | 설명 |
|---|---|---|
| SnapshotInterval | 3.0 | 스냅샷 갱신 주기 (초) |
| CandidatePromotionDelay | 8.0 | 하위 후보가 스스로 호스트가 되기까지 순위당 대기 |
| SearchInterval | 2.0 | 새 호스트 검색 주기 |
| MigrationTimeout | 60.0 | 전체 제한 시간 (+순위 × PromotionDelay) |
| RedundantHostCheckDelay | 10.0 | 중복 호스트 확인 지연 |

호스트가 강제 종료된 경우 감지 시간은 `DefaultEngine.ini [/Script/OnlineSubsystemUtils.IpNetDriver] ConnectionTimeout` (기본 10초로 설정) 에 좌우됩니다.

## Steam 으로 전환

1. `DefaultEngine.ini`
   - `[OnlineSubsystem] DefaultPlatformService=Steam`
   - `[OnlineSubsystemSteam] bEnabled=true` (`SteamDevAppId=480` 은 테스트용, 출시 시 실제 AppId)
   - 주석 처리된 `[/Script/Engine.GameEngine] NetDriverDefinitions` 블록 주석 해제
2. Steam 클라이언트 실행 상태에서 **서로 다른 Steam 계정/PC** 로 테스트
3. Steam 에서는 Lobby(`bUseLobbiesIfAvailable`) + Presence 로 세션이 광고/검색되며, 호스트 인계 키(`PT_LINEAGE`, `PT_EPOCH`, `PT_HOSTRANK`)도 Lobby 데이터로 전달됩니다.

## 한계 / 주의

- 인계 중에는 맵을 다시 로드하므로 **수 초간 끊김**이 있습니다 (seamless 하지 않음). 스냅샷 이후~끊김 사이의 서버 전용 변화는 유실될 수 있습니다 (플레이어 위치/어트리뷰트는 끊기는 순간 클라이언트 값으로 보정).
- AI 의 내부 상태(비헤이비어 트리 진행 등)는 자동 보존되지 않습니다 → Migratable 인터페이스 + SaveGame 으로 필요한 것만 저장.
- NAT 환경의 인터넷 플레이는 Steam/EOS 등 릴레이를 제공하는 서브시스템 사용을 권장합니다 (Null 은 LAN 전용).
