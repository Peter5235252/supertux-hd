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

#include <cstring>
#include <map>
#include <sstream>
#include <tuple>

#include "supertux/gameconfig.hpp"
#include "supertux/globals.hpp"
#include "util/log.hpp"
#include "video/blend.hpp"
#include "video/gpu/gpu_pipeline.hpp"
#include "video/gpu/gpu_renderer.hpp"
#include "video/gpu/gpu_texture.hpp"
#include "video/sampler.hpp"
#include "video/sdl_surface.hpp"
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
  m_vsync_mode(1),
  m_cmd(nullptr),
  m_pass(nullptr),
  m_swapchain(nullptr),
  m_swap_w(0),
  m_swap_h(0),
  m_pass_cleared(false),
  m_staging(nullptr),
  m_vertex_buf(nullptr),
  m_vertex_cursor(0),
  m_pipelines(),
  m_white_texture(),
  m_clear_texture()
{
  create_sdl_window(0);

  // Pin the Vulkan driver explicitly: SDL_GPU must use native Vulkan on
  // both Linux and Windows (Auto-select could pick D3D12/Metal elsewhere).
  // If no Vulkan driver exists, device creation fails loudly below.
  SDL_SetHint(SDL_HINT_GPU_DRIVER, "vulkan");

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
  m_renderer.reset(new GPURenderer(*this, true));
  m_lightmap.reset(new GPURenderer(*this, false));

  SDL_GPUBufferCreateInfo vbuf_info{};
  vbuf_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
  vbuf_info.size = static_cast<Uint32>(STAGING_SIZE);
  m_vertex_buf = SDL_CreateGPUBuffer(m_device.get(), &vbuf_info);
  if (!m_vertex_buf)
    throw std::runtime_error("Couldn't create GPU vertex buffer");

  SDL_GPUTransferBufferCreateInfo stage_info{};
  stage_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  stage_info.size = static_cast<Uint32>(STAGING_SIZE);
  m_staging = SDL_CreateGPUTransferBuffer(m_device.get(), &stage_info);
  if (!m_staging)
    throw std::runtime_error("Couldn't create GPU staging buffer");

  const SDL_GPUTextureFormat target_format =
    SDL_GetGPUSwapchainTextureFormat(m_device.get(), m_sdl_window.get());
  m_pipelines.reset(new GPUPipeline(m_device.get(), target_format));

  auto make_solid = [this](Uint8 fill) {
    SDLSurfacePtr surface = SDLSurface::create_rgba(1, 1);
    memset(surface->pixels, fill, 4);
    TexturePtr tex(new GPUTexture(this, *surface, Sampler()));
    return tex;
  };
  m_white_texture = make_solid(0xFF);
  m_clear_texture = make_solid(0x00);

  apply_config();
}

