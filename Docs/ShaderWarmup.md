# 셰이더(PSO) 워밍업

패키징된 게임에서 "셰이더 컴파일"은 실제로 **PSO(Pipeline State Object) 컴파일**입니다 (머티리얼 셰이더 자체는 쿡 시 컴파일됨). PSO 가 준비되지 않은 상태로 처음 화면에 나오면 히치가 생기므로, 최초 실행이나 콘텐츠가 바뀌었을 때 로딩 화면에서 미리 컴파일합니다.

## 동작

`UPTShaderWarmupSubsystem` (GameInstanceSubsystem)

1. 첫 프레임(뷰포트 생성 후) **Fingerprint** 계산
   - 빌드 버전, 엔진 버전, `FingerprintSalt`
   - RHI / GPU 이름 / 드라이버 버전 (드라이버가 바뀌면 드라이버 PSO 캐시가 무효화되므로)
   - `Content/Paks` 의 `.pak/.utoc/.ucas` 파일 이름·크기·시각 → **패치/DLC 설치 감지**
2. `GameUserSettings.ini` 에 저장된 값과 다르면 워밍업 화면 표시 후 다음이 모두 끝날 때까지 대기
   - `FShaderPipelineCache::NumPrecompilesRemaining()` — 번들 PSO 캐시 프리컴파일 (Fast 배치 모드)
   - `PipelineStateCache::NumActivePrecacheRequests()` — UE5 PSO Precache
   - (uncooked 실행 시) `GShaderCompilingManager` 남은 작업
   - 설정의 `WarmupAssets` 비동기 로드 (로드 시 PSO Precache 유발)
3. 완료 → Fingerprint 저장, 배치 모드를 Background 로 전환 (이후 실행은 화면 없이 백그라운드)
4. **런타임 Pak 마운트**(DLC/핫픽스) 감지 시: 메뉴(Standalone)에서는 즉시 다시 워밍업, 플레이 중에는 Fast 배치만 적용하고 다음 실행 때 전체 워밍업

메뉴 UI 에서는 `IsWarmupComplete()` / `OnWarmupComplete` 로 Host/Join 버튼을 잠글 수 있습니다.

## 설정

`Project Settings > Game > PT Shader Warmup` (DefaultGame.ini `[/Script/PT.PTShaderWarmupSettings]`)

| 항목 | 설명 |
|---|---|
| bEnableWarmup | 사용 여부 |
| bRunInEditor | 에디터(PIE)에서도 실행 (기본 꺼짐) |
| MinimumDisplayTime / MaxWaitTime | 최소 표시 / 최대 대기 시간 |
| bWaitForPSOPrecache | UE5 PSO Precache 요청도 대기 |
| bRewarmOnRuntimePakMount | 런타임 Pak 마운트 시 재실행 |
| WarmupAssets | 워밍업 중 미리 로드할 머티리얼/메시/나이아가라 등 |
| FingerprintSalt | 값을 바꾸면 모든 유저가 다음 실행 시 강제 재컴파일 |

관련 CVar 는 `DefaultEngine.ini [SystemSettings]` 에 있습니다 (`r.ShaderPipelineCache.Enabled=1`, `StartupMode=0` 등). 서브시스템이 시작 시 배치를 재개하므로 `StartupMode=0`(Paused) 이 안전합니다.

## 번들 PSO 캐시 만들기 (패키징 전 1회 이상)

PSO Precache(UE5 기본)만으로도 상당 부분 커버되지만, 글로벌 셰이더·포스트프로세스 등 누락분은 **번들 PSO 캐시**로 보완합니다.

1. 패키징 설정: `bShareMaterialShaderCode=True`, `bSharedMaterialNativeLibraries=True` (이미 DefaultGame.ini 에 설정됨)
2. 패키징 빌드를 `-logPSO` 인자로 실행해 게임을 플레이 (모든 맵/이펙트를 최대한 경험)
   → `Saved/CollectedPSOs/*.rec.upipelinecache` 생성
3. 쿡 결과의 `*.shk` (ShaderStableInfo) 파일과 함께 변환:
   ```
   UnrealEditor-Cmd.exe PT.uproject -run=ShaderPipelineCacheTools expand ^
       <CollectedPSOs>\*.rec.upipelinecache <Cooked>\*.shk ^
       PT_PCD3D_SM6.spc
   ```
4. 생성된 `.spc` 파일을 `Build/Windows/PipelineCaches/` 에 넣고 다시 패키징 → 게임에 번들 PSO 캐시 포함
5. 콘텐츠를 크게 바꾼 뒤에는 2~4 를 반복하세요.

## 테스트

- 패키징 빌드 실행 (또는 `UnrealEditor.exe PT.uproject -game` + `bRunInEditor=True`)
- 콘솔 `pt.ShaderWarmup.Run` 으로 강제 실행
- 로그 카테고리 `LogPTShader`
