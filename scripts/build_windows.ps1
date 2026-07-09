# Build Othello for Windows
# Requires: CMake, Visual Studio 2022, vcpkg with raylib installed

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $ProjectRoot

try {
    # Check prerequisites
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-Error "CMake not found. Install from https://cmake.org/download/"
        exit 1
    }

    # Check vcpkg
    $VcpkgRoot = $env:VCPKG_ROOT
    if (-not $VcpkgRoot) {
        $VcpkgRoot = Join-Path $env:USERPROFILE "vcpkg"
    }

    if (-not (Test-Path (Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"))) {
        Write-Host "vcpkg not found at $VcpkgRoot. Installing..." -ForegroundColor Yellow
        git clone https://github.com/Microsoft/vcpkg.git $VcpkgRoot
        & (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat")
        $env:VCPKG_ROOT = $VcpkgRoot
        [Environment]::SetEnvironmentVariable("VCPKG_ROOT", $VcpkgRoot, "User")
    }

    # Install raylib
    Write-Host "Installing raylib via vcpkg..." -ForegroundColor Cyan
    & (Join-Path $VcpkgRoot "vcpkg") install raylib:x64-windows

    # Configure and build
    Write-Host "Configuring CMake..." -ForegroundColor Cyan
    $Toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$Toolchain"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

    Write-Host "Building..." -ForegroundColor Cyan
    cmake --build build --config Release
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }

    # Package portable ZIP
    Write-Host "Packaging..." -ForegroundColor Cyan
    $dist = "Othello-Windows"
    Remove-Item -Recurse -Force $dist -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $dist | Out-Null
    Copy-Item "build/Release/othello.exe" -Destination $dist/
    Copy-Item -Recurse "assets" -Destination $dist/
    if (Test-Path "Othello-Windows.zip") { Remove-Item "Othello-Windows.zip" }
    Compress-Archive -Path "$dist/*" -DestinationPath "Othello-Windows.zip"
    Remove-Item -Recurse -Force $dist

    Write-Host "Done! Othello-Windows.zip created." -ForegroundColor Green
}
finally {
    Pop-Location
}
