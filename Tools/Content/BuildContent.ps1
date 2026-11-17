# 소리를 합성하고, 머티리얼·맵을 만들고, 소리를 임포트한다. 에디터를 끈 상태에서 실행.
#   powershell -File Tools/Content/BuildContent.ps1
#   powershell -File Tools/Content/BuildContent.ps1 -SkipAudio   # 소리 합성 생략 (이미 있으면)
param(
    [string]$EngineDir = "D:\source\repos\UnrealEngine\Engine",
    [switch]$SkipAudio
)

if (-not $SkipAudio) {
    $Python = Join-Path $EngineDir "Binaries\ThirdParty\Python3\Win64\python.exe"
    $AudioDir = Join-Path $PSScriptRoot "..\Audio"
    & $Python -I (Join-Path $AudioDir "generate_audio.py") (Join-Path $AudioDir "Generated")
    if ($LASTEXITCODE -ne 0) { throw "generate_audio.py failed" }
}

$ErrorActionPreference = "Stop"
$ProjectDir = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Project = Join-Path $ProjectDir "YJJWacky.uproject"
$Editor = Join-Path $EngineDir "Binaries\Win64\UnrealEditor-Cmd.exe"
$Log = Join-Path $ProjectDir "Saved\Logs\BuildContent.log"

$Script = (Join-Path $PSScriptRoot "build_content.py") -replace '\\', '/'
& $Editor $Project -run=pythonscript -script="$Script" -unattended -nosplash -nop4 -stdout -FullStdOutLogOutput -abslog="$Log" | Out-Null
$Code = $LASTEXITCODE
Select-String -Path $Log -Pattern "\[BuildContent\]|Error:|Traceback|error X|failed" | ForEach-Object { $_.Line }
if ($Code -ne 0) { throw "build_content.py failed ($Code)" }
