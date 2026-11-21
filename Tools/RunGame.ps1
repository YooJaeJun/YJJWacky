# 게임을 창 모드로 띄워 자동 테스트를 돌리고 로그 요약을 보여준다 (스크린샷은 Saved/Screenshots/Play_*.png).
#   powershell -File Tools/RunGame.ps1 -Mode play    # 자동 플레이 검증 ([AutoPilot] PASS/FAIL)
#   powershell -File Tools/RunGame.ps1 -Mode play -Waves 10   # 10 웨이브 끝까지
#   powershell -File Tools/RunGame.ps1 -Mode play -Tests walk,stress   # 일부만 (walk, towers, stress, waves)
#   powershell -File Tools/RunGame.ps1 -Mode none    # 그냥 플레이
param(
    [ValidateSet("play", "none")]
    [string]$Mode = "none",
    [string]$EngineDir = "D:\source\repos\UnrealEngine\Engine",
    [int]$ResX = 1600,
    [int]$ResY = 900,
    [int]$TimeoutSec = 1500,
    [int]$Waves = 3,
    [string]$Tests = "",
    # 임시: 테스트가 시끄러워서 기본은 소리 끔 (-Sound 로 켠다)
    [switch]$Sound
)

$ProjectDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$Project = Join-Path $ProjectDir "YJJWacky.uproject"
$Editor = Join-Path $EngineDir "Binaries\Win64\UnrealEditor.exe"
$Log = Join-Path $ProjectDir "Saved\Logs\RunGame.log"
$Shots = Join-Path $ProjectDir "Saved\Screenshots"

$Flags = @()
if (-not $Sound) { $Flags += "-nosound" }
if ($Mode -eq "play") { $Flags += "-yjjplay", "-yjjquit", "-yjjwaves=$Waves"; if ($Tests) { $Flags += "-yjjtests=$Tests" }; Remove-Item (Join-Path $Shots "Play_*.png") -ErrorAction SilentlyContinue }

$Args = @("`"$Project`"", "-game", "-windowed", "-ResX=$ResX", "-ResY=$ResY", "-WinX=40", "-WinY=40", "-nosplash", "-unattended", "-log", "-LogCmds=`"LogYJJWacky Verbose`"", "-abslog=`"$Log`"") + $Flags
$Process = Start-Process -FilePath $Editor -ArgumentList $Args -PassThru
if ($Mode -ne "none") {
    if (-not $Process.WaitForExit($TimeoutSec * 1000)) {
        $Process.Kill()
        Write-Host "Timed out"
    }
    Select-String -Path $Log -Pattern "\[AutoPilot\]|\[Game\]|\[Data\]|\[Loc\]|\[Wobbler\]|Error:|Ensure condition failed|Fatal|Failed to compile" | ForEach-Object { $_.Line }
}
