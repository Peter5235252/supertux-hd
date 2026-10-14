//  SuperTux-HD: macOS Vulkan ICD selection (KosmicKrisp + MoltenVK).
//  Copyright (C) 2026 SuperTux-HD contributors (GPL-3.0-or-later).
//
//  Apple ships no Vulkan driver. We bundle two Vulkan-on-Metal ICDs and
//  point the Vulkan loader at the right one before SDL creates its GPU
//  device:
//
//    - KosmicKrisp: preferred default on Apple Silicon + macOS 26+
//      (Darwin 25+, Metal 4).
//    - MoltenVK: fallback everywhere else, forced on Intel Macs and
//      older macOS.
//    - SUPERTUX_MAC_VULKAN_DRIVER overrides: "moltenvk" forces MoltenVK,
//      "kosmickrisp" forces KosmicKrisp (fails loudly when unsupported),
//      "auto" (default) picks per machine as above.
//
//  No rendering code differs between them: both expose plain Vulkan,
//  so the SDL_GPU backend, SPIR-V shaders, and the "vulkan" driver pin
//  work unchanged. See mk/macos-bundle-vulkan.sh for how the dylibs and
//  ICD manifests get staged into SuperTux.app.

#pragma once

#include <string>

/// Absolute path of the bundled Vulkan ICD directory, or empty if the
/// bundle does not ship one (e.g. development builds without packaging).
std::string GetBundledVulkanIcdDir();

/// Choose the ICD manifest for this machine. Returns the manifest path,
/// or empty when nothing suitable is bundled.
std::string SelectMacVulkanIcd();

/// Point the Vulkan loader at SelectMacVulkanIcd(). Call once at startup,
/// before any SDL GPU device is created. No-op when there is nothing to
/// select, or on non-Apple platforms (still safe to call).
void InitMacVulkanLoader();

/// Display name of the active Vulkan-on-Metal driver for the settings
/// menu: "KosmicKrisp" or "MoltenVK" when determinable, otherwise
/// "Vulkan". Always "Vulkan" off Apple platforms.
std::string GetMacVulkanDriverName();

/// Full settings-menu description for the driver, clarifying it is a
/// Vulkan-to-Metal translation layer rather than native Vulkan.
std::string GetMacVulkanDriverDescription();
