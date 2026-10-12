#!/usr/bin/env bash
# Stage Vulkan loader + ICDs (KosmicKrisp + MoltenVK) into a macOS SuperTux.app.
#
# Apple ships no Vulkan driver. This script takes the LunarG Vulkan SDK
# for macOS (zip-era packaging, e.g. vulkansdk-macos-1.4.363.0.zip) and stages:
#   SuperTux.app/Contents/Frameworks/libvulkan.1.dylib   (loader)
#   SuperTux.app/Contents/Frameworks/libvulkan.dylib     (loader, alt name)
#   SuperTux.app/Contents/Frameworks/<moltenvk dylib>    (all Macs)
#   SuperTux.app/Contents/Frameworks/<kosmickrisp dylib> (Apple Silicon, macOS 26+)
#   SuperTux.app/Contents/Frameworks/vulkan/icd.d/{moltenvk,kosmickrisp}.json
# At startup the game points the loader at one of those manifests via
# VK_ICD_FILENAMES - see src/supertux/mac_vulkan_select.cpp.
#
# RUN ON macOS ONLY.
# Usage: APP=/path/to/SuperTux.app [SDK_ZIP=/path/to/sdk.zip]
#        [SDK_VERSION=1.4.363.0|latest] [SDKROOT=$HOME/VulkanSDK/<ver>]
#        ./mk/macos-bundle-vulkan.sh
#
# SDK_ZIP skips the download (CI caches the zip). Otherwise the script
# downloads vulkansdk-macos-<ver>.zip from LunarG.
set -euo pipefail

APP="${APP:?Set APP to the SuperTux.app bundle path, e.g. APP=./build/SuperTux.app}"
SDK_VERSION="${SDK_VERSION:-latest}"
if [ "$SDK_VERSION" = "latest" ]; then
  SDK_VERSION="$(curl -fsSL --max-time 30 https://vulkan.lunarg.com/sdk/latest/mac.txt)"
  echo "== latest macOS SDK: $SDK_VERSION"
fi
SDKROOT="${SDKROOT:-$HOME/VulkanSDK/$SDK_VERSION}"
SDK_ZIP="${SDK_ZIP:-}"

if [[ "$(uname)" != "Darwin" ]]; then
  echo "error: macOS only" >&2
  exit 1
fi
if [ ! -d "$APP/Contents/MacOS" ]; then
  echo "error: $APP does not look like a macOS app bundle" >&2
  exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

find_sdk_file() { # $1 = dir, $2 = case-insensitive fragment -> first match
  find "$1" -ipath "*$2*" 2>/dev/null | head -1
}

if [ -z "$SDK_ZIP" ]; then
  SDK_ZIP="$WORK/vulkansdk-macos-${SDK_VERSION}.zip"
  echo "== downloading LunarG Vulkan SDK ${SDK_VERSION}"
  curl -fL -o "$SDK_ZIP" \
    "https://sdk.lunarg.com/sdk/download/${SDK_VERSION}/mac/vulkansdk-macos-${SDK_VERSION}.zip"
else
  echo "== using SDK zip: $SDK_ZIP"
fi

if [ ! -d "$SDKROOT/macOS/lib" ]; then
  echo "== installing SDK to $SDKROOT"
  unzip -q "$SDK_ZIP" -d "$WORK/sdk"
  INSTALLER="$(find "$WORK/sdk" -path '*Contents/MacOS*' -name 'vulkansdk-macOS-*' | head -1)"
  [ -n "$INSTALLER" ] || { echo "error: SDK installer not found in zip" >&2; exit 1; }
  chmod +x "$INSTALLER"
  "$INSTALLER" --accept-licenses --accept-messages \
    --confirm-command install --root "$SDKROOT"
else
  echo "== SDK already installed at $SDKROOT"
fi

LIBDIR="$SDKROOT/macOS/lib"
SHAREDIR="$SDKROOT/macOS/share"
[ -d "$LIBDIR" ] || { echo "error: $LIBDIR missing after install" >&2; exit 1; }

LOADER="$(find_sdk_file "$LIBDIR" 'libvulkan.1.dylib')"
MVK_LIB="$(find_sdk_file "$LIBDIR" 'libMoltenVK.dylib')"
KK_LIB="$(find_sdk_file "$LIBDIR" 'kosmic')"
[ -n "$LOADER" ] || { echo "error: Vulkan loader not found in SDK" >&2; exit 1; }
[ -n "$MVK_LIB" ] || { echo "error: MoltenVK dylib not found in SDK" >&2; exit 1; }
if [ -z "$KK_LIB" ]; then
  echo "warning: KosmicKrisp dylib not found in SDK; shipping MoltenVK only" >&2
fi

FW="$APP/Contents/Frameworks"
ICD="$FW/vulkan/icd.d"
mkdir -p "$ICD"

echo "== staging loader + ICDs"
cp "$LOADER" "$FW/libvulkan.1.dylib"
cp "$LOADER" "$FW/libvulkan.dylib"
MVK_BASE="$(basename "$MVK_LIB")"
cp "$MVK_LIB" "$FW/$MVK_BASE"
if [ -n "$KK_LIB" ]; then
  KK_BASE="$(basename "$KK_LIB")"
  cp "$KK_LIB" "$FW/$KK_BASE"
fi

# The game binary lives either directly in Contents/MacOS/ or (CPack
# layout) in Contents/Resources/bin/ behind a startup script. The loader
# resolves each manifest's library_path relative to the main executable,
# so determine the Frameworks prefix from the binary's own directory
# BEFORE writing the manifests.
if [ -f "$APP/Contents/MacOS/supertux2" ]; then
  BIN="$APP/Contents/MacOS/supertux2"
  RELDIR="@executable_path/../Frameworks"
elif [ -f "$APP/Contents/Resources/bin/supertux2" ]; then
  BIN="$APP/Contents/Resources/bin/supertux2"
  RELDIR="@executable_path/../../Frameworks"
else
  echo "error: supertux2 binary not found in $APP" >&2
  exit 1
fi

cat > "$ICD/moltenvk.json" <<EOF
{
  "file_format_version": "1.0.0",
  "ICD": {
    "library_path": "$RELDIR/$MVK_BASE",
    "api_version": "1.4.0"
  }
}
EOF
if [ -n "$KK_LIB" ]; then
cat > "$ICD/kosmickrisp.json" <<EOF
{
  "file_format_version": "1.0.0",
  "ICD": {
    "library_path": "$RELDIR/$KK_BASE",
    "api_version": "1.4.0"
  }
}
EOF
fi

echo "== fixing rpaths, ad-hoc signing"
install_name_tool -add_rpath "$RELDIR" "$BIN" 2>/dev/null || true
while IFS= read -r f; do
  codesign --force --sign - "$f"
done < <(find "$FW" "$APP/Contents/MacOS" "$APP/Contents/Resources/bin" "$APP/Contents/Resources/MacOS" -type f \( -name '*.dylib' -o -name 'supertux2' \) 2>/dev/null)

echo "== staged:"
ls -la "$FW" | grep -i -E "vulkan|kosmic|molten" || true
ls -la "$ICD"
echo "== arch slices:"
lipo -info "$FW/libvulkan.1.dylib" "$FW/$MVK_BASE" ${KK_LIB:+ "$FW/$KK_BASE"} 2>/dev/null || true
lipo -info "$BIN"
echo done.
