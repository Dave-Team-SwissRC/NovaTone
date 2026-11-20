#!/bin/bash
# NovaTone - macOS Build (Universal: Apple Silicon + Intel), VST3 + Standalone
# Voraussetzungen: Xcode (inkl. Command Line Tools), CMake >= 3.22 (brew install cmake)
set -e
cd "$(dirname "$0")"
cmake --preset macos-release
cmake --build --preset macos-release

ART="build/macos-release/NovaTone_artefacts/Release"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
APP_DIR="$HOME/Applications"
mkdir -p "$VST3_DIR" "$APP_DIR"
rm -rf "$VST3_DIR/NovaTone.vst3" "$APP_DIR/NovaTone.app"
cp -R "$ART/VST3/NovaTone.vst3" "$VST3_DIR/"
cp -R "$ART/Standalone/NovaTone.app" "$APP_DIR/"
xattr -dr com.apple.quarantine "$VST3_DIR/NovaTone.vst3" "$APP_DIR/NovaTone.app" 2>/dev/null || true

echo
echo "Installiert:"
echo "  VST3:       $VST3_DIR/NovaTone.vst3   (Cubase: VST-Plug-in-Verwaltung > Aktualisieren)"
echo "  Standalone: $APP_DIR/NovaTone.app"
