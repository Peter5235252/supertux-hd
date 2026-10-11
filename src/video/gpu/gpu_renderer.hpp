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

#include "video/renderer.hpp"

class GPUVideoSystem;
class GPUPainter;

/** Phase 0: Renderer stub. start/end_draw are no-ops; all pixels come
    from GPUVideoSystem::flip() clearing the swapchain. */
class GPURenderer final : public Renderer
{
public:
  /** @param is_screen  false for lightmap/backbuffer renderers, which stay
      no-ops until Phase 3 gives them texture targets. */
  GPURenderer(GPUVideoSystem& video_system, bool is_screen);
  ~GPURenderer() override;

  virtual void start_draw() override;
  virtual void end_draw() override;

  virtual Painter& get_painter() override;

  virtual Rect get_rect() const override;
  virtual Size get_logical_size() const override;

  virtual TexturePtr get_texture() const override { return {}; }

private:
  GPUVideoSystem& m_video_system;
  const bool m_is_screen;
  std::unique_ptr<GPUPainter> m_painter;

private:
  GPURenderer(const GPURenderer&) = delete;
  GPURenderer& operator=(const GPURenderer&) = delete;
};
