//  SuperTux-HD: macOS Vulkan ICD selection (KosmicKrisp + MoltenVK).
//  Copyright (C) 2026 SuperTux-HD contributors (GPL-3.0-or-later).

#include "supertux/mac_vulkan_select.hpp"

#include <cstdlib>

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
std::string
frameworks_dir()
{
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string path(size, '\0');
  if (_NSGetExecutablePath(path.data(), &size) != 0)
    return {};
  // .../SuperTux.app/Contents/MacOS/supertux2 -> .../Frameworks
  auto pos = path.rfind("/Contents/MacOS/");
  if (pos == std::string::npos)
    return {};
  return path.substr(0, pos) + "/Contents/Frameworks";
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
  if (dir.empty())
    return {};

  const bool modern = is_arm64() && (darwin_major() >= 25);
  const std::string kk = dir + "/kosmickrisp.json";
  const std::string mvk = dir + "/moltenvk.json";
  if (modern && file_exists(kk))
  {
    log_info << "Selecting KosmicKrisp Vulkan ICD (Apple Silicon, macOS 26+)" << std::endl;
    return kk;
  }
  if (file_exists(mvk))
  {
    log_info << "Selecting MoltenVK Vulkan ICD (compatibility path)" << std::endl;
    return mvk;
  }
  // KosmicKrisp on older systems is unsupported (needs Metal 4); fall
  // back to whichever manifest exists rather than failing outright.
  if (file_exists(kk))
  {
    log_warning << "Only KosmicKrisp is bundled but this system is unsupported; trying anyway" << std::endl;
    return kk;
  }
  return {};
}

void
InitMacVulkanLoader()
{
  const std::string icd = SelectMacVulkanIcd();
  if (icd.empty())
    return;
  // Must happen before the Vulkan loader is first used (SDL device init).
  // Only VK_ICD_FILENAMES: VK_LOADER_DRIVERS_SELECT takes driver names,
  // not paths, so setting it to a path would filter out every driver.
  setenv("VK_ICD_FILENAMES", icd.c_str(), 1);
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
