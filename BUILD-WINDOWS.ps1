# NovaTone - Windows Build (VST3 + Standalone)
# Voraussetzungen: Visual Studio 2022 (Workload "Desktopentwicklung mit C++"), CMake >= 3.22, Git
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
cmake --preset windows-release
cmake --build --preset windows-release

$art = Join-Path $PSScriptRoot "build\windows-release\NovaTone_artefacts\Release"
Write-Host ""
Write-Host "Fertig. Ergebnis:"
Write-Host "  VST3:       $art\VST3\NovaTone.vst3"
Write-Host "  Standalone: $art\Standalone\NovaTone.exe"
Write-Host ""
Write-Host "Installieren (VST3 fuer Cubase):  .\INSTALL-WINDOWS.ps1"
