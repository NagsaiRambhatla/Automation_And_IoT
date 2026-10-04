param([string]$ArduinoCli = 'arduino-cli', [string]$ConfigFile)
$ErrorActionPreference = 'Stop'
$stage = Join-Path $PSScriptRoot 'build\main-dashboard\sketch'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'sketch.ino') -Destination $stage
Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.h' | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $stage
}
$arguments = @('compile', '--fqbn', 'esp32:esp32:esp32', '--warnings', 'default', '--export-binaries')
if ($ConfigFile) { $arguments += @('--config-file', $ConfigFile) }
$arguments += $stage
& $ArduinoCli @arguments
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }
Write-Output (Join-Path $stage 'build\esp32.esp32.esp32\sketch.ino.merged.bin')
