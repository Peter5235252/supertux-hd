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

#pragma once

#include <SDL3/SDL.h>

#include <map>
#include <memory>
#include <tuple>

#include "video/color.hpp"
#include "video/sdlbase_video_system.hpp"
#include "video/sdl_surface_ptr.hpp"
#include "video/texture_ptr.hpp"
#include "video/viewport.hpp"

class GPUPipeline;
class GPURenderer;
class GPUTexture;
class TextureManager;

/** Phase 0 spike: SDL_GPU backend (native Vulkan on Linux/Windows).

    Owns the SDL_GPUDevice, presents the swapchain, and exposes a stub
    painter that only clears. Real drawing (textures, shaders, lightmap)
    lands in later phases; the Renderer/Painter/Texture seams already
    match what those phases need. */
class GPUVideoSystem final : public SDLBaseVideoSystem
{
public:
  GPUVideoSystem();
  ~GPUVideoSystem() override;

  virtual std::string get_name() const override;

  virtual Renderer* get_back_renderer() const override { return nullptr; }
  virtual Renderer& get_renderer() const override;
  virtual Renderer& get_lightmap() const override;

  virtual TexturePtr new_texture(const SDL_Surface& image, const Sampler& sampler = Sampler()) override;

  virtual const Viewport& get_viewport() const override { return m_viewport; }
  virtual void apply_config() override;
  virtual void flip() override;
  virtual void on_resize(int w, int h) override;

  virtual void set_vsync(int mode) override;
  virtual int get_vsync() const override { return m_vsync_mode; }

  virtual SDLSurfacePtr make_screenshot() override;

  SDL_GPUDevice* get_gpu_device() const { return m_device.get(); }
  Color& clear_color() { return m_clear_color; }

  /** Shared sampler cache (Vulkan caps sampler allocations). */
  SDL_GPUSampler* get_shared_sampler(bool linear, unsigned int wrap_s, unsigned int wrap_t);

  // Frame state (screen pass). Lightmap/backbuffer renderers are no-ops
  // until Phase 3 gives them targets.
  void begin_screen_pass();
  void end_screen_pass();
  void ensure_post_targets();
  void run_post_chain();
  void run_post_pass(bool easu,
                     SDL_GPUTexture* src, Uint32 src_w, Uint32 src_h,
                     SDL_GPUTexture* dst, Uint32 dst_w, Uint32 dst_h,
                     float stops = 0.0f);
  SDL_GPURenderPass* get_pass() const { return m_pass; }
  SDL_GPUCommandBuffer* get_cmd() const { return m_cmd; }
  GPUPipeline& pipelines();
  SDL_GPUBuffer* vertex_buffer() const { return m_vertex_buf; }

  /** FSR 1.0: render scale from config (1.0 when off/sharpen-only). */
  float fsr_render_scale() const;
  /** True when any post pass runs (upscale and/or sharpen). */
  bool fsr_post_active() const;
  /** True when EASU upscaling runs (render scale < 1). */
  bool fsr_upscale_active() const;

  /** Size the scene is rendered at (scaled when upscaling, else swapchain). */
  Uint32 render_target_width() const
  { return (fsr_post_active() && m_scene_w > 0) ? m_scene_w : m_swap_w; }
  Uint32 render_target_height() const
  { return (fsr_post_active() && m_scene_h > 0) ? m_scene_h : m_swap_h; }
  /** Viewport rect mapped into render-target pixels. */
  Rect render_viewport_rect() const;

  /** Download the last presented swapchain image as an RGBA surface.
      Returns null when no frame is available. Powers make_screenshot(). */
  SDLSurfacePtr download_swapchain();
  GPUTexture* white_texture() const;
  GPUTexture* clear_texture() const;
  Uint32 swapchain_width() const { return m_swap_w; }
  Uint32 swapchain_height() const { return m_swap_h; }

  /** Upload CPU vertex data to the shared vertex buffer; returns offset.
      Ends/re-begins the render pass around a copy pass as needed. */
  Uint32 upload_vertices(const float* data, size_t float_count);

private:
  struct DeviceDeleter { void operator()(SDL_GPUDevice* d) const; };

  static constexpr size_t STAGING_SIZE = 4 * 1024 * 1024;

  std::unique_ptr<SDL_GPUDevice, DeviceDeleter> m_device;
  Viewport m_viewport;
  std::unique_ptr<TextureManager> m_texture_manager;
  std::unique_ptr<GPURenderer> m_renderer;
  std::unique_ptr<GPURenderer> m_lightmap;
  Color m_clear_color;
  int m_vsync_mode;

  SDL_GPUCommandBuffer* m_cmd;
  SDL_GPURenderPass* m_pass;
  SDL_GPUTexture* m_swapchain;
  Uint32 m_swap_w;
  Uint32 m_swap_h;
  bool m_pass_cleared;
  // At most one frame in flight: waited at the start of flip(). This
  // makes target recreation (resize/mode change) and the upload ring
  // safe without per-resource tracking.
  SDL_GPUFence* m_inflight;
  // Target the current pass renders into (scene target when FSR upscales,
  // else the swapchain). Uploads re-begin the pass on this target.
  SDL_GPUTexture* m_active_target;
  Uint32 m_active_w;
  Uint32 m_active_h;

  SDL_GPUTransferBuffer* m_staging;
  SDL_GPUBuffer* m_vertex_buf;
  size_t m_vertex_cursor;

  std::unique_ptr<GPUPipeline> m_pipelines;
  TexturePtr m_white_texture;
  TexturePtr m_clear_texture;
  std::map<std::tuple<bool, unsigned int, unsigned int>, SDL_GPUSampler*> m_shared_samplers;

  // FSR 1.0 targets: scene (render scale) + intermediate (display size).
  SDL_GPUTexture* m_scene_target;
  SDL_GPUTexture* m_fsr_intermediate;
  Uint32 m_scene_w;
  Uint32 m_scene_h;
private:
  GPUVideoSystem(const GPUVideoSystem&) = delete;
  GPUVideoSystem& operator=(const GPUVideoSystem&) = delete;
};
