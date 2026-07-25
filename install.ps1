# install.ps1 — Caustic installer for Windows (PowerShell).
#
# One-liner (defaults: native PE toolchain + stdlib into %LOCALAPPDATA%\caustic):
#   irm https://raw.githubusercontent.com/Caua726/Caustic/main/install.ps1 | iex
#
# Pick everything interactively:
#   iwr -useb .../install.ps1 -OutFile i.ps1; .\i.ps1 -Custom
#
# WHERE
#   -Prefix DIR            install root (default: %LOCALAPPDATA%\caustic)
#   -System                install for every user under %ProgramFiles%\caustic and
#                          put it on the machine PATH. Needs administrator; the
#                          script relaunches itself elevated if it is not already.
#   -NoPath                do not touch PATH at all
#
# WHAT COMPILER            -Format LIST   (comma-separated; the first one also
#                                          answers to plain `caustic.exe`)
#   exe                    native Windows PE (default)
#   cse                    universal caustic-universal.cse.exe — one file that
#                          also runs on Linux and CausticOS, x86_64 and ARM64
#
# WHICH TOOLS              -Tools LIST    (as,ld,mk | all | none; default all —
#                                          the windows archive ships them)
#
# WHICH STDLIB             -Lib LIST      (dll,csl | none; default dll)
#   dll                    libcaustic.dll — dynamic stdlib for Windows
#   csl                    libcaustic.csl — universal, loaded by std/csl_loader
#   -NoSource              skip the stdlib .cst sources (they are what you
#                          compile against; only skip if you know why)
#
# OTHER
#   -DryRun                print what would happen, touch nothing
#
# Env-var equivalents, for the piped one-liner where parameters cannot be passed:
#   $env:CAUSTIC_PREFIX  $env:CAUSTIC_NOPATH  $env:CAUSTIC_FORMAT
#   $env:CAUSTIC_TOOLS   $env:CAUSTIC_LIB     $env:CAUSTIC_NOSOURCE
#   $env:CAUSTIC_SYSTEM

[CmdletBinding()]
param(
    [string]$Prefix,
    [string]$Format,
    [string]$Tools,
    [string]$Lib,
    [switch]$System,
    [switch]$NoPath,
    [switch]$NoSource,
    [switch]$DryRun,
    [switch]$Custom
)

$ErrorActionPreference = "Stop"

$Repo      = "Caua726/Caustic"
$Zip       = "caustic-x86_64-windows.zip"
$Tarball   = "caustic-x86_64-linux.tar.gz"
$Universal = "caustic-universal.cse.exe"

# Parameters win; environment variables fill the gaps for the piped one-liner,
# where PowerShell cannot pass arguments through `irm ... | iex`.
if (-not $System) { $System = ($env:CAUSTIC_SYSTEM -eq "1") }
if (-not $Prefix) {
    $Prefix = if ($env:CAUSTIC_PREFIX) { $env:CAUSTIC_PREFIX }
              elseif ($System)         { Join-Path $env:ProgramFiles "caustic" }
              else                     { Join-Path $env:LOCALAPPDATA "caustic" }
}
if (-not $Format) { $Format = if ($env:CAUSTIC_FORMAT) { $env:CAUSTIC_FORMAT } else { "exe" } }
if (-not $Tools)  { $Tools  = if ($env:CAUSTIC_TOOLS)  { $env:CAUSTIC_TOOLS }  else { "all" } }
if (-not $Lib)    { $Lib    = if ($env:CAUSTIC_LIB)    { $env:CAUSTIC_LIB }    else { "dll" } }
if (-not $NoPath)   { $NoPath   = ($env:CAUSTIC_NOPATH -eq "1") }
if (-not $NoSource) { $NoSource = ($env:CAUSTIC_NOSOURCE -eq "1") }

if ($env:PROCESSOR_ARCHITECTURE -ne "AMD64" -and $env:PROCESSOR_ARCHITEW6432 -ne "AMD64") {
    Write-Warning "Caustic ships x86_64 (AMD64) binaries; your architecture is $env:PROCESSOR_ARCHITECTURE. The universal build (-Format cse) carries an ARM64 body, but reaching it natively is not wired up yet, so it runs emulated."
}

