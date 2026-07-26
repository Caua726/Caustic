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
# FROM SOURCE
#   -FromSource [-Ref BRANCH]
#                          clone and build instead of downloading the release.
#                          Needs git; the compiler is written in itself, so a
#                          seed is taken from PATH or from the latest release.
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
    [switch]$FromSource,
    [string]$Ref,
    [switch]$Reinstall,
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
if (-not $Ref)    { $Ref    = if ($env:CAUSTIC_REF)    { $env:CAUSTIC_REF }    else { "main" } }
if (-not $FromSource) { $FromSource = ($env:CAUSTIC_FROMSOURCE -eq "1") }
if (-not $NoPath)   { $NoPath   = ($env:CAUSTIC_NOPATH -eq "1") }
if (-not $NoSource) { $NoSource = ($env:CAUSTIC_NOSOURCE -eq "1") }

if ($env:PROCESSOR_ARCHITECTURE -ne "AMD64" -and $env:PROCESSOR_ARCHITEW6432 -ne "AMD64") {
    Write-Warning "Caustic ships x86_64 (AMD64) binaries; your architecture is $env:PROCESSOR_ARCHITECTURE. The universal build (-Format cse) carries an ARM64 body, but reaching it natively is not wired up yet, so it runs emulated."
}

# --- arrow-key menu ---
# Up/Down move, Enter picks; returns the 1-based index. A host without a raw
# console — a CI log, a redirected stream — cannot read single keys, so it
# falls back to typing the number rather than throwing.
function Show-Menu {
    param([string]$Title, [string[]]$Options)
    Write-Host $Title
    $raw = $true
    try { $null = $Host.UI.RawUI.KeyAvailable } catch { $raw = $false }
    if (-not $raw -or [Console]::IsInputRedirected) {
        for ($i = 0; $i -lt $Options.Count; $i++) { Write-Host ("  {0}) {1}" -f ($i + 1), $Options[$i]) }
        $r = Read-Host "  choice [1-$($Options.Count)]"
        $n = 0
        if ([int]::TryParse($r, [ref]$n) -and $n -ge 1 -and $n -le $Options.Count) { return $n }
        return 1
    }
    $sel = 0
    $first = $true
    while ($true) {
        if (-not $first) { [Console]::SetCursorPosition(0, [Console]::CursorTop - $Options.Count) }
        $first = $false
        for ($i = 0; $i -lt $Options.Count; $i++) {
            $line = if ($i -eq $sel) { "  > " + $Options[$i] } else { "    " + $Options[$i] }
            $w = [Math]::Max(1, [Console]::WindowWidth - 1)
            if ($line.Length -gt $w) { $line = $line.Substring(0, $w) } else { $line = $line.PadRight($w) }
            if ($i -eq $sel) { Write-Host $line -ForegroundColor Cyan } else { Write-Host $line }
        }
        $k = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        switch ($k.VirtualKeyCode) {
            38 { $sel = ($sel - 1 + $Options.Count) % $Options.Count }   # Up
            40 { $sel = ($sel + 1) % $Options.Count }                    # Down
            75 { $sel = ($sel - 1 + $Options.Count) % $Options.Count }   # k
            74 { $sel = ($sel + 1) % $Options.Count }                    # j
            13 { return $sel + 1 }                                       # Enter
        }
    }
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
# A function rather than a block: choosing "change them" at the already-
# installed prompt below needs to ask the same questions, and that prompt
# cannot run until the prefix is known.
function Invoke-Setup {

    Write-Host "=== Caustic ==="

    if ((Show-Menu "Install from" @("the latest release (fast)", "source - clones and builds (needs git)")) -eq 2) {
        $FromSource = $true
        $r = Read-Host "  branch or tag (default main)"
        if ($r) { $Ref = $r }
    }

    switch (Show-Menu "Where" @("just me (%LOCALAPPDATA%)", "everyone (%ProgramFiles%, needs admin)", "somewhere else")) {
        2 { $System = $true; $Prefix = Join-Path $env:ProgramFiles "caustic" }
        3 { $Prefix = Read-Host "  path" }
        default { $System = $false }
    }

    switch (Show-Menu "Compiler" @(
        "native Windows .exe",
        "universal - one file for Windows, Linux and CausticOS, x86_64 and ARM64",
        "both")) {
        2 { $Format = "cse" }
        3 { $Format = "exe,cse" }
        default { $Format = "exe" }
    }

    switch (Show-Menu "Tools alongside the compiler" @("everything (as, ld, mk)", "assembler + linker", "none")) {
        2 { $Tools = "as,ld" }
        3 { $Tools = "none" }
        default { $Tools = "all" }
    }

    switch (Show-Menu "Shared standard library" @("libcaustic.dll", "+ libcaustic.csl (universal)", "just libcaustic.csl", "none")) {
        2 { $Lib = "dll,csl" }
        3 { $Lib = "csl" }
        4 { $Lib = "none" }
        default { $Lib = "dll" }
    }

    $NoSource = ((Show-Menu "Standard library sources (.cst) - you compile against these" @("install them", "skip them")) -eq 2)
}

if ($Custom) { Invoke-Setup }

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

# --- already installed? ---
# Re-running the one-liner used to overwrite an existing install with the
# defaults, quietly discarding whichever compiler and libraries had been chosen.
# The recorded choices carry over unless this run named its own.
$existing = Join-Path $LibDir "install-manifest"
if (Test-Path $existing) {
    $el = Get-Content $existing
    function Old([string]$k) { ($el | Where-Object { $_ -like "$k=*" } | Select-Object -First 1) -replace "^$k=", "" }
    $have = $null
    $exe = Join-Path $Bin "caustic.exe"
    if (Test-Path $exe) { $have = (& $exe --version 2>$null | Select-Object -First 1) -replace '^caustic\s+', '' }
    Write-Host "caustic $(if ($have) { $have } else { '(unknown version)' }) is already installed at $Prefix"
    Write-Host "  compiler: $(Old 'format')   tools: $(Old 'tools')   stdlib: $(Old 'lib')"

    if (-not $PSBoundParameters.ContainsKey('Format') -and (Old 'format')) { $Formats  = @((Old 'format') -split ',' | Where-Object { $_ }) ; $Primary = $Formats[0] }
    if (-not $PSBoundParameters.ContainsKey('Tools')  -and (Old 'tools') -ne 'none') { $ToolList = @((Old 'tools') -split ',' | Where-Object { $_ }) }
    if (-not $PSBoundParameters.ContainsKey('Lib')    -and (Old 'lib')   -ne 'none') { $LibList  = @((Old 'lib')   -split ',' | Where-Object { $_ }) }

    if (-not $Reinstall -and -not $Custom -and -not $DryRun -and [Environment]::UserInteractive) {
        switch (Show-Menu "Already installed - what now?" @("reinstall with the same choices", "reinstall, choosing again", "cancel")) {
            2 {
                # Ask the same questions -Custom asks, then recompute everything
                # derived from the answers — the prefix is among them.
                Invoke-Setup
                if ($Tools -eq "all")  { $Tools = "as,ld,mk" }
                if ($Tools -eq "none") { $Tools = "" }
                if ($Lib   -eq "none") { $Lib = "" }
                $Formats  = @($Format -split ',' | Where-Object { $_ })
                $ToolList = @($Tools  -split ',' | Where-Object { $_ })
                $LibList  = @($Lib    -split ',' | Where-Object { $_ })
                $Primary  = $Formats[0]
                $Bin      = Join-Path $Prefix "bin"
                $LibDir   = Join-Path $Prefix "lib\caustic"
            }
            3 { Write-Host "nothing done"; exit 0 }
        }
    }
}

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
    if ($FromSource) {
        # Caustic compiles itself, so building it needs a compiler to start
        # from: one already on PATH, or the released one downloaded to bootstrap.
        if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw "-FromSource needs git on PATH" }
        $work = Join-Path $tmp "src"
        Write-Host "cloning $Repo ($Ref) ..."
        & git clone --depth 1 --branch $Ref --recurse-submodules "https://github.com/$Repo.git" $work 2>$null | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "clone failed" }

        $seed = $null
        if (Get-Command caustic-mk -ErrorAction SilentlyContinue) { $seed = Split-Path (Get-Command caustic-mk).Source }
        else {
            Write-Host "  no seed compiler - fetching the release to bootstrap with ..."
            Invoke-WebRequest -Uri "https://github.com/$Repo/releases/latest/download/$Zip" `
                              -OutFile (Join-Path $tmp $Zip) -UseBasicParsing
            Expand-Archive -Path (Join-Path $tmp $Zip) -DestinationPath (Join-Path $tmp "seed") -Force
            $seed = Join-Path $tmp "seed\caustic-x86_64-windows\bin"
        }
        $env:PATH = "$seed;$env:PATH"

        Push-Location $work
        try {
            foreach ($t in @("caustic","caustic-as","caustic-ld","caustic-mk")) {
                Write-Host "  building $t"
                & caustic-mk build $t 2>&1 | Out-Null
                if ($LASTEXITCODE -ne 0) { throw "build of $t failed" }
            }
            Write-Host "  packaging"
            & caustic-mk run dist-windows 2>&1 | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "windows packaging failed" }
        } finally { Pop-Location }

        Expand-Archive -Path (Join-Path $work $Zip) -DestinationPath (Join-Path $tmp "built") -Force
        $src = Join-Path $tmp "built\caustic-x86_64-windows"
    }
    elseif ($needZip) {
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
