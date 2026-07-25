# install.ps1 — Caustic installer for Windows (PowerShell).
#
# One-liner:
#   irm https://raw.githubusercontent.com/Caua726/Caustic/main/install.ps1 | iex
#
# Downloads the latest caustic-x86_64-windows.zip release, installs the toolchain
# (caustic.exe + assembler/linker/maker) and the stdlib into %LOCALAPPDATA%\caustic,
# and adds the bin directory to your user PATH. Binaries are native PE — no MSVC,
# no MinGW, no runtime.
#
# Options (set as env vars before running):
#   $env:CAUSTIC_PREFIX  install root (default: %LOCALAPPDATA%\caustic)
#   $env:CAUSTIC_NOPATH  set to 1 to skip editing PATH
#   $env:CAUSTIC_CSE     set to 1 to install the universal caustic-universal.cse.exe
#                        instead of the native toolchain — one file that also runs on
#                        Linux and CausticOS, x86_64 and ARM64

$ErrorActionPreference = "Stop"

$Repo   = "Caua726/Caustic"
$Zip    = "caustic-x86_64-windows.zip"
$Universal = "caustic-universal.cse.exe"
$UseCse = ($env:CAUSTIC_CSE -eq "1")
$Prefix = if ($env:CAUSTIC_PREFIX) { $env:CAUSTIC_PREFIX } else { Join-Path $env:LOCALAPPDATA "caustic" }
$Bin    = Join-Path $Prefix "bin"
$Lib    = Join-Path $Prefix "lib\caustic"

if ($env:PROCESSOR_ARCHITECTURE -ne "AMD64" -and $env:PROCESSOR_ARCHITEW6432 -ne "AMD64") {
    Write-Warning "Caustic ships x86_64 (AMD64) binaries; your architecture is $env:PROCESSOR_ARCHITECTURE."
}

# --- download + extract ---
$tmp = Join-Path $env:TEMP ("caustic-install-" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
try {
    # The stdlib source comes from the zip either way — the universal compiler
    # still needs .cst files to compile against.
    $url = "https://github.com/$Repo/releases/latest/download/$Zip"
    $zipPath = Join-Path $tmp $Zip
    Write-Host "downloading latest release ..."
    Invoke-WebRequest -Uri $url -OutFile $zipPath -UseBasicParsing
    Expand-Archive -Path $zipPath -DestinationPath $tmp -Force
    $src = Join-Path $tmp "caustic-x86_64-windows"

    # --- install ---
    Write-Host "installing to $Prefix ..."
    New-Item -ItemType Directory -Force -Path $Bin, $Lib | Out-Null
    if ($UseCse) {
        # One file carrying a native body per OS and architecture. Windows runs
        # it because offset 0 is an MZ header, which is also why the name keeps
        # its .exe.
        $uurl = "https://github.com/$Repo/releases/latest/download/$Universal"
        $upath = Join-Path $Bin $Universal
        Write-Host "downloading $Universal ..."
        Invoke-WebRequest -Uri $uurl -OutFile $upath -UseBasicParsing
        Copy-Item -Path $upath -Destination (Join-Path $Bin "caustic.exe") -Force
    } else {
        Copy-Item -Path (Join-Path $src "bin\*")        -Destination $Bin -Recurse -Force
    }
    Copy-Item -Path (Join-Path $src "lib\caustic\std")  -Destination $Lib -Recurse -Force

    # --- PATH ---
    if ($env:CAUSTIC_NOPATH -ne "1") {
        $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
        if (($userPath -split ';') -notcontains $Bin) {
            $newPath = if ([string]::IsNullOrEmpty($userPath)) { $Bin } else { "$userPath;$Bin" }
            [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
            Write-Host "added $Bin to your user PATH (restart your terminal to pick it up)"
        }
    }

    Write-Host ""
    if ($UseCse) {
        Write-Host "caustic installed -> $Bin\caustic.exe (copy of $Universal, universal)"
    } else {
        Write-Host "caustic installed -> $Bin\caustic.exe"
    }
    Write-Host "try:  caustic --version"
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