# --- elevation ---
# A machine-wide prefix and the machine PATH both need administrator. Rather
# than failing halfway through with a copy denied, relaunch the whole script
# elevated and let this instance exit.
$IsAdmin = ([Security.Principal.WindowsPrincipal] `
            [Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($System -and -not $IsAdmin -and -not $DryRun) {
    if (-not $PSCommandPath) {
        throw "-System needs administrator. Save this script to a file and run it again, or start an elevated PowerShell — a piped one-liner cannot relaunch itself."
    }
    Write-Host "requesting administrator ..."
    $argv = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath, "-System")
    if ($Prefix)   { $argv += @("-Prefix", $Prefix) }
    if ($Format)   { $argv += @("-Format", $Format) }
    if ($Tools)    { $argv += @("-Tools", $Tools) }
    if ($Lib)      { $argv += @("-Lib", $Lib) }
    if ($NoPath)   { $argv += "-NoPath" }
    if ($NoSource) { $argv += "-NoSource" }
    $p = Start-Process -FilePath "powershell.exe" -ArgumentList $argv -Verb RunAs -Wait -PassThru
    exit $p.ExitCode
}

# --- interactive ---
if ($Custom) {
    function Ask([string]$q, [string]$def) {
        $r = Read-Host $q
        if ([string]::IsNullOrWhiteSpace($r)) { return $def } else { return $r }
    }
    Write-Host "=== Caustic custom install ==="
    switch (Ask "Scope - [1] this user (%LOCALAPPDATA%)  [2] every user (%ProgramFiles%, needs admin) (default 1)" "1") {
        "2" { $System = $true; $Prefix = Join-Path $env:ProgramFiles "caustic" }
        default { $System = $false }
    }
    $Prefix = Ask "Prefix (default $Prefix)" $Prefix
    switch (Ask "Compiler - [1] exe (native Windows)  [2] cse (universal: Windows+Linux+CausticOS, x86_64+ARM64)  [3] both (default 1)" "1") {
        "2" { $Format = "cse" }
        "3" { $Format = "exe,cse" }
        default { $Format = "exe" }
    }
    switch (Ask "Tools - [1] all (as, ld, mk)  [2] as + ld  [3] none (default 1)" "1") {
        "2" { $Tools = "as,ld" }
        "3" { $Tools = "none" }
        default { $Tools = "all" }
    }
    switch (Ask "Shared stdlib - [1] libcaustic.dll  [2] .dll + .csl  [3] .csl only  [4] none (default 1)" "1") {
        "2" { $Lib = "dll,csl" }
        "3" { $Lib = "csl" }
        "4" { $Lib = "none" }
        default { $Lib = "dll" }
    }
    $NoSource = ((Ask "Install the stdlib sources (.cst)? Required to compile anything [Y/n]" "Y") -match "^[Nn]")
}

if ($Tools -eq "all")  { $Tools = "as,ld,mk" }
if ($Tools -eq "none") { $Tools = "" }
if ($Lib   -eq "none") { $Lib = "" }

$Formats  = @($Format -split ',' | Where-Object { $_ })
$ToolList = @($Tools  -split ',' | Where-Object { $_ })
$LibList  = @($Lib    -split ',' | Where-Object { $_ })
foreach ($f in $Formats) { if ($f -notin @("exe","cse")) { throw "unknown -Format '$f' (exe|cse)" } }
foreach ($l in $LibList) { if ($l -notin @("dll","csl")) { throw "unknown -Lib '$l' (dll|csl|none)" } }
$Primary = $Formats[0]

$Bin    = Join-Path $Prefix "bin"
$LibDir = Join-Path $Prefix "lib\caustic"

Write-Host "install plan"
Write-Host "  prefix:   $Prefix$(if ($System) { '  (all users, elevated)' })"
Write-Host "  compiler: $($Formats -join ',')   (caustic.exe -> $Primary)"
Write-Host "  tools:    $(if ($ToolList) { $ToolList -join ',' } else { 'none' })"
Write-Host "  stdlib:   $(if ($LibList) { $LibList -join ',' } else { 'no shared lib' })$(if (-not $NoSource) { ' + sources' })"
if ($DryRun) { Write-Host "  (dry run - nothing will be written)" }

# Every installed path is recorded so uninstall removes exactly what was put
# there and nothing else — important when the prefix is shared, like
# %ProgramFiles%.
$Installed = New-Object System.Collections.ArrayList
function Track([string]$p) { [void]$Installed.Add($p) }

function Put([string]$from, [string]$to) {
    if ($DryRun) { Write-Host "  would: copy $(Split-Path -Leaf $from) -> $to" }
    else {
        Copy-Item -Path $from -Destination $to -Force -Recurse
        if (Test-Path -PathType Container $to) { Track (Join-Path $to (Split-Path -Leaf $from)) }
        else { Track $to }
    }
}

$tmp = Join-Path $env:TEMP ("caustic-install-" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
try {
    # The windows archive carries the stdlib sources, the native tools and the
    # .dll, so it is fetched unless the install is a bare universal compiler.
    $src = $null
    $needZip = ($Primary -eq "exe") -or $ToolList -or ("dll" -in $LibList) -or (-not $NoSource)
    if ($needZip) {
        Write-Host "downloading $Zip ..."
        Invoke-WebRequest -Uri "https://github.com/$Repo/releases/latest/download/$Zip" `
                          -OutFile (Join-Path $tmp $Zip) -UseBasicParsing
        Expand-Archive -Path (Join-Path $tmp $Zip) -DestinationPath $tmp -Force
        $src = Join-Path $tmp "caustic-x86_64-windows"
    }

    if (-not $DryRun) { New-Item -ItemType Directory -Force -Path $Bin, $LibDir | Out-Null }

    # --- compiler(s) ---
    $primaryName = $null
    foreach ($f in $Formats) {
        switch ($f) {
            "exe" {
                Put (Join-Path $src "bin\caustic.exe") (Join-Path $Bin "caustic-native.exe")
                if ($f -eq $Primary) { $primaryName = "caustic-native.exe" }
            }
            "cse" {
                # One file carrying a native body per OS and architecture. Windows
                # runs it because offset 0 is an MZ header, which is also why the
                # name keeps its .exe.
                if ($DryRun) { Write-Host "  would: download $Universal -> $Bin" }
                else {
                    Write-Host "downloading $Universal ..."
                    Invoke-WebRequest -Uri "https://github.com/$Repo/releases/latest/download/$Universal" `
                                      -OutFile (Join-Path $Bin $Universal) -UseBasicParsing
                    Track (Join-Path $Bin $Universal)
                }
                if ($f -eq $Primary) { $primaryName = $Universal }
            }
        }
    }
    # Windows has no symlink worth relying on without elevation, so the primary
    # is copied to the name you type rather than linked to it.
    if ($primaryName -and -not $DryRun) {
        Copy-Item (Join-Path $Bin $primaryName) (Join-Path $Bin "caustic.exe") -Force
        Track (Join-Path $Bin "caustic.exe")
    } elseif ($primaryName) {
        Write-Host "  would: copy $primaryName -> caustic.exe"
    }

    # --- tools ---
    foreach ($t in $ToolList) {
        $p = Join-Path $src "bin\caustic-$t.exe"
        if (Test-Path $p) { Put $p $Bin }
        else { Write-Host "  note: caustic-$t.exe is not in this release, skipped" }
    }

    # --- stdlib ---
    if (-not $NoSource) { Put (Join-Path $src "lib\caustic\std") $LibDir }
    foreach ($l in $LibList) {
        $p = if ($src) { Join-Path $src "lib\caustic\libcaustic.$l" } else { $null }
        if ($p -and (Test-Path $p)) { Put $p $LibDir }
        elseif ($l -eq "csl") {
            # The .csl is universal and ships in the Linux archive, so a Windows
            # install that wants it reaches over for that one file.
            if ($DryRun) { Write-Host "  would: fetch libcaustic.csl from $Tarball" }
            else {
                Write-Host "  fetching libcaustic.csl from the linux archive ..."
                Invoke-WebRequest -Uri "https://github.com/$Repo/releases/latest/download/$Tarball" `
                                  -OutFile (Join-Path $tmp $Tarball) -UseBasicParsing
                tar -xzf (Join-Path $tmp $Tarball) -C $tmp
                $c = Join-Path $tmp "caustic-x86_64-linux\lib\caustic\libcaustic.csl"
                if (Test-Path $c) { Copy-Item $c $LibDir -Force; Track (Join-Path $LibDir "libcaustic.csl") }
                else { Write-Host "  note: libcaustic.csl is not in this release, skipped" }
            }
        }
        else { Write-Host "  note: libcaustic.$l is not in this release, skipped" }
    }

    # --- PATH ---
    if (-not $NoPath -and -not $DryRun) {
        # A machine-wide install belongs on the machine PATH; a per-user one on
        # the user PATH. Writing the machine scope is why -System elevates.
        $scope = if ($System) { "Machine" } else { "User" }
        $cur = [Environment]::GetEnvironmentVariable("Path", $scope)
        if (($cur -split ';') -notcontains $Bin) {
            $newPath = if ([string]::IsNullOrEmpty($cur)) { $Bin } else { "$cur;$Bin" }
            [Environment]::SetEnvironmentVariable("Path", $newPath, $scope)
            Write-Host "added $Bin to the $($scope.ToLower()) PATH (restart your terminal to pick it up)"
        }
    }

    # --- manifest ---
    # update.ps1 replays these choices; uninstall.ps1 removes these paths.
    # Written last so a failed install leaves no manifest claiming success.
    if (-not $DryRun) {
        $mf = @(
            "# caustic install manifest - written by install.ps1, read by update.ps1 and uninstall.ps1",
            "prefix=$Prefix",
            "format=$($Formats -join ',')",
            "tools=$(if ($ToolList) { $ToolList -join ',' } else { 'none' })",
            "lib=$(if ($LibList) { $LibList -join ',' } else { 'none' })",
            "source=$(if ($NoSource) { '0' } else { '1' })",
            "system=$(if ($System) { '1' } else { '0' })"
        )
        if (-not $NoSource) { $mf += "stddir=$(Join-Path $LibDir 'std')" }
        $mf += "files:"
        $mf += $Installed
        Set-Content -Path (Join-Path $LibDir "install-manifest") -Value $mf -Encoding UTF8
    }

    Write-Host ""
    if ($DryRun) { Write-Host "dry run complete - nothing was written" }
    else {
        Write-Host "caustic installed -> $Bin\caustic.exe (from $primaryName)"
        Write-Host "try:  caustic --version"
    }
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