GPUVideoSystem::~GPUVideoSystem()
{
  m_texture_manager.reset();
  m_renderer.reset();
  m_lightmap.reset();
  m_white_texture.reset();
  m_clear_texture.reset();
  m_pipelines.reset();
  for (const auto& entry : m_shared_samplers)
    SDL_ReleaseGPUSampler(m_device.get(), entry.second);
  m_shared_samplers.clear();
  if (m_vertex_buf)
    SDL_ReleaseGPUBuffer(m_device.get(), m_vertex_buf);
  if (m_staging)
    SDL_ReleaseGPUTransferBuffer(m_device.get(), m_staging);
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
  return TexturePtr(new GPUTexture(this, image, sampler));
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
GPUVideoSystem::begin_screen_pass()
{
  if (m_cmd)
    return;

  m_cmd = SDL_AcquireGPUCommandBuffer(m_device.get());
  if (!m_cmd)
  {
    log_warning << "SDL_AcquireGPUCommandBuffer failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
    return;
  }

  m_swapchain = nullptr;
  m_swap_w = 0;
  m_swap_h = 0;
  if (!SDL_AcquireGPUSwapchainTexture(m_cmd, m_sdl_window.get(), &m_swapchain, &m_swap_w, &m_swap_h))
  {
    log_warning << "SDL_AcquireGPUSwapchainTexture failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
    SDL_CancelGPUCommandBuffer(m_cmd);
    m_cmd = nullptr;
    return;
  }

  if (!m_swapchain)
    return; // Minimized; flip() submits the empty command buffer.

  {
    static bool s_fmt_logged = false;
    if (!s_fmt_logged)
    {
      s_fmt_logged = true;
      log_info << "GPUSWAPCHAIN format="
               << static_cast<int>(SDL_GetGPUSwapchainTextureFormat(m_device.get(), m_sdl_window.get()))
               << " size=" << m_swap_w << "x" << m_swap_h << std::endl;
    }
  }

  SDL_GPUColorTargetInfo target{};
  target.texture = m_swapchain;
  target.clear_color = SDL_FColor{
    m_clear_color.red, m_clear_color.green, m_clear_color.blue, m_clear_color.alpha};
  target.load_op = SDL_GPU_LOADOP_CLEAR;
  target.store_op = SDL_GPU_STOREOP_STORE;
  m_pass = SDL_BeginGPURenderPass(m_cmd, &target, 1, nullptr);
  m_pass_cleared = true;
}

void
GPUVideoSystem::end_screen_pass()
{
  if (m_pass)
  {
    SDL_EndGPURenderPass(m_pass);
    m_pass = nullptr;
  }
}

void
GPUVideoSystem::flip()
{
  end_screen_pass();
  if (!m_cmd)
    return;
  if (!SDL_SubmitGPUCommandBuffer(m_cmd))
  {
    log_warning << "SDL_SubmitGPUCommandBuffer failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
  }
  m_cmd = nullptr;
  // NOTE: m_swapchain/m_swap_w/h intentionally survive flip: event polled
  // screenshots (F12) run between frames and need the last presented image.
  // begin_screen_pass() overwrites them on the next frame.
}

Uint32
GPUVideoSystem::upload_vertices(const float* data, size_t float_count)
{
  const size_t byte_count = float_count * sizeof(float);
  if (byte_count > STAGING_SIZE)
    throw std::runtime_error("Vertex upload exceeds staging buffer");

  if (m_vertex_cursor + byte_count > STAGING_SIZE)
    m_vertex_cursor = 0;

  void* mapped = SDL_MapGPUTransferBuffer(m_device.get(), m_staging, true);
  if (!mapped)
    throw std::runtime_error("SDL_MapGPUTransferBuffer failed");
  memcpy(static_cast<Uint8*>(mapped) + m_vertex_cursor, data, byte_count);
  SDL_UnmapGPUTransferBuffer(m_device.get(), m_staging);

  // A copy pass cannot run inside a render pass: close it, copy, reopen.
  const bool had_pass = (m_pass != nullptr);
  if (had_pass)
    end_screen_pass();

  SDL_GPUCommandBuffer* cmd = m_cmd;
  bool own_cmd = false;
  if (!cmd)
  {
    cmd = SDL_AcquireGPUCommandBuffer(m_device.get());
    own_cmd = true;
  }

  SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cmd);
  SDL_GPUTransferBufferLocation src{};
  src.transfer_buffer = m_staging;
  src.offset = static_cast<Uint32>(m_vertex_cursor);
  SDL_GPUBufferRegion dst{};
  dst.buffer = m_vertex_buf;
  dst.offset = static_cast<Uint32>(m_vertex_cursor);
  dst.size = static_cast<Uint32>(byte_count);
  SDL_UploadToGPUBuffer(pass, &src, &dst, false);
  SDL_EndGPUCopyPass(pass);

  if (own_cmd)
  {
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence)
    {
      SDL_WaitForGPUFences(m_device.get(), true, &fence, 1);
      SDL_ReleaseGPUFence(m_device.get(), fence);
    }
  }
  else if (had_pass && m_swapchain)
  {
    SDL_GPUColorTargetInfo target{};
    target.texture = m_swapchain;
    target.load_op = SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;
    m_pass = SDL_BeginGPURenderPass(m_cmd, &target, 1, nullptr);
    m_pass_cleared = true; // Already cleared this frame; keep pixels.
  }

  const Uint32 offset = static_cast<Uint32>(m_vertex_cursor);
  m_vertex_cursor += byte_count;
  return offset;
}

