//  SuperTux
//  Copyright (C) 2006 Matthias Braun <matze@braunis.de>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "video/video_system.hpp"

#include <assert.h>
#include <optional>
#include <config.h>
#include <iomanip>
#include <physfs.h>
#include <sstream>

#include "util/file_system.hpp"
#include "util/gettext.hpp"
#include "util/log.hpp"
#include "video/null/null_video_system.hpp"
#include "video/gpu/gpu_video_system.hpp"
#include "video/sdl_surface.hpp"
#include "video/sdl_surface_ptr.hpp"

namespace {

/// Display name of the GPU backend actually in use ("Metal",
/// "DirectX 12", "Vulkan"). Falls back to the platform default when no
/// GPU device exists yet (e.g. menus constructed before video init).
std::string
gpu_backend_display_name()
{
  if (auto* gpu = dynamic_cast<GPUVideoSystem*>(VideoSystem::current()))
  {
    if (SDL_GPUDevice* device = gpu->get_gpu_device())
    {
      const std::string driver = SDL_GetGPUDeviceDriver(device);
      if (driver == "metal")
        return "Metal";
      if (driver == "direct3d12")
        return "DirectX 12";
      if (driver == "vulkan")
        return "Vulkan";
    }
  }
#if defined(__APPLE__)
  return "Metal";
#elif defined(_WIN32)
  return "DirectX 12";
#else
  return "Vulkan";
#endif
}

} // namespace

std::unique_ptr<VideoSystem>
VideoSystem::create(VideoSystem::Enum video_system)
{
  switch (video_system)
  {
    case VIDEO_GPU:
      return std::make_unique<GPUVideoSystem>();

    case VIDEO_NULL:
      return std::make_unique<NullVideoSystem>();

    default:
      log_fatal << "invalid video system in config" << std::endl;
      assert(false);
      return {};
  }
}

VideoSystem::Enum
VideoSystem::get_video_system(const std::string &video)
{
  // SDL_GPU (Vulkan) is the only renderer; legacy names map to it so old
  // configs and command lines keep working.
  if (video == "gpu" || video == "sdl" || video == "auto" ||
      video == "opengl_auto" || video == "opengl" || video == "opengl33" ||
      video == "opengl33core" || video == "opengl20")
  {
    return VIDEO_GPU;
  }
  else if (video == "null")
  {
    return VIDEO_NULL;
  }
  else
  {
    throw std::runtime_error("invalid VideoSystem::Enum, valid values are 'gpu' and 'null'");
  }
}

std::string
VideoSystem::get_video_string(VideoSystem::Enum video)
{
  switch (video)
  {
    case VIDEO_GPU:
      return "gpu";
    case VIDEO_NULL:
      return "null";
    default:
      log_fatal << "invalid video system in config" << std::endl;
      assert(false);
      return "gpu";
  }
}

std::vector<VideoSystem::Info>
VideoSystem::get_available_video_systems()
{
  std::vector<VideoSystem::Info> output;
  // Name the real native backend (Metal / DirectX 12 / Vulkan), never a
  // translation layer. Display-only; the stored value stays VIDEO_GPU.
  const std::string backend = gpu_backend_display_name();
  output.push_back({ VIDEO_GPU, backend,
    _("Hardware-accelerated native renderer with 4K HD textures and HDR output. Active backend: ") + backend + "." });
  return output;
}

void
VideoSystem::do_take_screenshot()
{
  SDLSurfacePtr surface = make_screenshot();
  if (!surface) {
    log_warning << "Creating the screenshot has failed" << std::endl;
    return;
  }

  const std::string screenshots_dir = "/screenshots";
  if (!PHYSFS_exists(screenshots_dir.c_str())) {
    if (!PHYSFS_mkdir(screenshots_dir.c_str())) {
      log_warning << "Creating '" << screenshots_dir << "' failed" << std::endl;
      return;
    }
  }

  auto find_filename = [&]() -> std::optional<std::string>
    {
      for (int num = 0; num < 1000000; ++num)
      {
        std::ostringstream oss;
        oss << "screenshot" << std::setw(6) << std::setfill('0') << num << ".png";
        const std::string screenshot_filename = FileSystem::join(screenshots_dir, oss.str());
        if (!PHYSFS_exists(screenshot_filename.c_str())) {
          #ifdef __clang__
            return std::move(screenshot_filename);
          #else
            return screenshot_filename;
          #endif
        }
      }
      return std::nullopt;
    };

  auto filename = find_filename();
  if (!filename)
  {
    log_info << "Failed to find filename to save screenshot" << std::endl;
  }
  else
  {
    if (SDLSurface::save_png(*surface, *filename)) {
      log_info << "Wrote screenshot to \"" << *filename << "\"" << std::endl;
    }
  }
}
