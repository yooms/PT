# =============================================================================
#  PT 프로젝트를 GitHub(https://github.com/yooms/PT)에 올립니다.
#  GitPush.bat 을 더블클릭하면 이 스크립트가 실행됩니다.  (GitPush.bat 커밋메시지 도 가능)
#  - 변경된 파일을 모두 커밋하고 푸시합니다. (.gitignore 로 Binaries/Intermediate/Saved 등 제외)
#  - 언리얼 에셋(.uasset/.umap 등)은 Git LFS 로 올라갑니다.
#  - 처음 실행 시 GitHub 로그인 창이 뜨면 로그인하세요.
# =============================================================================

$RemoteUrl = 'https://github.com/yooms/PT.git'
Set-Location -LiteralPath $PSScriptRoot

function Wait-Close
{
    Write-Host ''
    Read-Host '엔터를 누르면 창이 닫힙니다' | Out-Null
}

function Fail([string]$Message)
{
    Write-Host ''
    Write-Host "[오류] $Message" -ForegroundColor Red
    Wait-Close
    exit 1
}

# ---- 필수 프로그램 확인 ----
if (-not (Get-Command git -ErrorAction SilentlyContinue))
{
    Fail 'Git 이 설치되어 있지 않습니다. https://git-scm.com 에서 Git for Windows 를 설치한 뒤 다시 실행하세요.'
}
git lfs version *> $null
if ($LASTEXITCODE -ne 0)
{
    Fail 'Git LFS 가 없습니다. https://git-lfs.com 에서 설치한 뒤 다시 실행하세요.'
}

# ---- 저장소 준비 ----
# 다른 환경에서 만든 저장소라 "dubious ownership" 오류가 나지 않도록 안전한 폴더로 등록
$SafeDir = (Get-Location).Path -replace '\\', '/'
$SafeList = @(git config --global --get-all safe.directory 2>$null)
if ($SafeList -notcontains $SafeDir)
{
    git config --global --add safe.directory $SafeDir
}

git lfs install *> $null

if (-not (Test-Path -LiteralPath '.git'))
{
    Write-Host '저장소를 새로 만듭니다.'
    git init -b main
}
git remote get-url origin *> $null
if ($LASTEXITCODE -ne 0)
{
    git remote add origin $RemoteUrl
}

$Branch = (git rev-parse --abbrev-ref HEAD 2>$null)
if (-not $Branch -or $Branch -eq 'HEAD') { $Branch = 'main' }

# ---- 커밋 ----
Write-Host ''
Write-Host '=== 변경된 파일 ===' -ForegroundColor Cyan
git status --short
Write-Host ''

$Message = ($args -join ' ').Trim()
if (-not $Message)
{
    $Message = (Read-Host '커밋 메시지 (그냥 엔터 = 날짜로 자동 작성)').Trim()
}
if (-not $Message)
{
    $Message = 'Update ' + (Get-Date -Format 'yyyy-MM-dd HH:mm')
}

git add -A
if ($LASTEXITCODE -ne 0) { Fail 'git add 에 실패했습니다. 위 메시지를 확인하세요.' }

git diff --cached --quiet
if ($LASTEXITCODE -ne 0)
{
    git commit -m $Message
    if ($LASTEXITCODE -ne 0) { Fail '커밋에 실패했습니다. 위 메시지를 확인하세요.' }
}
else
{
    Write-Host '새로 커밋할 변경 사항이 없습니다. 기존 커밋만 푸시합니다.'
}

# ---- 푸시 ----
Write-Host ''
Write-Host "=== 푸시: origin/$Branch (LFS 파일이 많으면 시간이 걸립니다) ===" -ForegroundColor Cyan
git push -u origin $Branch
if ($LASTEXITCODE -eq 0)
{
    Write-Host ''
    Write-Host '완료: https://github.com/yooms/PT' -ForegroundColor Green
    Wait-Close
    exit 0
}

Write-Host ''
Write-Host '[알림] 푸시가 거부되었습니다. GitHub 에 이 PC 에 없는 커밋이 있습니다.' -ForegroundColor Yellow
Write-Host '       (예: 웹에서 직접 올린 파일, 다른 PC 에서 푸시한 내용)'
Write-Host '  1 = GitHub 의 변경을 먼저 받아서 합친 뒤 다시 푸시 (pull --rebase)'
Write-Host '  2 = GitHub 를 이 PC 의 내용으로 덮어쓰기 (force push - GitHub 쪽에만 있던 커밋은 사라짐)'
Write-Host '  그 외 = 취소'
$Choice = Read-Host '선택'

if ($Choice -eq '1')
{
    git pull --rebase origin $Branch
    if ($LASTEXITCODE -ne 0)
    {
        git rebase --abort *> $null
        Fail '자동으로 합치지 못했습니다. 웹에서만 올린 기록과 이 PC 의 기록이 서로 다르면 2번(덮어쓰기)을 사용하세요.'
    }
    git push -u origin $Branch
    if ($LASTEXITCODE -ne 0) { Fail '푸시에 실패했습니다. 위 메시지를 확인하세요.' }
}
elseif ($Choice -eq '2')
{
    $Confirm = Read-Host "정말 GitHub 의 $Branch 브랜치를 덮어쓸까요? (Y/N)"
    if ($Confirm -notmatch '^[Yy]$')
    {
        Write-Host '취소했습니다.'
        Wait-Close
        exit 0
    }
    git push -u origin $Branch --force
    if ($LASTEXITCODE -ne 0) { Fail '푸시에 실패했습니다. 위 메시지를 확인하세요.' }
}
else
{
    Write-Host '취소했습니다.'
    Wait-Close
    exit 0
}

Write-Host ''
Write-Host '완료: https://github.com/yooms/PT' -ForegroundColor Green
Wait-Close
exit 0
