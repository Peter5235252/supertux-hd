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

#include <vector>

#include "math/rectf.hpp"
#include "math/vector.hpp"
#include "video/blend.hpp"
#include "video/color.hpp"
#include "video/painter.hpp"

class GPUTexture;
class GPUVideoSystem;

/** Phase 2: real painter. Quad math mirrors GLPainter (dst corners,
    flip UV swaps, center rotation); vertices stream through the shared
    upload ring and draw with the cached textured-quad pipeline. */
class GPUPainter final : public Painter
{
public:
  explicit GPUPainter(GPUVideoSystem& video_system);
  ~GPUPainter() override {}

  virtual void draw_texture(const DrawingRequest& request) override;
  virtual void draw_gradient(const DrawingRequest& request) override;
  virtual void draw_filled_rect(const DrawingRequest& request) override;
  virtual void draw_inverse_ellipse(const DrawingRequest& request) override;
  virtual void draw_line(const DrawingRequest& request) override;
  virtual void draw_triangle(const DrawingRequest& request) override;

  virtual void clear(const Color& color) override;

  virtual void get_pixel(const DrawingRequest& request) const override {}

  virtual void set_clip_rect(const Rect& rect) override;
  virtual void clear_clip_rect() override {}

private:
  struct Vertex { float x, y, u, v, r, g, b, a; };

  void push_quad(std::vector<float>& out,
                 float left, float top, float right, float bottom,
                 float uv_left, float uv_top, float uv_right, float uv_bottom,
                 const Color& color, float alpha);
  void push_quad_wrapper(const DrawingRequest& draw_req, const Rectf& rect, const Color& color);
  void draw_vertices(Blend blend, bool lines,
                     const GPUTexture* diffuse, const GPUTexture* displacement,
                     const Vector& animate, int blur,
                     const float* data, size_t float_count);
  void apply_scissor(SDL_GPURenderPass* pass) const;

private:
  GPUVideoSystem& m_video_system;
  SDL_Rect m_clip_rect;
  bool m_clip_enabled;

private:
  GPUPainter(const GPUPainter&) = delete;
  GPUPainter& operator=(const GPUPainter&) = delete;
};
