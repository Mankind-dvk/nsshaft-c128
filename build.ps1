param(
    [string]$Cc65Bin = $env:CC65_BIN
)

$ErrorActionPreference = 'Stop'

# Set CC65_BIN (or pass -Cc65Bin) to the cc65 bin directory, or use PATH.
if ($Cc65Bin) {
    $assembler = Join-Path $Cc65Bin 'ca65.exe'
    $linker = Join-Path $Cc65Bin 'ld65.exe'
} else {
    $assembler = (Get-Command ca65 -ErrorAction Stop).Source
    $linker = (Get-Command ld65 -ErrorAction Stop).Source
}
if (!(Test-Path -LiteralPath $assembler) -or !(Test-Path -LiteralPath $linker)) {
    throw 'ca65 and ld65 are required. Set CC65_BIN or pass -Cc65Bin with the cc65 bin directory.'
}

$buildDirectory = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force $buildDirectory | Out-Null

& $assembler (Join-Path $PSScriptRoot 'src/main.s') -g `
    -l (Join-Path $buildDirectory 'nsshaft-c128.lst') `
    -o (Join-Path $buildDirectory 'nsshaft-c128.o')
if ($LASTEXITCODE -ne 0) { throw "ca65 failed with exit code $LASTEXITCODE" }

& $linker -C (Join-Path $PSScriptRoot 'c128-prg.cfg') `
    -Ln (Join-Path $buildDirectory 'nsshaft-c128.lbl') `
    -m (Join-Path $buildDirectory 'nsshaft-c128.map') `
    -o (Join-Path $buildDirectory 'nsshaft-c128.prg') `
    (Join-Path $buildDirectory 'nsshaft-c128.o')
if ($LASTEXITCODE -ne 0) { throw "ld65 failed with exit code $LASTEXITCODE" }
Write-Host "Built $buildDirectory\nsshaft-c128.prg"
