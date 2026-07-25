# update.ps1 — reinstalls the latest release with the choices you made the first
# time.
#
#   .\update.ps1                      update the install found in the usual places
#   .\update.ps1 -Prefix DIR          update the install at DIR
#   .\update.ps1 -Check               report versions, change nothing
#   .\update.ps1 -DryRun              show the install it would run
#
# It reads the manifest install.ps1 wrote and replays the same options, so the
# flavour you picked — native or universal compiler, which tools, which shared
# stdlib — survives the update instead of silently reverting to the defaults.

[CmdletBinding()]
param(
    [string]$Prefix,
    [switch]$Check,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$Repo = "Caua726/Caustic"

if (-not $Prefix) {
    foreach ($p in @((Join-Path $env:LOCALAPPDATA "caustic"), (Join-Path $env:ProgramFiles "caustic"))) {
        if (Test-Path (Join-Path $p "lib\caustic\install-manifest")) { $Prefix = $p; break }
    }
    if (-not $Prefix) { throw "no caustic install found (use -Prefix DIR, or install.ps1 for a first install)" }
}

$manifest = Join-Path $Prefix "lib\caustic\install-manifest"
if (-not (Test-Path $manifest)) { throw "no manifest at $manifest - run install.ps1 instead" }
$lines = Get-Content $manifest
function Get-Field([string]$k) {
    ($lines | Where-Object { $_ -like "$k=*" } | Select-Object -First 1) -replace "^$k=", ""
}

$mFormat = Get-Field "format"; $mTools = Get-Field "tools"; $mLib = Get-Field "lib"
$mSource = Get-Field "source"; $mSystem = Get-Field "system"

$have = $null
$exe = Join-Path $Prefix "bin\caustic.exe"
if (Test-Path $exe) { $have = (& $exe --version 2>$null | Select-Object -First 1) -replace '^caustic\s+', '' }

$latest = $null
try {
    # The redirect target of /releases/latest ends in the tag, which is the
    # cheapest way to ask "what is current" without an API token.
    $r = Invoke-WebRequest -Uri "https://github.com/$Repo/releases/latest" -UseBasicParsing -MaximumRedirection 0 -ErrorAction SilentlyContinue
    $loc = $r.Headers.Location
    if (-not $loc) { $loc = (Invoke-WebRequest -Uri "https://github.com/$Repo/releases/latest" -UseBasicParsing).BaseResponse.ResponseUri.AbsoluteUri }
    if ($loc -match '/tag/v?(.+)$') { $latest = $Matches[1] }
} catch { }

Write-Host "caustic update"
Write-Host "  prefix:    $Prefix"
Write-Host "  installed: $(if ($have) { $have } else { 'unknown' })"
Write-Host "  latest:    $(if ($latest) { $latest } else { 'unknown' })"
Write-Host "  replaying: format=$mFormat tools=$mTools lib=$mLib source=$mSource"

if ($Check) {
    if ($have -and $latest -and ($have -eq $latest)) { Write-Host "up to date" }
    else { Write-Host "an update is available - run without -Check to install it" }
    exit 0
}
if ($have -and $latest -and ($have -eq $latest)) { Write-Host "already at $have - reinstalling anyway" }

# Prefer the install.ps1 sitting next to this script; fall back to fetching it.
$installer = if ($PSScriptRoot) { Join-Path $PSScriptRoot "install.ps1" } else { $null }
$tmp = $null
if (-not ($installer -and (Test-Path $installer))) {
    $tmp = Join-Path $env:TEMP ("caustic-update-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $tmp | Out-Null
    $installer = Join-Path $tmp "install.ps1"
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/$Repo/main/install.ps1" -OutFile $installer -UseBasicParsing
}

try {
    $argv = @("-Prefix", $Prefix, "-Format", $mFormat, "-Tools", $mTools, "-Lib", $mLib)
    if ($mSource -eq "0") { $argv += "-NoSource" }
    if ($mSystem -eq "1") { $argv += "-System" }
    if ($DryRun)          { $argv += "-DryRun" }
    & $installer @argv
}
finally {
    if ($tmp) { Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue }
}
