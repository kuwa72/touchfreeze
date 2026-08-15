# build.ps1 - Automated Build and Release Script for TouchFreeze
param(
    [string]$Configuration = "Release",
    [string]$Platform = "Win32"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Building TouchFreeze ($Configuration|$Platform) ===" -ForegroundColor Cyan

# Locate MSBuild using vswhere
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found at $vswhere"
}

$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
if (-not $msbuild -or -not (Test-Path $msbuild)) {
    throw "MSBuild.exe could not be found via vswhere."
}

Write-Host "Using MSBuild: $msbuild" -ForegroundColor DarkGray

# Execute MSBuild
& $msbuild TouchFreeze.sln /p:Configuration=$Configuration /p:Platform=$Platform /m /v:m
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

Write-Host "`n=== Verifying Output Artifacts ===" -ForegroundColor Cyan
$exePath = "Executable\Bin\TouchFreeze.exe"
$msiPath = "Executable\Bin\TouchFreeze.msi"
$dllPath = "Executable\Bin\TouchFreeze.dll"

if (Test-Path $exePath) {
    $exeItem = Get-Item $exePath
    Write-Host "[OK] Executable: $exePath ($($exeItem.Length) bytes, LastWrite: $($exeItem.LastWriteTime))" -ForegroundColor Green
} else {
    Write-Warning "[FAIL] Executable not found at $exePath"
}

if (Test-Path $dllPath) {
    $dllItem = Get-Item $dllPath
    Write-Host "[OK] Hook DLL:   $dllPath ($($dllItem.Length) bytes, LastWrite: $($dllItem.LastWriteTime))" -ForegroundColor Green
} else {
    Write-Warning "[FAIL] Hook DLL not found at $dllPath"
}

if (Test-Path $msiPath) {
    $msiItem = Get-Item $msiPath
    Write-Host "[OK] Installer:  $msiPath ($($msiItem.Length) bytes, LastWrite: $($msiItem.LastWriteTime))" -ForegroundColor Green
} else {
    Write-Warning "[FAIL] Installer not found at $msiPath"
}

Write-Host "`n=== Build & Release verification complete ===" -ForegroundColor Green
