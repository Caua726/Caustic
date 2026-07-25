# uninstall.ps1 — removes exactly what install.ps1 put down.
#
#   .\uninstall.ps1                   remove the install found in the usual places
#   .\uninstall.ps1 -Prefix DIR       remove the install at DIR
#   .\uninstall.ps1 -DryRun           list what would go, remove nothing
#
# It reads the manifest install.ps1 wrote, so it deletes the files it created
# and nothing else — important when the prefix is shared, like %ProgramFiles%.
# Removing a machine-wide install needs administrator; the script relaunches
# itself elevated if it is not already.

[CmdletBinding()]
param(
    [string]$Prefix,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

if (-not $Prefix) {
    foreach ($p in @((Join-Path $env:LOCALAPPDATA "caustic"), (Join-Path $env:ProgramFiles "caustic"))) {
        if (Test-Path (Join-Path $p "lib\caustic\install-manifest")) { $Prefix = $p; break }
    }
    if (-not $Prefix) { throw "no caustic install found in %LOCALAPPDATA% or %ProgramFiles% (use -Prefix DIR)" }
}

$manifest = Join-Path $Prefix "lib\caustic\install-manifest"
if (-not (Test-Path $manifest)) { throw "no manifest at $manifest - nothing to uninstall (or it predates manifests)" }

$lines = Get-Content $manifest
function Get-Field([string]$k) {
    ($lines | Where-Object { $_ -like "$k=*" } | Select-Object -First 1) -replace "^$k=", ""
}
$isSystem = (Get-Field "system") -eq "1"
$stdDir   = Get-Field "stddir"

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

Write-Host "removing the caustic install at $Prefix"

# Files, from the manifest's `files:` section to the end.
$idx = [array]::IndexOf($lines, "files:")
if ($idx -ge 0) {
    foreach ($f in $lines[($idx + 1)..($lines.Count - 1)]) {
        if (-not $f) { continue }
        if (Test-Path $f) {
            if ($DryRun) { Write-Host "  would: remove $f" }
            else { Remove-Item -Force -Recurse $f }
        } else { Write-Host "  gone already: $f" }
    }
}

if ($stdDir -and (Test-Path $stdDir)) {
    if ($DryRun) { Write-Host "  would: remove $stdDir" } else { Remove-Item -Recurse -Force $stdDir }
}
if ($DryRun) { Write-Host "  would: remove $manifest" } else { Remove-Item -Force $manifest }

# Only remove the directories if they are now empty — a shared prefix keeps
# whatever else lives there.
foreach ($d in @((Join-Path $Prefix "lib\caustic"), (Join-Path $Prefix "bin"), (Join-Path $Prefix "lib"), $Prefix)) {
    if ((Test-Path $d) -and -not (Get-ChildItem -Force $d -ErrorAction SilentlyContinue)) {
        if ($DryRun) { Write-Host "  would: remove empty $d" } else { Remove-Item -Force $d }
    }
}

# PATH
if (-not $DryRun) {
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
