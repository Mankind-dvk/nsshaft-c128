$ErrorActionPreference = 'Stop'

& (Join-Path $PSScriptRoot 'build.ps1')
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$vice = 'D:\C64Tools\GTK3VICE-3.10-win64\bin\x128.exe'
$milliseconds = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$seed = [int](($milliseconds % 2147483646) + 1)

& $vice `
    +saveres `
    -40col `
    -hidevdcwindow `
    -seed $seed `
    -sound `
    -sounddev wmm `
    -soundvolume 100 `
    -controlport1device 2 `
    -controlport2device 0 `
    -paddles1inputmouse `
    -mouse `
    +autostart-warp `
    -autostart-drop-mode 2 `
    -autostartprgmode 2 `
    -autostart (Join-Path $PSScriptRoot 'build\nsshaft-c128.prg')
exit $LASTEXITCODE
