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

$ErrorActionPreference = "Stop"

$Repo   = "Caua726/Caustic"
$Zip    = "caustic-x86_64-windows.zip"
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
    $url = "https://github.com/$Repo/releases/latest/download/$Zip"
    $zipPath = Join-Path $tmp $Zip
    Write-Host "downloading latest release ..."
    Invoke-WebRequest -Uri $url -OutFile $zipPath -UseBasicParsing
    Expand-Archive -Path $zipPath -DestinationPath $tmp -Force
    $src = Join-Path $tmp "caustic-x86_64-windows"

    # --- install ---
    Write-Host "installing to $Prefix ..."
    New-Item -ItemType Directory -Force -Path $Bin, $Lib | Out-Null
    Copy-Item -Path (Join-Path $src "bin\*")            -Destination $Bin -Recurse -Force
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
    Write-Host "caustic installed -> $Bin\caustic.exe"
    Write-Host "try:  caustic --version"
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
