//  SuperTux
//  Copyright (C) 2026 SuperTux-HD contributors
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

#include "video/gpu/gpu_video_system.hpp"

#include <sstream>

#include "supertux/gameconfig.hpp"
#include "supertux/globals.hpp"
#include "util/log.hpp"
#include "video/gpu/gpu_renderer.hpp"
#include "video/gpu/gpu_texture.hpp"
#include "video/sdl_surface_ptr.hpp"
#include "video/texture_manager.hpp"
#include "video/viewport.hpp"

void
GPUVideoSystem::DeviceDeleter::operator()(SDL_GPUDevice* d) const
{
  if (d)
    SDL_DestroyGPUDevice(d);
}

GPUVideoSystem::GPUVideoSystem() :
  m_device(),
  m_viewport(Rect(0, 0, 1920, 1080), Vector(1.0f, 1.0f)),
  m_texture_manager(),
  m_renderer(),
  m_lightmap(),
  m_clear_color(0.1f, 0.2f, 0.4f),
  m_vsync_mode(1)
{
  create_sdl_window(0);

  m_device.reset(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr));
  if (!m_device)
  {
    std::ostringstream msg;
    msg << "Couldn't create SDL_GPUDevice: " << SDL_GetError();
    SDL_ClearError();
    throw std::runtime_error(msg.str());
  }
  log_info << "SDL_GPU driver: " << SDL_GetGPUDeviceDriver(m_device.get()) << std::endl;

  if (!SDL_ClaimWindowForGPUDevice(m_device.get(), m_sdl_window.get()))
  {
    std::ostringstream msg;
    msg << "Couldn't claim window for GPU device: " << SDL_GetError();
    SDL_ClearError();
    throw std::runtime_error(msg.str());
  }

  m_texture_manager.reset(new TextureManager);
  m_renderer.reset(new GPURenderer(*this));
  m_lightmap.reset(new GPURenderer(*this));

  apply_config();
}

GPUVideoSystem::~GPUVideoSystem()
{
  m_texture_manager.reset();
  m_renderer.reset();
  m_lightmap.reset();
  if (m_device && m_sdl_window)
    SDL_ReleaseWindowFromGPUDevice(m_device.get(), m_sdl_window.get());
}

std::string
GPUVideoSystem::get_name() const
{
  std::ostringstream out;
  out << "SDL_GPU";
  if (m_device)
    out << " (" << SDL_GetGPUDeviceDriver(m_device.get()) << ")";
  return out.str();
}

Renderer&
GPUVideoSystem::get_renderer() const
{
  return *m_renderer;
}

Renderer&
GPUVideoSystem::get_lightmap() const
{
  return *m_lightmap;
}

TexturePtr
GPUVideoSystem::new_texture(const SDL_Surface& image, const Sampler& sampler)
{
  return TexturePtr(new GPUTexture(image, sampler));
}

void
GPUVideoSystem::apply_config()
{
  apply_video_mode();

#ifndef __ANDROID__
  Size target_size = (g_config->use_fullscreen && g_config->fullscreen_size != Size(0, 0)) ?
    g_config->fullscreen_size :
    g_config->window_size;
#else
  Size target_size = m_desktop_size;
#endif
  m_viewport = Viewport::from_size(target_size, m_desktop_size);
  set_vsync(g_config->vsync);
}

void
GPUVideoSystem::flip()
{
  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device.get());
  if (!cmd)
  {
    log_warning << "SDL_AcquireGPUCommandBuffer failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
    return;
  }

  SDL_GPUTexture* swapchain = nullptr;
  Uint32 w = 0, h = 0;
  if (!SDL_AcquireGPUSwapchainTexture(cmd, m_sdl_window.get(), &swapchain, &w, &h))
  {
    log_warning << "SDL_AcquireGPUSwapchainTexture failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
    SDL_CancelGPUCommandBuffer(cmd);
    return;
  }

  if (swapchain)
  {
    SDL_GPUColorTargetInfo target{};
    target.texture = swapchain;
    target.clear_color = SDL_FColor{
      m_clear_color.red, m_clear_color.green, m_clear_color.blue, m_clear_color.alpha};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
    SDL_EndGPURenderPass(pass);
  }

  if (!SDL_SubmitGPUCommandBuffer(cmd))
  {
    log_warning << "SDL_SubmitGPUCommandBuffer failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
  }
}

void
GPUVideoSystem::on_resize(int w, int h)
{
  // Swapchain images track the window automatically; just refresh viewport.
  apply_config();
}

void
GPUVideoSystem::set_vsync(int mode)
{
  m_vsync_mode = mode;
  if (!m_device || !m_sdl_window)
    return;
  SDL_GPUPresentMode present = (mode == 0) ?
    SDL_GPU_PRESENTMODE_IMMEDIATE : SDL_GPU_PRESENTMODE_VSYNC;
  if (!SDL_SetGPUSwapchainParameters(m_device.get(), m_sdl_window.get(),
                                     SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present))
  {
    log_warning << "SDL_SetGPUSwapchainParameters failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
  }
}

SDLSurfacePtr
GPUVideoSystem::make_screenshot()
{
  // Phase 0: readback arrives with the texture pipeline (Phase 1).
  log_warning << "Screenshots not supported by the GPU backend yet" << std::endl;
  return {};
}
