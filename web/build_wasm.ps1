$ErrorActionPreference = "Stop"

# Move to repo root
$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo

# Locate emsdk: EMSDK env var > %USERPROFILE%\emsdk > C:\emsdk
$emsdk = if ($env:EMSDK) {
    $env:EMSDK
} elseif (Test-Path "$env:USERPROFILE\emsdk") {
    "$env:USERPROFILE\emsdk"
} elseif (Test-Path "C:\emsdk") {
    "C:\emsdk"
} else {
    throw "Could not find emsdk. Set the EMSDK environment variable or install emsdk to $env:USERPROFILE\emsdk"
}

# Load emsdk env (adds em++ to PATH for this script run) quietly
$env:EMSDK_QUIET = "1"
. "$emsdk\emsdk_env.ps1" | Out-Null

# Build via CMake (mirrors the CI/GitHub Actions workflow)
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm -j $env:NUMBER_OF_PROCESSORS
Write-Host "Built web/sim.js"

# Serve the web folder (required for ES modules)
Write-Host "Serving web/ at http://localhost:8000  (Ctrl+C to stop)"
Push-Location web
try {
  if (Get-Command py -ErrorAction SilentlyContinue) {
    py -m http.server 8000
  } else {
    python -m http.server 8000
  }
} finally {
  Pop-Location
}
