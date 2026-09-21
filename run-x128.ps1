param(
    [string]$VicePath = $env:VICE_X128,
    [string]$Cc65Bin = $env:CC65_BIN
)

$ErrorActionPreference = 'Stop'
if (!$VicePath) { $VicePath = (Get-Command x128 -ErrorAction Stop).Source }
if (!(Test-Path -LiteralPath $VicePath)) { throw 'Set VICE_X128 or pass -VicePath with the path to x128.exe.' }

& (Join-Path $PSScriptRoot 'build.ps1') -Cc65Bin $Cc65Bin
$milliseconds = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$seed = [int](($milliseconds % 2147483646) + 1)

& $VicePath `
    +saveres -pal -40col -hidevdcwindow -seed $seed `
    -sound -sounddev wmm -soundvolume 100 `
    -controlport1device 2 -controlport2device 0 `
    -paddles1inputmouse -mouse +autostart-warp `
    -autostart-drop-mode 2 -autostartprgmode 2 `
    -autostart (Join-Path $PSScriptRoot 'build/nsshaft-c128.prg')
if ($LASTEXITCODE -ne 0) { throw "VICE failed with exit code $LASTEXITCODE" }
