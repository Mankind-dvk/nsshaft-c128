$ErrorActionPreference = 'Stop'

$cc65 = 'D:\C64Tools\cc65-snapshot-win64\bin'
$vice = 'D:\C64Tools\GTK3VICE-3.10-win64\bin\x128.exe'
$buildDirectory = Join-Path $PSScriptRoot 'build'
$source = Join-Path $PSScriptRoot 'tests\sid-tone.s'
$object = Join-Path $buildDirectory 'sid-tone.o'
$program = Join-Path $buildDirectory 'sid-tone.prg'

New-Item -ItemType Directory -Force $buildDirectory | Out-Null

& (Join-Path $cc65 'ca65.exe') $source -g -o $object
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& (Join-Path $cc65 'ld65.exe') `
    -C (Join-Path $PSScriptRoot 'tests\sid-tone.cfg') `
    -o $program `
    $object
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $vice `
    +saveres `
    -40col `
    -hidevdcwindow `
    -sound `
    -sounddev wmm `
    -soundvolume 100 `
    +autostart-warp `
    -autostart-drop-mode 2 `
    -autostartprgmode 2 `
    -autostart $program
exit $LASTEXITCODE
