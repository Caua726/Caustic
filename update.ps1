# update.ps1 - reinstalls the latest release with the choices you made the first
# time. Works on Windows and on Linux.
#
#   .\update.ps1                      update the install found in the usual places
#   .\update.ps1 -Prefix DIR          update the install at DIR
#   .\update.ps1 -Check               report versions, change nothing
#   .\update.ps1 -DryRun              show the install it would run
#
# It reads the manifest install.ps1 wrote and replays the same options, so the
# flavour you picked - native or universal compiler, which tools, which shared
# stdlib - survives the update instead of silently reverting to the defaults.
#
# A piped one-liner cannot be given parameters, so each of those is also an
# environment variable: $env:CAUSTIC_PREFIX, $env:CAUSTIC_CHECK,
# $env:CAUSTIC_DRYRUN.

[CmdletBinding()]
param(
    [string]$Prefix,
    [switch]$Check,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$Repo = "Caua726/Caustic"

# See install.ps1: 5.1 needs TLS 1.2 turned on before it can reach GitHub, and
# its progress bar dominates the transfer time. Both no-ops on 7.
if ($PSVersionTable.PSVersion.Major -lt 5) {
    throw "PowerShell 5.0 or newer is required (this is $($PSVersionTable.PSVersion))."
}
try {
    [Net.ServicePointManager]::SecurityProtocol =
        [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
} catch { }
$ProgressPreference = 'SilentlyContinue'

if (-not $Prefix) { $Prefix = $env:CAUSTIC_PREFIX }
if (-not $Check)  { $Check  = ($env:CAUSTIC_CHECK  -eq "1") }
if (-not $DryRun) { $DryRun = ($env:CAUSTIC_DRYRUN -eq "1") }

# Get-Variable rather than a bare $IsLinux: PowerShell 6+ defines these and
# Windows PowerShell 5.1 does not, and under a session that has Set-StrictMode
# on - which `irm | iex` inherits - reading an undefined variable throws instead
# of yielding $null. Absent means Windows.
$Linux = [bool](Get-Variable -Name IsLinux -ValueOnly -ErrorAction SilentlyContinue)
$Mac   = [bool](Get-Variable -Name IsMacOS -ValueOnly -ErrorAction SilentlyContinue)
if ($Mac) { throw "Caustic has no macOS build yet - Linux and Windows only." }
$Win   = -not $Linux

# The same two places install.ps1 offers, in the order it offers them.
$Places = if ($Linux) { @((Join-Path $HOME ".local"), "/usr/local") }
          else        { @((Join-Path $env:LOCALAPPDATA "caustic"), (Join-Path $env:ProgramFiles "caustic")) }
$ManifestRel = Join-Path (Join-Path "lib" "caustic") "install-manifest"

if (-not $Prefix) {
    foreach ($p in $Places) {
        if (Test-Path (Join-Path $p $ManifestRel)) { $Prefix = $p; break }
    }
    if (-not $Prefix) { throw "no caustic install found (use -Prefix DIR, or install.ps1 for a first install)" }
}

$manifest = Join-Path $Prefix $ManifestRel
if (-not (Test-Path $manifest)) { throw "no manifest at $manifest - run install.ps1 instead" }
$lines = Get-Content $manifest
function Get-Field([string]$k) {
    ($lines | Where-Object { $_ -like "$k=*" } | Select-Object -First 1) -replace "^$k=", ""
}

$mFormat = Get-Field "format"; $mTools = Get-Field "tools"; $mLib = Get-Field "lib"
$mSource = Get-Field "source"; $mSystem = Get-Field "system"; $mRoot = Get-Field "root"

$have = $null
$probe = Join-Path (Join-Path $Prefix "bin") $(if ($Win) { "caustic.exe" } else { "caustic" })
if (Test-Path $probe) { $have = (& $probe --version 2>$null | Select-Object -First 1) -replace '^caustic\s+', '' }

$latest = $null
try {
    # /releases/latest redirects to /releases/tag/vX.Y.Z, so where the request
    # ended up is the cheapest way to ask "what is current" without an API
    # token. PowerShell 6+ exposes that as RequestMessage.RequestUri; Windows
    # PowerShell 5.1 as the older ResponseUri.
    $r = Invoke-WebRequest -Uri "https://github.com/$Repo/releases/latest" -UseBasicParsing
    $loc = $r.BaseResponse.RequestMessage.RequestUri.AbsoluteUri
    if (-not $loc) { $loc = $r.BaseResponse.ResponseUri.AbsoluteUri }
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
    $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("caustic-update-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $tmp | Out-Null
    $installer = Join-Path $tmp "install.ps1"
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/$Repo/main/install.ps1" -OutFile $installer -UseBasicParsing
}

try {
    # A hashtable and not an array: splatting an array passes every element as a
    # *positional* argument, so "-Lib" arrives as a value rather than as the name
    # of the next one. Only hashtable splatting binds by name.
    # -Reinstall so the installer does not stop to ask about the install we are
    # deliberately replacing.
    $opts = @{ Prefix = $Prefix; Format = $mFormat; Tools = $mTools; Lib = $mLib; Reinstall = $true }
    if ($mRoot)           { $opts.Root     = $mRoot }
    if ($mSource -eq "0") { $opts.NoSource = $true }
    if ($mSystem -eq "1") { $opts.System   = $true }
    if ($DryRun)          { $opts.DryRun   = $true }
    # No `exit $LASTEXITCODE` after this: invoking a .ps1 with & does not
    # reliably set it across 5.1 and 7, and a stale value from an earlier native
    # command would be reported as this update's result. Failures throw instead.
    & $installer @opts
}
finally {
    if ($tmp) { Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue }
}
