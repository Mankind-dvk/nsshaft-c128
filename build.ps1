$ErrorActionPreference = 'Stop'

$cc65 = 'D:\C64Tools\cc65-snapshot-win64\bin'
$buildDirectory = Join-Path $PSScriptRoot 'build'
$sourceDirectory = Join-Path $PSScriptRoot 'src'

New-Item -ItemType Directory -Force $buildDirectory | Out-Null

& (Join-Path $cc65 'ca65.exe') `
    (Join-Path $sourceDirectory 'main.s') `
    -g `
    -l (Join-Path $buildDirectory 'nsshaft-c128.lst') `
    -o (Join-Path $buildDirectory 'nsshaft-c128.o')
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& (Join-Path $cc65 'ld65.exe') `
    -C (Join-Path $PSScriptRoot 'c128-prg.cfg') `
    -Ln (Join-Path $buildDirectory 'nsshaft-c128.lbl') `
    -m (Join-Path $buildDirectory 'nsshaft-c128.map') `
    -o (Join-Path $buildDirectory 'nsshaft-c128.prg') `
    (Join-Path $buildDirectory 'nsshaft-c128.o')
exit $LASTEXITCODE
