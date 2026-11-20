# Kopiert das gebaute VST3 nach C:\Program Files\Common Files\VST3 (Cubase findet es dort automatisch).
# Fordert bei Bedarf selbst Administratorrechte an.
$ErrorActionPreference = "Stop"
$src = Join-Path $PSScriptRoot "build\windows-release\NovaTone_artefacts\Release\VST3\NovaTone.vst3"
$dst = Join-Path $env:CommonProgramFiles "VST3"

if (-not (Test-Path $src)) { Write-Host "NovaTone.vst3 nicht gefunden. Zuerst .\BUILD-WINDOWS.ps1 ausfuehren."; exit 1 }

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "Administratorrechte noetig - starte neu als Administrator ..."
    Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    exit
}
Copy-Item $src $dst -Recurse -Force
Write-Host "Installiert nach: $dst\NovaTone.vst3"
Write-Host "In Cubase: Studio > VST-Plug-in-Verwaltung > Aktualisieren."
Read-Host "Enter zum Schliessen"
