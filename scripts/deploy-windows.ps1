param(
    [Parameter(Mandatory = $true)][string]$QtRoot,
    [Parameter(Mandatory = $true)][string]$OpenCVBin,
    [string]$BuildDir = "",
    [string]$Destination = ""
)
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$BuildDir) { $BuildDir = Join-Path $projectRoot "build-windows" }
if (!$Destination) { $Destination = Join-Path $projectRoot "dist/windows" }
$QtRoot = (Resolve-Path $QtRoot).Path
$OpenCVBin = (Resolve-Path $OpenCVBin).Path
$deployTool = Join-Path $QtRoot "bin/windeployqt.exe"
if (!(Test-Path $deployTool)) { throw "Cannot find windeployqt.exe in QtRoot/bin." }
if (!(Test-Path (Join-Path $BuildDir "base_vision.exe"))) {
    throw "Build base_vision.exe first with build-windows.ps1."
}
# CMake's Windows runtime resolver does NOT search PATH. Configure explicit
# DIRECTORIES for dependency resolution; PATH also allows tools/DLLs to load.
$env:PATH = "$OpenCVBin;$(Join-Path $QtRoot 'bin');$env:PATH"
$runtimeDirs = "$(Join-Path $QtRoot 'bin');$OpenCVBin".Replace('\', '/')
& cmake -S $projectRoot -B $BuildDir "-DBASEVISION_RUNTIME_DIRS=$runtimeDirs"
if ($LASTEXITCODE -ne 0) { throw "Runtime search path configuration failed." }
& cmake --install $BuildDir --config Release --prefix $Destination
if ($LASTEXITCODE -ne 0) { throw "Runtime dependency installation failed; check OpenCVBin and PATH." }
$program = Join-Path $Destination "base_vision.exe"
& $deployTool --release --compiler-runtime --no-translations --no-opengl-sw $program
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }
Copy-Item (Join-Path $projectRoot "README.md") (Join-Path $Destination "README.md")
Write-Host "Deployed: $Destination"
Write-Host "Validate on a Windows machine with a camera; Windows binaries are not tested on Linux."
