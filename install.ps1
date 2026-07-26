# install.ps1 - Caustic installer for PowerShell, on Windows and on Linux.
#
# One-liner (defaults: the native compiler + the stdlib, just for you):
#   irm https://raw.githubusercontent.com/Caua726/Caustic/main/install.ps1 | iex
#
# Pick everything interactively (arrow keys):
#   $env:CAUSTIC_CUSTOM=1; irm https://raw.githubusercontent.com/Caua726/Caustic/main/install.ps1 | iex
#
# WHERE
#   -Prefix DIR            install root
#                          default: %LOCALAPPDATA%\caustic  |  $HOME/.local
#   -System                install for everyone
#                          %ProgramFiles%\caustic (elevates through UAC)
#                          /usr/local             (elevates, see -Root)
#   -Root METHOD           Linux only: pkexec | sudo | doas | none | auto
#                          (default auto - the first one found)
#   -NoPath                Windows only: do not touch PATH. On Linux nothing is
#                          written to PATH either way; you get told what to add.
#
# WHAT COMPILER            -Format LIST   (comma-separated; the first one also
#                                          answers to plain `caustic`)
#   elf                    native x86_64 Linux ELF          (default on Linux)
#   exe                    native Windows PE                (default on Windows)
#   cse                    universal caustic-universal.cse.exe - one file that
#                          runs on Linux, Windows and CausticOS, x86_64 and ARM64
#   The one that is not native to this machine still installs - that is how you
#   get a cross-compiler - but it never becomes the plain `caustic`.
#
# WHICH TOOLS              -Tools LIST    (as,ld,mk,lsp | all | none; default
#                                          none - the compiler already embeds an
#                                          assembler and a linker. lsp is not in
#                                          the Windows archive.)
#
# WHICH STDLIB             -Lib LIST      (so,csl,dll | none)
#   so                     libcaustic.so  - Linux            (default on Linux)
#   dll                    libcaustic.dll - Windows          (default on Windows)
#   csl                    libcaustic.csl - universal, loaded by std/csl_loader
#   The non-native ones are what you link against when cross-compiling with
#   -lcaustic.
#   -NoSource              skip the stdlib .cst sources (they are what you
#                          compile against; only skip if you know why)
#
# FROM SOURCE
#   -FromSource [-SourceDir DIR] [-Ref BRANCH]
#                          build instead of downloading the release. Without
#                          -SourceDir it clones (needs git); the compiler is
#                          written in itself, so a seed is taken from the
#                          checkout, from PATH, or from the latest release.
#
# OTHER
#   -Custom                ask for everything instead of taking the defaults
#   -Reinstall             skip the "already installed" prompt and just do it
#   -DryRun                print the plan; touch neither the disk nor the network
#   -Help                  print this and exit
#
# Env-var equivalents, for the piped one-liner where parameters cannot be passed:
#   $env:CAUSTIC_PREFIX  $env:CAUSTIC_NOPATH   $env:CAUSTIC_FORMAT
#   $env:CAUSTIC_TOOLS   $env:CAUSTIC_LIB      $env:CAUSTIC_NOSOURCE
#   $env:CAUSTIC_SYSTEM  $env:CAUSTIC_CUSTOM   $env:CAUSTIC_FROMSOURCE
#   $env:CAUSTIC_REF     $env:CAUSTIC_ROOT     $env:CAUSTIC_DRYRUN

