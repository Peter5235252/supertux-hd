//  SuperTux-HD: macOS Vulkan ICD selection (KosmicKrisp + MoltenVK).
//  Copyright (C) 2026 SuperTux-HD contributors (GPL-3.0-or-later).

#include "supertux/mac_vulkan_select.hpp"

#include <cstdlib>
#include <cstdio>
#include <string>
#include <filesystem>
#include <stdexcept>

#include "util/log.hpp"

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach-o/dyld.h>

namespace {

/// Darwin major version from kern.osrelease ("25.1.0" -> 25).
/// macOS 26 (Tahoe, Metal 4 baseline for KosmicKrisp) is Darwin 25.
int
darwin_major()
{
  char buf[64] = {};
  size_t len = sizeof(buf);
  if (sysctlbyname("kern.osrelease", buf, &len, nullptr, 0) != 0)
    return 0;
  return atoi(buf);
}

bool
is_arm64()
{
  char buf[32] = {};
  size_t len = sizeof(buf);
  if (sysctlbyname("hw.machine", buf, &len, nullptr, 0) != 0)
    return false;
  return std::string(buf) == "arm64";
}

/// Directory of SuperTux.app/Contents/Frameworks (empty on failure).
/// Handles both bundle layouts: binary directly in Contents/MacOS/
/// and the CPack layout with the binary in Contents/Resources/bin/
/// (launched via the Contents/MacOS/SuperTux startup script).
std::string
frameworks_dir()
{
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string path(size, '\0');
  if (_NSGetExecutablePath(path.data(), &size) != 0)
    return {};
  path.resize(std::char_traits<char>::length(path.c_str()));
  // SDL may be started with a relative path, or through a symlink.
  std::error_code ec;
  path = std::filesystem::canonical(path, ec).string();
  if (ec)
    return {};
  auto pos = path.rfind("/Contents/MacOS/");
  if (pos != std::string::npos)
    return path.substr(0, pos) + "/Contents/Frameworks";
  pos = path.rfind("/Contents/Resources/");
  if (pos != std::string::npos)
    return path.substr(0, pos) + "/Contents/Frameworks";
  return {};
}

bool
file_exists(const std::string& path)
{
  // Minimal existence check without extra dependencies here.
  FILE* f = fopen(path.c_str(), "rb");
  if (f)
    fclose(f);
  return f != nullptr;
}

} // namespace

std::string
GetBundledVulkanIcdDir()
{
  const std::string fw = frameworks_dir();
  if (fw.empty())
    return {};
  const std::string dir = fw + "/vulkan/icd.d";
  // Presence of either manifest means the packaging step staged ICDs.
  if (file_exists(dir + "/kosmickrisp.json") ||
      file_exists(dir + "/moltenvk.json"))
    return dir;
  return {};
}

std::string
SelectMacVulkanIcd()
{
  const std::string dir = GetBundledVulkanIcdDir();
  const char* override = std::getenv("SUPERTUX_MAC_VULKAN_DRIVER");
  const std::string requested = override ? override : "auto";
  if (requested != "auto" && requested != "moltenvk" && requested != "kosmickrisp")
    throw std::runtime_error("SUPERTUX_MAC_VULKAN_DRIVER must be auto, moltenvk or kosmickrisp");
  if (dir.empty())
  {
    if (requested != "auto")
      throw std::runtime_error("Requested macOS Vulkan driver is not bundled");
    return {};
  }

  const bool modern = is_arm64() && (darwin_major() >= 25);
  const std::string kk = dir + "/kosmickrisp.json";
  const std::string mvk = dir + "/moltenvk.json";
  if (requested == "moltenvk")
  {
    if (!file_exists(mvk))
      throw std::runtime_error("MoltenVK was explicitly requested but is not bundled");
    log_info << "Selecting MoltenVK Vulkan ICD (explicit selection)" << std::endl;
    return mvk;
  }
  if (requested == "kosmickrisp")
  {
    if (!modern || !file_exists(kk))
      throw std::runtime_error("KosmicKrisp requires an arm64 process, macOS 26+ and the bundled ICD");
    log_info << "Selecting KosmicKrisp Vulkan ICD (explicit selection)" << std::endl;
    return kk;
  }
  // Default: KosmicKrisp is preferred on modern Apple Silicon; MoltenVK
  // is the fallback everywhere else (Intel Macs, older macOS, missing KK).
  if (modern && file_exists(kk))
  {
    log_info << "Selecting KosmicKrisp Vulkan ICD (preferred path for Apple Silicon, macOS 26+)" << std::endl;
    return kk;
  }
  if (file_exists(mvk))
  {
    log_info << "Selecting MoltenVK Vulkan ICD (fallback path)" << std::endl;
    return mvk;
  }
  throw std::runtime_error("No bundled Vulkan ICD found");
}

void
InitMacVulkanLoader()
{
  const std::string icd = SelectMacVulkanIcd();
  if (icd.empty())
    return;
  // The library paths are relative to each manifest, not dyld tokens.
  // No shared temporary files: parallel app copies cannot race or leave
  // a stale absolute path after the user moves/renames the bundle.
  // Must happen before the Vulkan loader is first used (SDL device init).
  // Only VK_ICD_FILENAMES: VK_LOADER_DRIVERS_SELECT takes driver names,
  // not paths, so setting it to a path would filter out every driver.
  setenv("VK_ICD_FILENAMES", icd.c_str(), 1);
  setenv("VK_DRIVER_FILES", icd.c_str(), 1);
}

#else // !__APPLE__

std::string
GetBundledVulkanIcdDir()
{
  return {};
}

std::string
SelectMacVulkanIcd()
{
  return {};
}

void
InitMacVulkanLoader()
{
}

#endif
