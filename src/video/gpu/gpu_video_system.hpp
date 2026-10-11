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

#include <memory>

#include "video/color.hpp"
#include "video/sdlbase_video_system.hpp"
#include "video/sdl_surface_ptr.hpp"
#include "video/texture_ptr.hpp"
#include "video/viewport.hpp"

class GPURenderer;
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

private:
  struct DeviceDeleter { void operator()(SDL_GPUDevice* d) const; };

  std::unique_ptr<SDL_GPUDevice, DeviceDeleter> m_device;
  Viewport m_viewport;
  std::unique_ptr<TextureManager> m_texture_manager;
  std::unique_ptr<GPURenderer> m_renderer;
  std::unique_ptr<GPURenderer> m_lightmap;
  Color m_clear_color;
  int m_vsync_mode;

private:
  GPUVideoSystem(const GPUVideoSystem&) = delete;
  GPUVideoSystem& operator=(const GPUVideoSystem&) = delete;
};