[CmdletBinding()]
param(
    [string]$Prefix,
    # [string[]] and not [string]: PowerShell parses `-Lib so,csl` on the command
    # line as an *array*, so a [string] parameter refuses to bind it and every
    # comma-separated example above fails. They are joined back into one string
    # below, which is the shape the rest of the script reads.
    [string[]]$Format,
    [string[]]$Tools,
    [string[]]$Lib,
    [string]$Root,
    [switch]$System,
    [switch]$NoPath,
    [switch]$NoSource,
    [switch]$FromSource,
    [string]$SourceDir,
    [string]$Ref,
    [switch]$Reinstall,
    [switch]$DryRun,
    [switch]$Custom,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

# --- Windows PowerShell 5.1 ---
# Everything here is a no-op on PowerShell 7 and load-bearing on 5.1.
if ($PSVersionTable.PSVersion.Major -lt 5) {
    throw "PowerShell 5.0 or newer is required (this is $($PSVersionTable.PSVersion)). Windows 10 and Server 2016 ship 5.1."
}
# 5.1 negotiates TLS 1.0 by default and GitHub has refused it since 2018, so
# every download dies with "could not create SSL/TLS secure channel" without
# this. 7 already follows the system default.
try {
    [Net.ServicePointManager]::SecurityProtocol =
        [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
} catch { }
# Invoke-WebRequest's progress bar costs 5.1 more time than the transfer does;
# on the 36 MB universal build the difference is minutes.
$ProgressPreference = 'SilentlyContinue'

$Repo      = "Caua726/Caustic"
$Zip       = "caustic-x86_64-windows.zip"
$Tarball   = "caustic-x86_64-linux.tar.gz"
$Universal = "caustic-universal.cse.exe"
$WinDir    = "caustic-x86_64-windows"
$LnxDir    = "caustic-x86_64-linux"

if ($Help) {
    # `foreach` rather than the pipeline: `break` has to leave the loop, and
    # inside ForEach-Object it would leave the script instead.
    if ($PSCommandPath) {
        foreach ($line in (Get-Content $PSCommandPath)) {
            if ($line -notmatch '^#') { break }
            Write-Host ($line -replace '^# ?', '')
        }
    } else { Write-Host "-Help needs the script on disk; it cannot read itself out of a pipe." }
    exit 0
}

# --- which machine is this ---
# Get-Variable rather than a bare $IsLinux: PowerShell 6+ defines these and
# Windows PowerShell 5.1 does not, and under a session that has Set-StrictMode
# on - which `irm | iex` inherits - reading an undefined variable throws instead
# of yielding $null. Absent means Windows.
$Linux = [bool](Get-Variable -Name IsLinux -ValueOnly -ErrorAction SilentlyContinue)
$Mac   = [bool](Get-Variable -Name IsMacOS -ValueOnly -ErrorAction SilentlyContinue)
if ($Mac) { throw "Caustic has no macOS build yet - Linux and Windows only." }
$Win   = -not $Linux

if ($Linux) {
    $arch = (& uname -m).Trim()
    if ($arch -ne "x86_64") { throw "unsupported architecture '$arch' (x86_64 only)" }
    foreach ($need in @("tar", "cp", "mkdir", "chmod", "ln")) {
        if (-not (Get-Command $need -ErrorAction SilentlyContinue)) { throw "'$need' is required" }
    }
} elseif ($env:PROCESSOR_ARCHITECTURE -ne "AMD64" -and $env:PROCESSOR_ARCHITEW6432 -ne "AMD64") {
    Write-Warning "Caustic ships x86_64 (AMD64) binaries; your architecture is $env:PROCESSOR_ARCHITECTURE. The universal build (-Format cse) carries an ARM64 body, but reaching it natively is not wired up yet, so it runs emulated."
}

# What this machine can actually execute. A format outside this set still
# installs - a cross-compiler is a reasonable thing to want - but it cannot be
# the file `caustic` resolves to.
$Runnable = if ($Linux) { @("elf", "cse") } else { @("exe", "cse") }
$Native   = if ($Linux) { "elf" } else { "exe" }
$NativeLib = if ($Linux) { "so" } else { "dll" }

# --- defaults ---
# Parameters win; environment variables fill the gaps for the piped one-liner,
# where PowerShell cannot pass arguments through `irm ... | iex`.
if (-not $System) { $System = ($env:CAUSTIC_SYSTEM -eq "1") }
if (-not $Custom) { $Custom = ($env:CAUSTIC_CUSTOM -eq "1") }
if (-not $DryRun) { $DryRun = ($env:CAUSTIC_DRYRUN -eq "1") }
if (-not $Prefix) {
    $Prefix = if ($env:CAUSTIC_PREFIX)  { $env:CAUSTIC_PREFIX }
              elseif ($System -and $Linux) { "/usr/local" }
              elseif ($System)          { Join-Path $env:ProgramFiles "caustic" }
              elseif ($Linux)           { Join-Path $HOME ".local" }
              else                      { Join-Path $env:LOCALAPPDATA "caustic" }
}

# Whether a choice was named *at all* - by parameter or by environment variable.
# The already-installed branch keeps the recorded value for anything this run
# did not name, and `irm | iex` can only name things through the environment.
$SetFormat = $PSBoundParameters.ContainsKey('Format') -or [bool]$env:CAUSTIC_FORMAT
$SetTools  = $PSBoundParameters.ContainsKey('Tools')  -or [bool]$env:CAUSTIC_TOOLS
$SetLib    = $PSBoundParameters.ContainsKey('Lib')    -or [bool]$env:CAUSTIC_LIB

$FmtSpec  = if ($Format) { $Format -join ',' } elseif ($env:CAUSTIC_FORMAT) { $env:CAUSTIC_FORMAT } else { $Native }
$ToolSpec = if ($Tools)  { $Tools  -join ',' } elseif ($env:CAUSTIC_TOOLS)  { $env:CAUSTIC_TOOLS }  else { "none" }
$LibSpec  = if ($Lib)    { $Lib    -join ',' } elseif ($env:CAUSTIC_LIB)    { $env:CAUSTIC_LIB }    else { $NativeLib }
if (-not $Root)   { $Root   = if ($env:CAUSTIC_ROOT)   { $env:CAUSTIC_ROOT }   else { "auto" } }
if (-not $Ref)    { $Ref    = if ($env:CAUSTIC_REF)    { $env:CAUSTIC_REF }    else { "main" } }
if (-not $FromSource) { $FromSource = ($env:CAUSTIC_FROMSOURCE -eq "1") }
if (-not $NoPath)     { $NoPath     = ($env:CAUSTIC_NOPATH -eq "1") }
if (-not $NoSource)   { $NoSource   = ($env:CAUSTIC_NOSOURCE -eq "1") }
if ($SourceDir) { $FromSource = $true }

# --- arrow-key menu ---
# Up/Down move, Enter picks; returns the 1-based index. A host without a raw
# console - a CI log, a redirected stream - cannot read single keys, so it
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

# --- interactive ---
# A function rather than a block: choosing "reinstall, choosing again" at the
# already-installed prompt below has to ask the same questions, and that prompt
# cannot run until the prefix is known.
#
# Every assignment is $script: on purpose. A bare $Format = ... inside a
# function creates a *function-local* variable and the answer is lost the
# moment the function returns - reads see the parent scope, writes do not
# reach it.
function Invoke-Setup {
    Write-Host "=== Caustic ==="

    if ((Show-Menu "Install from" @("the latest release (fast)", "source - clones and builds (needs git)")) -eq 2) {
        $script:FromSource = $true
        $r = Read-Host "  branch or tag (default main)"
        if ($r) { $script:Ref = $r }
    } else { $script:FromSource = $false }

    $mine = if ($Linux) { "$HOME/.local - just me, no root" } else { "just me (%LOCALAPPDATA%)" }
    $all  = if ($Linux) { "/usr/local - everyone, needs root" } else { "everyone (%ProgramFiles%, needs admin)" }
    switch (Show-Menu "Where" @($mine, $all, "somewhere else")) {
        2 { $script:System = $true
            $script:Prefix = if ($Linux) { "/usr/local" } else { Join-Path $env:ProgramFiles "caustic" } }
        3 { $script:Prefix = Read-Host "  path" }
        default { $script:System = $false
                  $script:Prefix = if ($Linux) { Join-Path $HOME ".local" } else { Join-Path $env:LOCALAPPDATA "caustic" } }
    }

    if ($Linux -and -not $script:Prefix.StartsWith($HOME)) {
        switch (Show-Menu "Become root with" @("whatever is available", "pkexec", "sudo", "doas")) {
            2 { $script:Root = "pkexec" }
            3 { $script:Root = "sudo" }
            4 { $script:Root = "doas" }
            default { $script:Root = "auto" }
        }
    }

    $universal = "universal - one file for Linux, Windows and CausticOS, x86_64 and ARM64"
    if ($Linux) {
        switch (Show-Menu "Compiler" @("native Linux binary", $universal, "Windows .exe", "native + universal")) {
            2 { $script:FmtSpec = "cse" }
            3 { $script:FmtSpec = "exe" }
            4 { $script:FmtSpec = "elf,cse" }
            default { $script:FmtSpec = "elf" }
        }
    } else {
        switch (Show-Menu "Compiler" @("native Windows .exe", $universal, "Linux ELF", "native + universal")) {
            2 { $script:FmtSpec = "cse" }
            3 { $script:FmtSpec = "elf" }
            4 { $script:FmtSpec = "exe,cse" }
            default { $script:FmtSpec = "exe" }
        }
    }

    $every = if ($Linux) { "everything (as, ld, mk, lsp)" } else { "everything (as, ld, mk)" }
    switch (Show-Menu "Tools alongside the compiler" @("none", "assembler + linker", $every)) {
        2 { $script:ToolSpec = "as,ld" }
        3 { $script:ToolSpec = "all" }
        default { $script:ToolSpec = "none" }
    }

    if ($Linux) {
        switch (Show-Menu "Shared standard library" @("libcaustic.so", "+ libcaustic.csl (universal)", "all three, with the Windows .dll", "none")) {
            2 { $script:LibSpec = "so,csl" }
            3 { $script:LibSpec = "so,csl,dll" }
            4 { $script:LibSpec = "none" }
            default { $script:LibSpec = "so" }
        }
    } else {
        switch (Show-Menu "Shared standard library" @("libcaustic.dll", "+ libcaustic.csl (universal)", "all three, with the Linux .so", "none")) {
            2 { $script:LibSpec = "dll,csl" }
            3 { $script:LibSpec = "dll,csl,so" }
            4 { $script:LibSpec = "none" }
            default { $script:LibSpec = "dll" }
        }
    }

    $script:NoSource = ((Show-Menu "Standard library sources (.cst) - you compile against these" @("install them", "skip them")) -eq 2)
}

# Turns the free-form answers into the shapes the rest of the script reads.
# Called again after the already-installed prompt re-asks, because the prefix is
# one of the answers and everything below is derived from it.
function Resolve-Choices {
    if ($script:ToolSpec -eq "all")  { $script:ToolSpec = if ($Linux) { "as,ld,mk,lsp" } else { "as,ld,mk" } }
    if ($script:ToolSpec -eq "none") { $script:ToolSpec = "" }
    if ($script:LibSpec  -eq "none") { $script:LibSpec = "" }

    $script:Formats  = @($script:FmtSpec  -split ',' | Where-Object { $_ })
    $script:ToolList = @($script:ToolSpec -split ',' | Where-Object { $_ })
    $script:LibList  = @($script:LibSpec  -split ',' | Where-Object { $_ })
    if (-not $script:Formats) { throw "no -Format given" }
    foreach ($f in $script:Formats) { if ($f -notin @("elf","exe","cse")) { throw "unknown -Format '$f' (elf|exe|cse)" } }
    foreach ($l in $script:LibList) { if ($l -notin @("so","csl","dll")) { throw "unknown -Lib '$l' (so|csl|dll|none)" } }
    $script:Primary = $script:Formats[0]
    $script:Bin     = Join-Path $script:Prefix "bin"
    $script:LibDir  = Join-Path $script:Prefix (Join-Path "lib" "caustic")
    $script:StdDir  = Join-Path $script:LibDir "std"
}

if ($Custom) { Invoke-Setup }
Resolve-Choices

# --- already installed? ---
# Re-running the one-liner used to overwrite an existing install with the
# defaults, quietly discarding whichever compiler and libraries had been chosen.
# The recorded choices carry over unless this run named its own.
$existing = Join-Path $LibDir "install-manifest"
if (Test-Path $existing) {
    $el = Get-Content $existing
    function Old([string]$k) { ($el | Where-Object { $_ -like "$k=*" } | Select-Object -First 1) -replace "^$k=", "" }
    $have = $null
    $probe = Join-Path $Bin $(if ($Win) { "caustic.exe" } else { "caustic" })
    if (Test-Path $probe) { $have = (& $probe --version 2>$null | Select-Object -First 1) -replace '^caustic\s+', '' }
    Write-Host "caustic $(if ($have) { $have } else { '(unknown version)' }) is already installed at $Prefix"
    Write-Host "  compiler: $(Old 'format')   tools: $(Old 'tools')   stdlib: $(Old 'lib')"

    if (-not $SetFormat -and (Old 'format'))                              { $FmtSpec  = Old 'format' }
    if (-not $SetTools  -and (Old 'tools') -and (Old 'tools') -ne 'none') { $ToolSpec = Old 'tools' }
    if (-not $SetLib    -and (Old 'lib')   -and (Old 'lib')   -ne 'none') { $LibSpec  = Old 'lib' }
    Resolve-Choices

    if (-not $Reinstall -and -not $Custom -and -not $DryRun) {
        switch (Show-Menu "Already installed - what now?" @("reinstall with the same choices", "reinstall, choosing again", "cancel")) {
            2 { Invoke-Setup; Resolve-Choices }
            3 { Write-Host "nothing done"; exit 0 }
        }
    }
}

# --- privilege ---
# After the questions, not before: "everyone" is one of the answers, and asking
# for root before it is given means a machine-wide install fails halfway through
# with a copy denied.
$Sudo = ""
if ($Win) {
    $IsAdmin = ([Security.Principal.WindowsPrincipal] `
                [Security.Principal.WindowsIdentity]::GetCurrent()
               ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if ($System -and -not $IsAdmin -and -not $DryRun) {
        if (-not $PSCommandPath) {
            throw "-System needs administrator. Save this script to a file and run it again, or start an elevated PowerShell - a piped one-liner cannot relaunch itself."
        }
        Write-Host "requesting administrator ..."
        # The answers travel with it: the elevated instance must not ask again,
        # hence -Reinstall and the absence of -Custom.
        $argv = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath, "-System", "-Reinstall")
        $argv += @("-Prefix", $Prefix, "-Format", ($Formats -join ','))
        $argv += @("-Tools", $(if ($ToolList) { $ToolList -join ',' } else { "none" }))
        $argv += @("-Lib",   $(if ($LibList)  { $LibList  -join ',' } else { "none" }))
        if ($NoPath)     { $argv += "-NoPath" }
        if ($NoSource)   { $argv += "-NoSource" }
        if ($FromSource) { $argv += @("-FromSource", "-Ref", $Ref) }
        if ($SourceDir)  { $argv += @("-SourceDir", $SourceDir) }
        $p = Start-Process -FilePath "powershell.exe" -ArgumentList $argv -Verb RunAs -Wait -PassThru
        exit $p.ExitCode
    }
} elseif ((& id -u).Trim() -ne "0") {
    # Walk up to the nearest directory that exists - a prefix two levels deep
    # tells you nothing about permissions until you find its living ancestor.
    $parent = $Prefix
    while (-not (Test-Path $parent) -and $parent -ne "/" -and $parent) { $parent = Split-Path -Parent $parent }
    & test -w $parent
    if ($LASTEXITCODE -ne 0) {
        switch ($Root) {
            "none" { throw "$Prefix needs root and -Root none was given" }
            "auto" {
                foreach ($m in @("pkexec", "sudo", "doas")) {
                    if (Get-Command $m -ErrorAction SilentlyContinue) { $Sudo = $m; break }
                }
                if (-not $Sudo) { throw "$Prefix needs root and no pkexec/sudo/doas was found (try -Prefix $HOME/.local)" }
            }
            default {
                if (-not (Get-Command $Root -ErrorAction SilentlyContinue)) { throw "-Root $Root is not on PATH" }
                $Sudo = $Root
            }
        }
        Write-Host "note: $Prefix needs root - using $Sudo"
    }
}

Write-Host "install plan"
Write-Host "  prefix:   $Prefix$(if ($System -and $Win) { '  (all users, elevated)' } elseif ($Sudo) { "  (via $Sudo)" })"
Write-Host "  compiler: $($Formats -join ',')   (caustic -> $Primary)"
Write-Host "  tools:    $(if ($ToolList) { $ToolList -join ',' } else { 'none' })"
Write-Host "  stdlib:   $(if ($LibList) { $LibList -join ',' } else { 'no shared lib' })$(if (-not $NoSource) { ' + sources' })"

# A dry run answers "what would you do", so it must not reach the network or
# unpack anything - it prints the plan and stops.
if ($DryRun) { Write-Host "dry run - no download, nothing written"; exit 0 }

# --- file operations ---
# On Linux the writes may need root, and root here means prefixing an external
# command, so every mutation goes through the same external tools whether or not
# it is elevated. On Windows they are cmdlets.
function Invoke-Priv([string]$exe, [string[]]$argv) {
    if ($Sudo) { & $Sudo $exe @argv } else { & $exe @argv }
    if ($LASTEXITCODE -ne 0) { throw "$exe $($argv -join ' ') failed" }
}

# Every installed path is recorded so uninstall removes exactly what was put
# there and nothing else - important when the prefix is shared, like /usr/local
# or %ProgramFiles%.
$Installed = New-Object System.Collections.ArrayList
function Track([string]$p) { [void]$Installed.Add($p) }

function New-Dir([string]$p) {
    if ($Linux) { Invoke-Priv "mkdir" @("-p", $p) }
    else { New-Item -ItemType Directory -Force -Path $p | Out-Null }
}
function Put([string]$from, [string]$to) {
    if (-not (Test-Path $from)) { throw "missing from the archive: $from" }
    if ($Linux) { Invoke-Priv "cp" @("-f", $from, $to) }
    else { Copy-Item -Path $from -Destination $to -Force }
    Track $to
}
function Put-Tree([string]$fromDir, [string]$toDir) {
    if ($Linux) { Invoke-Priv "cp" @("-R", "$fromDir/.", "$toDir/") }
    else { Copy-Item -Path (Join-Path $fromDir '*') -Destination $toDir -Force -Recurse }
}
function Set-Exec([string]$p) { if ($Linux) { Invoke-Priv "chmod" @("+x", $p) } }

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("caustic-install-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
try {
    # --- the trees to install from ---
    # Each release archive is fetched at most once, and only if something in the
    # plan actually lives in it: the .so and the .csl are in the Linux tarball,
    # the .dll in the Windows zip, and the universal build is its own asset.
    $Roots = @{}
    function Get-Archive([string]$os) {
        if ($Roots[$os]) { return $Roots[$os] }
        $name = if ($os -eq "windows") { $Zip } else { $Tarball }
        $dir  = if ($os -eq "windows") { $WinDir } else { $LnxDir }
        Write-Host "downloading $name ..."
        $f = Join-Path $tmp $name
        Invoke-WebRequest -Uri "https://github.com/$Repo/releases/latest/download/$name" -OutFile $f -UseBasicParsing
        if ($os -eq "windows") { Expand-Archive -Path $f -DestinationPath $tmp -Force }
        else {
            # bsdtar has shipped with Windows since 10 1803; older machines - the
            # ones most likely to be on 5.1 - have none, and only need it here
            # because .so and .csl live in the Linux tarball.
            if (-not (Get-Command tar -ErrorAction SilentlyContinue)) {
                throw "'tar' is required to unpack $name (it carries libcaustic.so and libcaustic.csl). Windows 10 1803 and later include it; otherwise drop 'so' and 'csl' from -Lib."
            }
            & tar -xzf $f -C $tmp
            if ($LASTEXITCODE -ne 0) { throw "could not unpack $name" }
        }
        $Roots[$os] = Join-Path $tmp $dir
        return $Roots[$os]
    }

    if ($FromSource) {
        # Caustic compiles itself, so building it needs a compiler to start from.
        # Prefer one already in the checkout, then one on PATH, and fall back to
        # the released binaries - which is what the bootstrap is for.
        if (-not $SourceDir) {
            if ((Test-Path (Join-Path $PWD "Causticfile")) -and (Test-Path (Join-Path $PWD "src/main.cst"))) {
                $SourceDir = "$PWD"
            } else {
                if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw "-FromSource needs git on PATH (or -SourceDir DIR)" }
                $SourceDir = Join-Path $tmp "src"
                Write-Host "cloning $Repo ($Ref) ..."
                & git clone --depth 1 --branch $Ref --recurse-submodules "https://github.com/$Repo.git" $SourceDir 2>&1 | Out-Null
                if ($LASTEXITCODE -ne 0) { throw "clone failed" }
            }
        }
        if (-not (Test-Path (Join-Path $SourceDir "Causticfile"))) { throw "$SourceDir is not a Caustic checkout" }
        Write-Host "building from $SourceDir ..."

        $mk = if ($Win) { "caustic-mk.exe" } else { "caustic-mk" }
        $seed = $null
        if (Test-Path (Join-Path $SourceDir $mk)) { $seed = $SourceDir }
        elseif (Get-Command $mk -ErrorAction SilentlyContinue) { $seed = Split-Path (Get-Command $mk).Source }
        else {
            Write-Host "  no seed compiler - fetching the release to bootstrap with ..."
            $seed = Join-Path (Get-Archive $(if ($Win) { "windows" } else { "linux" })) "bin"
        }
        $env:PATH = "$seed$([IO.Path]::PathSeparator)$env:PATH"

        # The build log stays out of the terminal: packaging libcaustic warns
        # about the symbols the other OS cannot resolve, every time, and that is
        # expected noise. It is printed only if a step actually fails.
        $log = Join-Path $tmp "build.log"
        Push-Location $SourceDir
        try {
            $targets = @("caustic", "caustic-as", "caustic-ld", "caustic-mk")
            if ($Linux) { $targets += "caustic-lsp" }
            foreach ($t in $targets) {
                Write-Host "  building $t"
                & caustic-mk build $t *>> $log
                if ($LASTEXITCODE -ne 0) { Get-Content $log -Tail 20; throw "build of $t failed" }
            }
            # Whichever archives the plan needs get built rather than downloaded.
            $wantNative  = ($Native -in $Formats) -or $ToolList -or (-not $NoSource) -or ($NativeLib -in $LibList)
            $wantForeign = (($Formats + $LibList) | Where-Object { $_ -in @("exe","dll","elf","so","csl") -and $_ -ne $Native -and $_ -ne $NativeLib })
            if ($wantNative -or -not $wantForeign) {
                Write-Host "  packaging"
                & caustic-mk run $(if ($Win) { "dist-windows" } else { "dist" }) *>> $log
                if ($LASTEXITCODE -ne 0) { Get-Content $log -Tail 20; throw "packaging failed" }
            }
            if ($wantForeign) {
                Write-Host "  packaging $(if ($Win) { 'linux' } else { 'windows' })"
                & caustic-mk run $(if ($Win) { "dist" } else { "dist-windows" }) *>> $log
                if ($LASTEXITCODE -ne 0) { Get-Content $log -Tail 20; throw "cross packaging failed" }
            }
        } finally { Pop-Location }

        # Unpack whatever the build produced, so the install path below cannot
        # tell a built tree from a downloaded one.
        foreach ($pair in @(@("linux", $Tarball, $LnxDir), @("windows", $Zip, $WinDir))) {
            $built = Join-Path $SourceDir $pair[1]
            if (Test-Path $built) {
                if ($pair[0] -eq "windows") { Expand-Archive -Path $built -DestinationPath $tmp -Force }
                else { & tar -xzf $built -C $tmp }
                $Roots[$pair[0]] = Join-Path $tmp $pair[2]
            }
        }
    }

    New-Dir $Bin
    if (-not $NoSource) { New-Dir $StdDir } else { New-Dir $LibDir }

    # --- compiler(s) ---
    # Each format installs under its own name; the primary one additionally
    # answers to plain `caustic`, so the command you type does not change with
    # the flavour you picked.
    $primaryName = $null
    foreach ($f in $Formats) {
        $name = switch ($f) {
            "elf" {
                $p = Join-Path (Join-Path (Get-Archive "linux") "bin") "caustic"
                Put $p (Join-Path $Bin "caustic-elf"); Set-Exec (Join-Path $Bin "caustic-elf")
                "caustic-elf"
            }
            "exe" {
                $p = Join-Path (Join-Path (Get-Archive "windows") "bin") "caustic.exe"
                $to = if ($Win) { "caustic-native.exe" } else { "caustic.exe" }
                Put $p (Join-Path $Bin $to)
                $to
            }
            "cse" {
                # One file carrying a native body per OS and architecture. On
                # Linux a shell stub picks the right ELF, which is why it needs
                # the exec bit; the .exe in the name is what lets the same file
                # run on Windows, whose loader reads the MZ at offset 0.
                $dst = Join-Path $Bin $Universal
                Write-Host "downloading $Universal ..."
                $dl = Join-Path $tmp $Universal
                Invoke-WebRequest -Uri "https://github.com/$Repo/releases/latest/download/$Universal" -OutFile $dl -UseBasicParsing
                Put $dl $dst; Set-Exec $dst
                $Universal
            }
        }
        if ($f -eq $Primary) { $primaryName = $name }
    }

    if ($Primary -in $Runnable) {
        if ($Linux) {
            # Relative target, so the link keeps working if the prefix moves.
            Invoke-Priv "ln" @("-sf", $primaryName, (Join-Path $Bin "caustic"))
            Track (Join-Path $Bin "caustic")
        } else {
            # Windows has no symlink worth relying on without elevation, so the
            # primary is copied to the name you type rather than linked to it.
            Copy-Item (Join-Path $Bin $primaryName) (Join-Path $Bin "caustic.exe") -Force
            Track (Join-Path $Bin "caustic.exe")
        }
    } else {
        Write-Host "  note: $Primary does not run on this machine, so it keeps its own name ($primaryName)"
    }

    # --- tools ---
    foreach ($t in $ToolList) {
        $n = if ($Win) { "caustic-$t.exe" } else { "caustic-$t" }
        $p = Join-Path (Join-Path (Get-Archive $(if ($Win) { "windows" } else { "linux" })) "bin") $n
        if (Test-Path $p) { Put $p (Join-Path $Bin $n); Set-Exec (Join-Path $Bin $n) }
        else { Write-Host "  note: $n is not in this release, skipped" }
    }

    # --- stdlib ---
    if (-not $NoSource) {
        $std = Join-Path (Join-Path (Get-Archive $(if ($Win) { "windows" } else { "linux" })) (Join-Path "lib" "caustic")) "std"
        Put-Tree $std $StdDir
    }
    foreach ($l in $LibList) {
        # .so and .csl ship in the Linux tarball, .dll in the Windows zip -
        # whichever machine you are installing on.
        $from = if ($l -eq "dll") { "windows" } else { "linux" }
        $p = Join-Path (Join-Path (Get-Archive $from) (Join-Path "lib" "caustic")) "libcaustic.$l"
        if (Test-Path $p) { Put $p (Join-Path $LibDir "libcaustic.$l") }
        else { Write-Host "  note: libcaustic.$l is not in this release, skipped" }
    }

    # --- PATH ---
    if ($Win -and -not $NoPath) {
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
    $mf = @(
        "# caustic install manifest - written by install.ps1, read by update.ps1 and uninstall.ps1",
        "prefix=$Prefix",
        "format=$($Formats -join ',')",
        "tools=$(if ($ToolList) { $ToolList -join ',' } else { 'none' })",
        "lib=$(if ($LibList) { $LibList -join ',' } else { 'none' })",
        "source=$(if ($NoSource) { '0' } else { '1' })",
        "system=$(if ($System) { '1' } else { '0' })",
        "root=$Root"
    )
    if (-not $NoSource) { $mf += "stddir=$StdDir" }
    $mf += "files:"
    $mf += $Installed
    $stage = Join-Path $tmp "install-manifest"
    Set-Content -Path $stage -Value $mf -Encoding UTF8
    if ($Linux) { Invoke-Priv "cp" @("-f", $stage, (Join-Path $LibDir "install-manifest")) }
    else { Copy-Item $stage (Join-Path $LibDir "install-manifest") -Force }

    Write-Host ""
    $shown = if ($Win) { "caustic.exe" } else { "caustic" }
    if ($Primary -in $Runnable) {
        Write-Host "caustic installed -> $(Join-Path $Bin $shown) (from $primaryName)"
        Write-Host "try:  caustic --version"
    } else {
        Write-Host "caustic installed -> $(Join-Path $Bin $primaryName)"
    }
    if ($Linux -and -not (Get-Command caustic -ErrorAction SilentlyContinue)) {
        Write-Host "  (add to PATH:  export PATH=`"$Bin`:`$PATH`")"
    }
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
