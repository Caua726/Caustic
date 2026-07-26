# uninstall.ps1 - removes exactly what install.ps1 put down. Works on Windows
# and on Linux.
#
#   .\uninstall.ps1                   remove the install found in the usual places
#   .\uninstall.ps1 -Prefix DIR       remove the install at DIR
#   .\uninstall.ps1 -DryRun           list what would go, remove nothing
#
# A piped one-liner cannot be given parameters, so both of those are also
# environment variables: $env:CAUSTIC_PREFIX, $env:CAUSTIC_DRYRUN.
#
# It reads the manifest install.ps1 wrote, so it deletes the files it created
# and nothing else - important when the prefix is shared, like %ProgramFiles%
# or /usr/local. Removing a machine-wide install needs administrator (Windows,
# where the script relaunches itself elevated) or root (Linux, where it uses
# the same pkexec/sudo/doas the install used).

[CmdletBinding()]
param(
    [string]$Prefix,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

if ($PSVersionTable.PSVersion.Major -lt 5) {
    throw "PowerShell 5.0 or newer is required (this is $($PSVersionTable.PSVersion))."
}

if (-not $Prefix) { $Prefix = $env:CAUSTIC_PREFIX }
if (-not $DryRun) { $DryRun = ($env:CAUSTIC_DRYRUN -eq "1") }

# Get-Variable rather than a bare $IsLinux: PowerShell 6+ defines these and
# Windows PowerShell 5.1 does not, and under a session that has Set-StrictMode
# on - which `irm | iex` inherits - reading an undefined variable throws instead
# of yielding $null. Absent means Windows.
$Linux = [bool](Get-Variable -Name IsLinux -ValueOnly -ErrorAction SilentlyContinue)
$Mac   = [bool](Get-Variable -Name IsMacOS -ValueOnly -ErrorAction SilentlyContinue)
if ($Mac) { throw "Caustic has no macOS build yet - Linux and Windows only." }
$Win   = -not $Linux

$Places = if ($Linux) { @((Join-Path $HOME ".local"), "/usr/local") }
          else        { @((Join-Path $env:LOCALAPPDATA "caustic"), (Join-Path $env:ProgramFiles "caustic")) }
$ManifestRel = Join-Path (Join-Path "lib" "caustic") "install-manifest"

if (-not $Prefix) {
    foreach ($p in $Places) {
        if (Test-Path (Join-Path $p $ManifestRel)) { $Prefix = $p; break }
    }
    if (-not $Prefix) { throw "no caustic install found in $($Places -join ' or ') (use -Prefix DIR)" }
}

$manifest = Join-Path $Prefix $ManifestRel
if (-not (Test-Path $manifest)) { throw "no manifest at $manifest - nothing to uninstall (or it predates manifests)" }

$lines = Get-Content $manifest
function Get-Field([string]$k) {
    ($lines | Where-Object { $_ -like "$k=*" } | Select-Object -First 1) -replace "^$k=", ""
}
$isSystem = (Get-Field "system") -eq "1"
$stdDir   = Get-Field "stddir"
$root     = Get-Field "root"

# --- privilege ---
$Sudo = ""
if ($Win) {
    $IsAdmin = ([Security.Principal.WindowsPrincipal] `
                [Security.Principal.WindowsIdentity]::GetCurrent()
               ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if ($isSystem -and -not $IsAdmin -and -not $DryRun) {
        if (-not $PSCommandPath) { throw "removing a machine-wide install needs administrator" }
        Write-Host "requesting administrator ..."
        $argv = @("-NoProfile","-ExecutionPolicy","Bypass","-File",$PSCommandPath,"-Prefix",$Prefix)
        $p = Start-Process powershell.exe -ArgumentList $argv -Verb RunAs -Wait -PassThru
        exit $p.ExitCode
    }
} elseif (-not $DryRun -and (& id -u).Trim() -ne "0") {
    & test -w $Prefix
    if ($LASTEXITCODE -ne 0) {
        $cands = if ($root -and $root -ne "auto" -and $root -ne "none") { @($root) } else { @("pkexec","sudo","doas") }
        foreach ($m in $cands) { if (Get-Command $m -ErrorAction SilentlyContinue) { $Sudo = $m; break } }
        if (-not $Sudo) { throw "$Prefix needs root and no pkexec/sudo/doas was found" }
        Write-Host "note: $Prefix needs root - using $Sudo"
    }
}

function Remove-Path([string]$p) {
    if ($Linux) {
        # /bin/rm and not `rm`: on Windows PowerShell `rm` is an alias for
        # Remove-Item, and naming the binary keeps this branch unambiguous.
        if ($Sudo) { & $Sudo /bin/rm -rf $p } else { & /bin/rm -rf $p }
        if ($LASTEXITCODE -ne 0) { throw "could not remove $p" }
    } else { Remove-Item -Force -Recurse $p }
}

Write-Host "removing the caustic install at $Prefix"

# Files, from the manifest's `files:` section to the end.
$idx = [array]::IndexOf($lines, "files:")
if ($idx -ge 0 -and $idx -lt ($lines.Count - 1)) {
    foreach ($f in $lines[($idx + 1)..($lines.Count - 1)]) {
        if (-not $f) { continue }
        # A symlink whose target is already gone fails Test-Path, so ask the
        # filesystem about the link itself rather than about what it points at.
        $there = Test-Path $f
        if (-not $there -and $Linux) { & test -L $f; $there = ($LASTEXITCODE -eq 0) }
        if ($there) {
            if ($DryRun) { Write-Host "  would: remove $f" } else { Remove-Path $f }
        } else { Write-Host "  gone already: $f" }
    }
}

if ($stdDir -and (Test-Path $stdDir)) {
    if ($DryRun) { Write-Host "  would: remove $stdDir" } else { Remove-Path $stdDir }
}
if ($DryRun) { Write-Host "  would: remove $manifest" } else { Remove-Path $manifest }

# Only remove the directories if they are now empty - a shared prefix keeps
# whatever else lives there.
foreach ($d in @((Join-Path $Prefix (Join-Path "lib" "caustic")), (Join-Path $Prefix "bin"), (Join-Path $Prefix "lib"), $Prefix)) {
    if ((Test-Path $d) -and -not (Get-ChildItem -Force $d -ErrorAction SilentlyContinue)) {
        if ($DryRun) { Write-Host "  would: remove empty $d" } else { Remove-Path $d }
    }
}

# --- PATH ---
# Windows only: install.ps1 writes the registry PATH there, and on Linux it
# writes nothing to PATH, so there is nothing to undo.
if ($Win -and -not $DryRun) {
    $scope = if ($isSystem) { "Machine" } else { "User" }
    $bin = Join-Path $Prefix "bin"
    $cur = [Environment]::GetEnvironmentVariable("Path", $scope)
    if ($cur -and (($cur -split ';') -contains $bin)) {
        $new = (($cur -split ';') | Where-Object { $_ -ne $bin }) -join ';'
        [Environment]::SetEnvironmentVariable("Path", $new, $scope)
        Write-Host "removed $bin from the $($scope.ToLower()) PATH"
    }
}

Write-Host ""
if ($DryRun) { Write-Host "dry run complete - nothing was removed" }
else { Write-Host "caustic removed from $Prefix" }
