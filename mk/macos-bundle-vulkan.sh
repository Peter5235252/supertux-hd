#!/usr/bin/env bash
# Stage Vulkan ICDs (KosmicKrisp + MoltenVK) into a macOS SuperTux.app.
#
# Apple ships no Vulkan driver. This script downloads the LunarG Vulkan SDK
# for macOS (needs KosmicKrisp, SDK >= 1.4.363, Sept 2026) and stages:
#   SuperTux.app/Contents/Frameworks/vulkan/icd.d/kosmickrisp.json
#   SuperTux.app/Contents/Frameworks/vulkan/icd.d/moltenvk.json
# plus the ICD dylibs + libvulkan loader into Frameworks/.
# At startup the game picks KosmicKrisp (Apple Silicon + macOS 26+) or
# MoltenVK (everything else) - see src/supertux/mac_vulkan_select.cpp.
#
# RUN ON macOS ONLY (needs hdiutil/curl + the LunarG SDK disk image).
# Usage: APP=/path/to/SuperTux.app [SDK_VERSION=1.4.363.0] ./mk/macos-bundle-vulkan.sh
#
# Verify afterwards with: vulkaninfo --json | grep -i -E "kosmic|molten"
set -euo pipefail

APP="${APP:?Set APP to the SuperTux.app bundle path, e.g. APP=./build/SuperTux.app}"
SDK_VERSION="${SDK_VERSION:-1.4.363.0}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

if [[ "$(uname)" != "Darwin" ]]; then
  echo "error: macOS only (needs hdiutil)" >&2
  exit 1
fi

DMG_URL="https://sdk.lunarg.com/sdk/download/${SDK_VERSION}/mac/vulkansdk-macos-${SDK_VERSION}.dmg"
echo "== downloading LunarG Vulkan SDK ${SDK_VERSION}"
curl -fL -o "$WORK/sdk.dmg" "$DMG_URL"

echo "== mounting SDK"
MNT="$(hdiutil attach -nobrowse -mountpoint "$WORK/mnt" "$WORK/sdk.dmg" | tail -1 | cut -f3-)"
trap 'hdiutil detach "$MNT" >/dev/null 2>&1; rm -rf "$WORK"' EXIT

FW="$APP/Contents/Frameworks"
ICD="$FW/vulkan/icd.d"
mkdir -p "$ICD"

pick_dylib() { # $1 = case-insensitive name fragment -> prints first match
  find "$MNT" \( -name '*.dylib' -o -name '*.json' \) -ipath "*$1*" | head -1
}

echo "== staging loader + ICDs"
cp "$(pick_dylib 'libvulkan.1')" "$FW/libvulkan.1.dylib"
KK_LIB="$(pick_dylib 'kosmickrisp')"
MVK_LIB="$(pick_dylib 'moltenvk')"
[ -n "$KK_LIB" ] || { echo "error: KosmicKrisp dylib not found in SDK (need >= 1.4.363)" >&2; exit 1; }
[ -n "$MVK_LIB" ] || { echo "error: MoltenVK dylib not found in SDK" >&2; exit 1; }
cp "$KK_LIB" "$FW/$(basename "$KK_LIB")"
cp "$MVK_LIB" "$FW/$(basename "$MVK_LIB")"

# Our own minimal ICD manifests (robust against upstream renames upstream).
cat > "$ICD/kosmickrisp.json" <<EOF
{
  "file_format_version": "1.0.0",
  "ICD": {
    "library_path": "@executable_path/../Frameworks/$(basename "$KK_LIB")",
    "api_version": "1.4.0"
  }
}
EOF
cat > "$ICD/moltenvk.json" <<EOF
{
  "file_format_version": "1.0.0",
  "ICD": {
    "library_path": "@executable_path/../Frameworks/$(basename "$MVK_LIB")",
    "api_version": "1.4.0"
  }
}
EOF

echo "== fixing rpaths"
install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP/Contents/MacOS/supertux2" || true

echo "== staged:"
ls -la "$FW" | grep -i -E "vulkan|kosmic|molten"
ls -la "$ICD"
echo "done. Verify on target machine: VK_ICD_FILENAMES=$ICD/kosmickrisp.json vulkaninfo | head -20"
