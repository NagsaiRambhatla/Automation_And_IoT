param(
    [string]$ArduinoCli,
    [string]$ConfigFile
)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
if (-not $ArduinoCli) {
    $ArduinoCli = (Get-Command arduino-cli -ErrorAction Stop).Source
}
# Arduino requires sketch.ino to be inside a directory named sketch.
# Build artifacts and the staged private credentials stay inside ignored build/.
$taskStage = Join-Path $taskRoot 'build\sketch'
New-Item -ItemType Directory -Force -Path $taskStage | Out-Null
Copy-Item -LiteralPath (Join-Path $taskRoot 'sketch.ino') -Destination $taskStage
Get-ChildItem -LiteralPath $taskRoot -Filter '*.h' | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $taskStage
}
$taskStagedSecret = Join-Path $taskStage 'secrets.h'
if (-not (Test-Path -LiteralPath (Join-Path $taskRoot 'secrets.h')) -and (Test-Path -LiteralPath $taskStagedSecret)) {
    Remove-Item -LiteralPath $taskStagedSecret
}
$taskArguments = @('compile', '--fqbn', 'esp32:esp32:esp32', '--warnings', 'default', '--export-binaries')
if ($ConfigFile) { $taskArguments += @('--config-file', $ConfigFile) }
$taskArguments += $taskStage
& $ArduinoCli @taskArguments
if ($LASTEXITCODE -ne 0) { throw "Firmware compilation failed ($LASTEXITCODE)." }
Write-Output (Join-Path $taskStage 'build\esp32.esp32.esp32\sketch.ino.merged.bin')
