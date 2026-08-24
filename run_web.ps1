$ErrorActionPreference = "Stop"

# Ensure we're running from repo root (this script's folder)
$repo = Split-Path $MyInvocation.MyCommand.Path -Parent
Set-Location $repo

# Run the build + serve script in /web
& "$repo\web\build_wasm.ps1"