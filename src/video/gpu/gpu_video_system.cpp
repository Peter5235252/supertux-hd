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
#include <cstdlib>
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
  m_inflight(nullptr),
  m_active_target(nullptr),
  m_active_w(0),
  m_active_h(0),
  m_staging(nullptr),
  m_vertex_buf(nullptr),
  m_vertex_cursor(0),
  m_pipelines(),
  m_white_texture(),
  m_clear_texture(),
  m_shared_samplers(),
  m_scene_target(nullptr),
  m_fsr_intermediate(nullptr),
  m_scene_w(0),
  m_scene_h(0),
  m_post_format(SDL_GPU_TEXTUREFORMAT_INVALID),
  m_hdr_active(false)
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
#ifdef __APPLE__
  // The ICD selection itself happens before the log system is up
  // (mac_vulkan_select.cpp), so report the effective manifest here where
  // CI and users can see it.
  if (const char* icd = std::getenv("VK_ICD_FILENAMES"))
    log_info << "Vulkan ICD: " << icd << std::endl;
#endif

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
  if (m_scene_target)
    SDL_ReleaseGPUTexture(m_device.get(), m_scene_target);
  if (m_fsr_intermediate)
    SDL_ReleaseGPUTexture(m_device.get(), m_fsr_intermediate);
  m_scene_target = nullptr;
  m_fsr_intermediate = nullptr;
  for (const auto& entry : m_shared_samplers)
    SDL_ReleaseGPUSampler(m_device.get(), entry.second);
  m_shared_samplers.clear();
  if (m_inflight)
  {
    SDL_WaitForGPUFences(m_device.get(), true, &m_inflight, 1);
    SDL_ReleaseGPUFence(m_device.get(), m_inflight);
    m_inflight = nullptr;
  }
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
  // HDR after vsync: set_hdr() re-applies the stored present mode, and
  // forces the toggle back off when the display refuses the composition.
  if (g_config->use_hdr != m_hdr_active)
    g_config->use_hdr = set_hdr(g_config->use_hdr);
}

float
GPUVideoSystem::fsr_render_scale() const
{
  if (!g_config)
    return 1.0f;
  switch (g_config->fsr_mode)
  {
    case 1: return 0.77f; // Ultra quality
    case 2: return 0.67f; // Quality
    case 3: return 0.59f; // Balanced
    case 4: return 0.50f; // Performance
    default: break; // 0 = off, 5 = sharpen only (full res)
  }
  return 1.0f;
}

bool
GPUVideoSystem::fsr_post_active() const
{
  return g_config && g_config->fsr_mode >= 1 && g_config->fsr_mode <= 5;
}

bool
GPUVideoSystem::fsr_upscale_active() const
{
  return g_config && g_config->fsr_mode >= 1 && g_config->fsr_mode <= 4;
}

Rect
GPUVideoSystem::render_viewport_rect() const
{
  const Rect& vprect = m_viewport.get_rect();
  if (!fsr_post_active() || m_swap_w == 0 || m_swap_h == 0)
    return vprect;
  const float k = static_cast<float>(m_scene_w) / static_cast<float>(m_swap_w);
  return Rect(static_cast<int>(vprect.left * k), static_cast<int>(vprect.top * k),
              static_cast<int>(vprect.right * k), static_cast<int>(vprect.bottom * k));
}

