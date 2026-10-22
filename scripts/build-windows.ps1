param(
    [Parameter(Mandatory = $true)][string]$QtRoot,
    [Parameter(Mandatory = $true)][string]$OpenCVDir,
    [string]$OpenCVBin = "",
    [string]$BuildDir = "",
    [switch]$RunTests
)
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$BuildDir) { $BuildDir = Join-Path $projectRoot "build-windows" }
$QtRoot = (Resolve-Path $QtRoot).Path
$OpenCVDir = (Resolve-Path $OpenCVDir).Path
if (!(Test-Path (Join-Path $QtRoot "lib/cmake/Qt6/Qt6Config.cmake"))) {
    throw "QtRoot must be the MSVC x64 Qt installation, for example C:/Qt/6.8.3/msvc2022_64"
}
if (!(Test-Path (Join-Path $OpenCVDir "OpenCVConfig.cmake"))) {
    throw "OpenCVDir must contain OpenCVConfig.cmake (matching MSVC x64 Release binaries)."
}
if (!$OpenCVBin) {
    $candidate = Join-Path (Split-Path -Parent $OpenCVDir) "bin"
    if (Test-Path $candidate) { $OpenCVBin = (Resolve-Path $candidate).Path }
}
$env:PATH = "$(Join-Path $QtRoot 'bin');$env:PATH"
if ($OpenCVBin) { $env:PATH = "$((Resolve-Path $OpenCVBin).Path);$env:PATH" }
$runtimeDirs = (Join-Path $QtRoot "bin").Replace('\', '/')
if ($OpenCVBin) { $runtimeDirs += ";$((Resolve-Path $OpenCVBin).Path.Replace('\', '/'))" }
# Run this script in the x64 Native Tools Command Prompt / Developer PowerShell.
$testSetting = if ($RunTests) { "ON" } else { "OFF" }
& cmake -S $projectRoot -B $BuildDir -G Ninja `
    "-DCMAKE_BUILD_TYPE=Release" "-DBUILD_TESTING=$testSetting" `
    "-DCMAKE_PREFIX_PATH=$QtRoot" "-DOpenCV_DIR=$OpenCVDir" `
    "-DBASEVISION_RUNTIME_DIRS=$runtimeDirs"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
& cmake --build $BuildDir --parallel 4
if ($LASTEXITCODE -ne 0) { throw "Build failed." }
if ($RunTests) {
    # OpenCV's DLL directory must also be on PATH when running tests.
    & ctest --test-dir $BuildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed." }
}
Write-Host "Built: $BuildDir/base_vision.exe"