GPUPipeline&
GPUVideoSystem::pipelines()
{
  return *m_pipelines;
}

GPUTexture*
GPUVideoSystem::white_texture() const
{
  return static_cast<GPUTexture*>(m_white_texture.get());
}

GPUTexture*
GPUVideoSystem::clear_texture() const
{
  return static_cast<GPUTexture*>(m_clear_texture.get());
}

SDL_GPUSampler*
GPUVideoSystem::get_shared_sampler(bool linear, unsigned int wrap_s, unsigned int wrap_t)
{
  const auto key = std::make_tuple(linear, wrap_s, wrap_t);
  auto it = m_shared_samplers.find(key);
  if (it != m_shared_samplers.end())
    return it->second;

  auto wrap = [](unsigned int w) {
    if (w == GL_REPEAT)
      return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    if (w == GL_MIRRORED_REPEAT)
      return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
    return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  };

  SDL_GPUSamplerCreateInfo info{};
  info.min_filter = linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
  info.mag_filter = linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
  info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  info.address_mode_u = wrap(wrap_s);
  info.address_mode_v = wrap(wrap_t);
  info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  info.max_anisotropy = 1.0f;
  info.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
  info.min_lod = 0.0f;
  info.max_lod = 1000.0f;
  info.enable_anisotropy = false;
  info.enable_compare = false;
  SDL_GPUSampler* sampler = SDL_CreateGPUSampler(m_device.get(), &info);
  if (!sampler)
  {
    log_warning << "SDL_CreateGPUSampler failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
    return nullptr;
  }
  m_shared_samplers[key] = sampler;
  return sampler;
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
GPUVideoSystem::download_swapchain()
{
  if (!m_swapchain || m_swap_w == 0 || m_swap_h == 0)
    return {};

  const Uint32 w = m_swap_w;
  const Uint32 h = m_swap_h;

  SDL_GPUTransferBufferCreateInfo dl_info{};
  dl_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
  dl_info.size = w * h * 4;
  SDL_GPUTransferBuffer* download = SDL_CreateGPUTransferBuffer(m_device.get(), &dl_info);
  if (!download)
    return {};

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device.get());
  if (!cmd)
  {
    SDL_ReleaseGPUTransferBuffer(m_device.get(), download);
    return {};
  }

  SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
  SDL_GPUTextureRegion src{};
  src.texture = m_swapchain;
  src.w = w;
  src.h = h;
  src.d = 1;
  SDL_GPUTextureTransferInfo dst{};
  dst.transfer_buffer = download;
  dst.pixels_per_row = w;
  dst.rows_per_layer = h;
  SDL_DownloadFromGPUTexture(copy, &src, &dst);
  SDL_EndGPUCopyPass(copy);

  SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
  if (fence)
  {
    SDL_WaitForGPUFences(m_device.get(), true, &fence, 1);
    SDL_ReleaseGPUFence(m_device.get(), fence);
  }

  SDLSurfacePtr result;
  if (void* mapped = SDL_MapGPUTransferBuffer(m_device.get(), download, false))
  {
    // Swapchain is B8G8R8A8: swizzle to RGBA for the PNG.
    SDLSurfacePtr surface = SDLSurface::create_rgba(static_cast<int>(w), static_cast<int>(h));
    const Uint8* src_bytes = static_cast<const Uint8*>(mapped);
    Uint8* dst_bytes = static_cast<Uint8*>(surface->pixels);
    for (Uint32 i = 0; i < w * h; ++i)
    {
      dst_bytes[0] = src_bytes[2];
      dst_bytes[1] = src_bytes[1];
      dst_bytes[2] = src_bytes[0];
      dst_bytes[3] = src_bytes[3];
      src_bytes += 4;
      dst_bytes += 4;
    }
    SDL_UnmapGPUTransferBuffer(m_device.get(), download);
    result = std::move(surface);
  }
  SDL_ReleaseGPUTransferBuffer(m_device.get(), download);
  return result;
}

SDLSurfacePtr
GPUVideoSystem::make_screenshot()
{
  return download_swapchain();
}