void
GPUVideoSystem::ensure_post_targets()
{
  const float scale = fsr_render_scale();
  const Uint32 want_w = std::max<Uint32>(1, static_cast<Uint32>(m_swap_w * scale));
  const Uint32 want_h = std::max<Uint32>(1, static_cast<Uint32>(m_swap_h * scale));
  const SDL_GPUTextureFormat want_fmt =
    SDL_GetGPUSwapchainTextureFormat(m_device.get(), m_sdl_window.get());
  if (m_scene_target && want_w == m_scene_w && want_h == m_scene_h &&
      m_fsr_intermediate && want_fmt == m_post_format)
    return;

  if (m_scene_target)
    SDL_ReleaseGPUTexture(m_device.get(), m_scene_target);
  if (m_fsr_intermediate)
    SDL_ReleaseGPUTexture(m_device.get(), m_fsr_intermediate);
  m_scene_target = nullptr;
  m_fsr_intermediate = nullptr;

  const SDL_GPUTextureFormat fmt =
    SDL_GetGPUSwapchainTextureFormat(m_device.get(), m_sdl_window.get());
  auto make_target = [&](Uint32 w, Uint32 h, SDL_GPUTextureFormat format) {    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = static_cast<SDL_GPUTextureUsageFlags>(
      SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
    info.width = w;
    info.height = h;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    SDL_GPUTexture* tex = SDL_CreateGPUTexture(m_device.get(), &info);
    if (!tex)
      throw std::runtime_error("Couldn't create FSR render target");
    return tex;
  };
  m_scene_target = make_target(want_w, want_h, fmt);
  // EASU emits negative lobes; the EASU->RCAS intermediate must be FP16
  // or they clip to zero and RCAS reconstructs grid artifacts from them.
  m_fsr_intermediate = make_target(m_swap_w, m_swap_h,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT);
  m_scene_w = want_w;
  m_scene_h = want_h;
  m_post_format = fmt;
}

void
GPUVideoSystem::run_post_chain()
{
  // EASU upscales scene -> intermediate (display size), then RCAS
  // sharpens intermediate (or scene, in sharpen-only mode) -> swapchain.
  SDL_GPUTexture* easu_src = m_scene_target;
  SDL_GPUTexture* rcas_src = m_fsr_intermediate;
  Uint32 rcas_w = m_swap_w;
  Uint32 rcas_h = m_swap_h;
  if (fsr_upscale_active())
  {
    run_post_pass(true, easu_src, m_scene_w, m_scene_h,
                  m_fsr_intermediate, m_swap_w, m_swap_h);
  }
  else
  {
    rcas_src = m_scene_target;
    rcas_w = m_scene_w;
    rcas_h = m_scene_h;
  }

  const float sharp = g_config ? g_config->fsr_sharpness : 0.5f;
  const float stops = (1.0f - std::min(1.0f, std::max(0.0f, sharp))) * 2.0f;
  run_post_pass(false, rcas_src, rcas_w, rcas_h,
                m_swapchain, m_swap_w, m_swap_h, stops);
}

void
GPUVideoSystem::run_post_pass(bool easu,
                             SDL_GPUTexture* src, Uint32 src_w, Uint32 src_h,
                             SDL_GPUTexture* dst, Uint32 dst_w, Uint32 dst_h,
                             float stops)
{
  SDL_GPUColorTargetInfo target{};
  target.texture = dst;
  target.load_op = SDL_GPU_LOADOP_DONT_CARE;
  target.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_cmd, &target, 1, nullptr);

  SDL_BindGPUGraphicsPipeline(pass, m_pipelines->for_post(easu));

  SDL_GPUSampler* sampler = get_shared_sampler(true, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
  SDL_GPUTextureSamplerBinding binding{src, sampler};
  SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);

  if (easu)
  {
    // inputViewport/inputSize/outputSize, 6 floats.
    const float fw = static_cast<float>(src_w);
    const float fh = static_cast<float>(src_h);
    const float dw = static_cast<float>(dst_w);
    const float dh = static_cast<float>(dst_h);
    const float ubo[6] = {fw, fh, fw, fh, dw, dh};
    SDL_PushGPUFragmentUniformData(m_cmd, 0, ubo, sizeof(ubo));
  }
  else
  {
    // RCAS tail: sharpness stops + HDR output transform. SDR keeps the
    // exact legacy behavior (hdrEnabled = 0 leaves values untouched).
    const SDL_GPUTextureFormat swap_fmt =
      SDL_GetGPUSwapchainTextureFormat(m_device.get(), m_sdl_window.get());
    const float hdr_on = (m_hdr_active &&
      swap_fmt == SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM) ? 1.0f : 0.0f;
    const float headroom = g_config ? g_config->hdr_headroom : 2.0f;
    const float ubo[4] = {stops, hdr_on, headroom, 0.0f};
    SDL_PushGPUFragmentUniformData(m_cmd, 0, ubo, sizeof(ubo));
  }

  SDL_GPUViewport vp{0.0f, 0.0f,
    static_cast<float>(dst_w), static_cast<float>(dst_h), 0.0f, 1.0f};
  SDL_SetGPUViewport(pass, &vp);
  SDL_Rect sc{0, 0, static_cast<int>(dst_w), static_cast<int>(dst_h)};
  SDL_SetGPUScissor(pass, &sc);

  SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
  SDL_EndGPURenderPass(pass);
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

  // With FSR active the scene renders into the (smaller) scene target;
  // the swapchain is only touched by the post chain in flip().
  SDL_GPUTexture* target_tex = m_swapchain;
  if (fsr_post_active())
  {
    ensure_post_targets();
    target_tex = m_scene_target;
  }

  SDL_GPUColorTargetInfo target{};
  target.texture = target_tex;
  target.clear_color = SDL_FColor{
    m_clear_color.red, m_clear_color.green, m_clear_color.blue, m_clear_color.alpha};
  target.load_op = SDL_GPU_LOADOP_CLEAR;
  target.store_op = SDL_GPU_STOREOP_STORE;
  m_pass = SDL_BeginGPURenderPass(m_cmd, &target, 1, nullptr);
  m_pass_cleared = true;
  m_active_target = target_tex;
  if (target_tex == m_swapchain)
  {
    m_active_w = m_swap_w;
    m_active_h = m_swap_h;
  }
  else
  {
    m_active_w = m_scene_w;
    m_active_h = m_scene_h;
  }
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

  if (fsr_post_active() && m_swapchain)
    run_post_chain();
  SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(m_cmd);
  m_cmd = nullptr;
  if (m_inflight)
  {
    SDL_WaitForGPUFences(m_device.get(), true, &m_inflight, 1);
    SDL_ReleaseGPUFence(m_device.get(), m_inflight);
    m_inflight = nullptr;
  }
  // NOTE: m_swapchain/m_swap_w/h intentionally survive flip: event polled
  // screenshots (F12) run between frames and need the last presented image.
  // begin_screen_pass() overwrites them on the next frame.
  m_inflight = fence;
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
  else if (had_pass && m_active_target)
  {
    SDL_GPUColorTargetInfo target{};
    target.texture = m_active_target;
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
  apply_swapchain();
}

SDL_GPUPresentMode
GPUVideoSystem::current_present_mode() const
{
  return (m_vsync_mode == 0) ?
    SDL_GPU_PRESENTMODE_IMMEDIATE : SDL_GPU_PRESENTMODE_VSYNC;
}

void
GPUVideoSystem::apply_swapchain()
{
  if (!m_device || !m_sdl_window)
    return;
  const SDL_GPUSwapchainComposition composition = m_hdr_active ?
    SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2084 : SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
  if (!SDL_SetGPUSwapchainParameters(m_device.get(), m_sdl_window.get(),
                                     composition, current_present_mode()))
  {
    log_warning << "SDL_SetGPUSwapchainParameters failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
  }
}

bool
GPUVideoSystem::is_hdr_supported() const
{
  if (!m_sdl_window)
    return false;
  const SDL_DisplayID display = SDL_GetDisplayForWindow(m_sdl_window.get());
  if (display == 0)
    return false;
  const SDL_PropertiesID props = SDL_GetDisplayProperties(display);
  if (props == 0)
    return false;
  return SDL_GetBooleanProperty(props, SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, false);
}

bool
GPUVideoSystem::set_hdr(bool on)
{
  if (on && !is_hdr_supported())
  {
    log_info << "HDR requested but the display reports no HDR support; staying SDR" << std::endl;
    return false;
  }
  if (on == m_hdr_active)
    return m_hdr_active;
  // At most one frame is ever in flight (fenced in flip()), so it is
  // safe to swap composition plus pipelines/targets synchronously here.
  const bool was_active = m_hdr_active;
  m_hdr_active = on;
  apply_swapchain();
  const SDL_GPUTextureFormat fmt =
    SDL_GetGPUSwapchainTextureFormat(m_device.get(), m_sdl_window.get());
  const bool hdr_format =
    (fmt == SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM);
  if (on && !hdr_format)
  {
    log_warning << "HDR composition accepted but swapchain format stayed SDR; reverting" << std::endl;
    m_hdr_active = was_active;
    apply_swapchain();
    return m_hdr_active;
  }
  // Pipelines bake the swapchain (target) format: rebuild them, and drop
  // the post targets so they are recreated in the new format on demand.
  m_pipelines.reset(new GPUPipeline(m_device.get(), fmt));
  if (m_scene_target)
  {
    SDL_ReleaseGPUTexture(m_device.get(), m_scene_target);
    m_scene_target = nullptr;
  }
  if (m_fsr_intermediate)
  {
    SDL_ReleaseGPUTexture(m_device.get(), m_fsr_intermediate);
    m_fsr_intermediate = nullptr;
  }
  log_info << "HDR output " << (m_hdr_active ? "enabled (HDR10 ST2084)" : "disabled (SDR)") << std::endl;
  return m_hdr_active;
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
    const SDL_GPUTextureFormat swap_fmt =
      SDL_GetGPUSwapchainTextureFormat(m_device.get(), m_sdl_window.get());
    // HDR10 swapchain is R10G10B10A2: R bits 0-9, G 10-19, B 20-29.
    const bool hdr = (swap_fmt == SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM);
    SDLSurfacePtr surface = SDLSurface::create_rgba(static_cast<int>(w), static_cast<int>(h));
    if (!hdr)
    {
      // Swapchain is B8G8R8A8: swizzle to RGBA for the PNG.
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
    }
    else
    {
      // HDR10 swapchain: 10-bit PQ in a BT.2020 container. Convert back
      // to sRGB so screenshots look correct on SDR viewers.
      const float headroom = g_config ? g_config->hdr_headroom : 2.0f;
      const Uint32* src_px = static_cast<const Uint32*>(mapped);
      Uint8* dst_bytes = static_cast<Uint8*>(surface->pixels);
      for (Uint32 i = 0; i < w * h; ++i)
      {
        const Uint32 px = src_px[i];
        float pr = static_cast<float>(px & 0x3FF) / 1023.0f;
        float pg = static_cast<float>((px >> 10) & 0x3FF) / 1023.0f;
        float pb = static_cast<float>((px >> 20) & 0x3FF) / 1023.0f;
        // Inverse sRGB->BT.2020 (BT.2087). Rows (white-preserving):
        // [1.6605 -0.5879 -0.0728 / -0.1246 1.1329 -0.0083 /
        //  -0.0182 -0.1006 1.1187].
        const float lr =  1.6605f * pr - 0.5879f * pg - 0.0728f * pb;
        const float lg = -0.1246f * pr + 1.1329f * pg - 0.0083f * pb;
        const float lb = -0.0182f * pr - 0.1006f * pg + 1.1187f * pb;
        // PQ EOTF to linear nits, then back to SDR-white-relative linear.
        auto pq_eotf = [](float n) -> float {
          const float m1 = 0.1593017578125f, m2 = 78.84375f;
          const float c1 = 0.8359375f, c2 = 18.8515625f, c3 = 18.6875f;
          float y = std::pow(n, 1.0f / m2);
          float num = std::max(y - c1, 0.0f);
          float den = c2 - c3 * y;
          float l = (den <= 0.0f) ? 1.0f : num / den;
          return std::pow(std::max(l, 0.0f), 1.0f / m1) * 10000.0f;
        };
        auto to_srgb = [](float l) -> float {
          l = std::max(l, 0.0f);
          return (l <= 0.0031308f) ? l * 12.92f
                                   : 1.055f * std::pow(l, 1.0f / 2.4f) - 0.055f;
        };
        const float inv = 1.0f / (100.0f * headroom);
        dst_bytes[0] = static_cast<Uint8>(std::min(255.0f, to_srgb(pq_eotf(lr) * inv) * 255.0f));
        dst_bytes[1] = static_cast<Uint8>(std::min(255.0f, to_srgb(pq_eotf(lg) * inv) * 255.0f));
        dst_bytes[2] = static_cast<Uint8>(std::min(255.0f, to_srgb(pq_eotf(lb) * inv) * 255.0f));
        dst_bytes[3] = 255;
        dst_bytes += 4;
      }
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
