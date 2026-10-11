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

#include "video/gpu/gpu_painter.hpp"

#include <cmath>

#include "math/util.hpp"
#include "supertux/globals.hpp"
#include "util/log.hpp"
#include "video/drawing_request.hpp"
#include "video/flip.hpp"
#include "video/gpu/gpu_pipeline.hpp"
#include "video/gpu/gpu_texture.hpp"
#include "video/gpu/gpu_video_system.hpp"
#include "video/gradient.hpp"

namespace {

// std140 fragment block: mat3 (48B) + backbuffer/game_time/animate(16B)
// + displacement_animate/is_displacement/blur (16B) = 80 bytes.
struct FragUniforms
{
  float f2uv[12];
  float backbuffer;
  float game_time;
  float animate_x;
  float animate_y;
  float disp_x;
  float disp_y;
  int32_t is_displacement;
  int32_t blur;
};
static_assert(sizeof(FragUniforms) == 80, "FragUniforms must be 80 bytes");

} // namespace

GPUPainter::GPUPainter(GPUVideoSystem& video_system) :
  m_video_system(video_system),
  m_clip_rect{0, 0, 0, 0},
  m_clip_enabled(false)
{
}

void
GPUPainter::clear(const Color& color)
{
  m_video_system.clear_color() = color;
}

void
GPUPainter::set_clip_rect(const Rect& rect)
{
  m_clip_rect = rect.to_sdl();
  m_clip_enabled = true;
}

void
GPUPainter::apply_scissor(SDL_GPURenderPass* pass) const
{
  if (m_clip_enabled)
  {
    SDL_SetGPUScissor(pass, &m_clip_rect);
  }
  else
  {
    SDL_Rect full{0, 0,
      static_cast<int>(m_video_system.swapchain_width()),
      static_cast<int>(m_video_system.swapchain_height())};
    SDL_SetGPUScissor(pass, &full);
  }
}

void
GPUPainter::push_quad(std::vector<float>& out,
                      float left, float top, float right, float bottom,
                      float uv_left, float uv_top, float uv_right, float uv_bottom,
                      const Color& color, float alpha)
{
  const float verts[6][8] = {
    {left,  top,    uv_left,  uv_top,    color.red, color.green, color.blue, color.alpha * alpha},
    {right, top,    uv_right, uv_top,    color.red, color.green, color.blue, color.alpha * alpha},
    {right, bottom, uv_right, uv_bottom, color.red, color.green, color.blue, color.alpha * alpha},
    {left,  bottom, uv_left,  uv_bottom, color.red, color.green, color.blue, color.alpha * alpha},
    {left,  top,    uv_left,  uv_top,    color.red, color.green, color.blue, color.alpha * alpha},
    {right, bottom, uv_right, uv_bottom, color.red, color.green, color.blue, color.alpha * alpha},
  };
  for (const auto& v : verts)
    out.insert(out.end(), v, v + 8);
}

void
GPUPainter::draw_vertices(Blend blend, bool lines,
                          const GPUTexture* diffuse, const GPUTexture* displacement,
                          const Vector& animate, int blur,
                          const float* data, size_t float_count)
{
  SDL_GPURenderPass* pass = m_video_system.get_pass();
  SDL_GPUCommandBuffer* cmd = m_video_system.get_cmd();
  if (!pass || !cmd || !diffuse || !diffuse->get_handle())
    return;

  const Uint32 w = m_video_system.swapchain_width();
  const Uint32 h = m_video_system.swapchain_height();
  if (w == 0 || h == 0)
    return;

  // The vertex shader takes NDC directly (no vertex uniforms, since SDL
  // won't share uniform binding 0 across stages). Convert physical
  // y-down pixels to NDC here. Empirically (XWayland/NVIDIA present),
  // NDC(-1,-1) lands bottom-left, so y-down pixels need ndc_y = 1-2y/h.
  const float fw = static_cast<float>(w);
  const float fh = static_cast<float>(h);
  std::vector<float> ndc(data, data + float_count);
  for (size_t i = 0; i < float_count; i += 8)
  {
    ndc[i] = 2.0f * ndc[i] / fw - 1.0f;
    ndc[i + 1] = 1.0f - 2.0f * ndc[i + 1] / fh;
  }

  // Uploading closes and reopens the pass; fetch it afterwards.
  const Uint32 offset = m_video_system.upload_vertices(ndc.data(), float_count);
  pass = m_video_system.get_pass();
  if (!pass)
    return;

  SDL_BindGPUGraphicsPipeline(pass, m_video_system.pipelines().for_blend(blend, lines));

  SDL_GPUBufferBinding vb{};
  vb.buffer = m_video_system.vertex_buffer();
  vb.offset = offset;
  SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

  const GPUTexture* disp_tex = displacement && displacement->get_handle() ?
    displacement : m_video_system.clear_texture();
  const GPUTexture* fb_tex = m_video_system.clear_texture();
  SDL_GPUTextureSamplerBinding samplers[3] = {
    {const_cast<GPUTexture*>(diffuse)->get_handle(), const_cast<GPUTexture*>(diffuse)->get_sampler_handle()},
    {const_cast<GPUTexture*>(disp_tex)->get_handle(), const_cast<GPUTexture*>(disp_tex)->get_sampler_handle()},
    {const_cast<GPUTexture*>(fb_tex)->get_handle(), const_cast<GPUTexture*>(fb_tex)->get_sampler_handle()},
  };
  SDL_BindGPUFragmentSamplers(pass, 0, samplers, 3);

  FragUniforms frag{};
  const float inv_w = 1.0f / static_cast<float>(w);
  const float inv_h = 1.0f / static_cast<float>(h);
  const float f2uv[12] = {
    inv_w, 0.0f, 0.0f, 0.0f,
    0.0f, inv_h, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
  };
  memcpy(frag.f2uv, f2uv, sizeof(f2uv));
  frag.backbuffer = 0.0f;
  frag.game_time = g_game_time;
  frag.animate_x = animate.x;
  frag.animate_y = animate.y;
  frag.disp_x = 0.0f;
  frag.disp_y = 0.0f;
  frag.is_displacement = (displacement && displacement->get_handle()) ? 1 : 0;
  frag.blur = blur;
  SDL_PushGPUFragmentUniformData(cmd, 0, &frag, sizeof(frag));

  apply_scissor(pass);

  SDL_GPUViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.w = static_cast<float>(w);
  viewport.h = static_cast<float>(h);
  viewport.min_depth = 0.0f;
  viewport.max_depth = 1.0f;
  SDL_SetGPUViewport(pass, &viewport);

  const Uint32 vertex_count = static_cast<Uint32>(float_count / 8);
  SDL_DrawGPUPrimitives(pass, vertex_count, 1, 0, 0);
}

void
GPUPainter::draw_texture(const DrawingRequest& draw_req)
{
  auto&& request = std::get<TextureRequest>(draw_req.request);
  if (request.srcrects.size() != request.dstrects.size() ||
      request.srcrects.size() != request.angles.size() ||
      request.srcrects.empty())
    return;

  const auto* texture = static_cast<const GPUTexture*>(request.texture);
  const auto* displacement = static_cast<const GPUTexture*>(request.displacement_texture);
  if (!texture || !texture->get_handle())
    return;

  std::vector<float> verts;
  verts.reserve(request.srcrects.size() * 6 * 8);

  for (size_t i = 0; i < request.srcrects.size(); ++i)
  {
    const float left = request.dstrects[i].get_left();
    const float top = request.dstrects[i].get_top();
    const float right = request.dstrects[i].get_right();
    const float bottom = request.dstrects[i].get_bottom();

    float uv_left = request.srcrects[i].get_left() / static_cast<float>(texture->get_texture_width());
    float uv_top = request.srcrects[i].get_top() / static_cast<float>(texture->get_texture_height());
    float uv_right = request.srcrects[i].get_right() / static_cast<float>(texture->get_texture_width());
    float uv_bottom = request.srcrects[i].get_bottom() / static_cast<float>(texture->get_texture_height());

    if (draw_req.flip & HORIZONTAL_FLIP)
      std::swap(uv_left, uv_right);
    if (draw_req.flip & VERTICAL_FLIP)
      std::swap(uv_top, uv_bottom);

    if (request.angles[i] == 0.0f)
    {
      push_quad(verts, left, top, right, bottom,
                uv_left, uv_top, uv_right, uv_bottom,
                request.color, draw_req.alpha);
    }
    else
    {
      const float center_x = (left + right) / 2.0f;
      const float center_y = (top + bottom) / 2.0f;
      const float sa = sinf(math::radians(request.angles[i]));
      const float ca = cosf(math::radians(request.angles[i]));
      const float corners[4][2] = {
        {left - center_x, top - center_y},
        {right - center_x, top - center_y},
        {right - center_x, bottom - center_y},
        {left - center_x, bottom - center_y},
      };
      const float uvs[4][2] = {
        {uv_left, uv_top}, {uv_right, uv_top},
        {uv_right, uv_bottom}, {uv_left, uv_bottom},
      };
      const int idx[6] = {0, 1, 2, 0, 2, 3};
      for (int k = 0; k < 6; ++k)
      {
        const float rx = corners[idx[k]][0] * ca - corners[idx[k]][1] * sa + center_x;
        const float ry = corners[idx[k]][0] * sa + corners[idx[k]][1] * ca + center_y;
        const float v[8] = {rx, ry, uvs[idx[k]][0], uvs[idx[k]][1],
                            request.color.red, request.color.green,
                            request.color.blue, request.color.alpha * draw_req.alpha};
        verts.insert(verts.end(), v, v + 8);
      }
    }
  }

  draw_vertices(draw_req.blend, false, texture, displacement,
                texture->get_sampler().get_animate(), 0,
                verts.data(), verts.size());
}

void
GPUPainter::draw_gradient(const DrawingRequest& draw_req)
{
  auto&& request = std::get<GradientRequest>(draw_req.request);
  const Rectf& region = request.region;

  const float l = region.get_left();
  const float t = region.get_top();
  const float r = region.get_right();
  const float b = region.get_bottom();

  Color c0, c1, c2, c3;
  if (request.direction == VERTICAL || request.direction == VERTICAL_SECTOR)
  {
    c0 = c1 = request.top;
    c2 = c3 = request.bottom;
  }
  else
  {
    c0 = c3 = request.top;
    c1 = c2 = request.bottom;
  }
  const Color* corners[4] = {&c0, &c1, &c2, &c3};
  const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  const float pos[4][2] = {{l, t}, {r, t}, {r, b}, {l, b}};

  std::vector<float> verts;
  verts.reserve(6 * 8);
  const int idx[6] = {0, 1, 2, 0, 2, 3};
  for (int k = 0; k < 6; ++k)
  {
    const float v[8] = {pos[idx[k]][0], pos[idx[k]][1], uv[idx[k]][0], uv[idx[k]][1],
                        corners[idx[k]]->red, corners[idx[k]]->green,
                        corners[idx[k]]->blue, corners[idx[k]]->alpha * draw_req.alpha};
    verts.insert(verts.end(), v, v + 8);
  }

  draw_vertices(draw_req.blend, false, m_video_system.white_texture(), nullptr,
                Vector(0.0f, 0.0f), 0, verts.data(), verts.size());
}

void
GPUPainter::draw_filled_rect(const DrawingRequest& draw_req)
{
  auto&& request = std::get<FillRectRequest>(draw_req.request);
  // Phase 2: rounded corners (radius) and framebuffer blur need the
  // backbuffer target (Phase 3/4); plain rect for now.
  const Rectf& rect = request.rect;
  push_quad_wrapper(draw_req, rect, request.color);
}

void
GPUPainter::push_quad_wrapper(const DrawingRequest& draw_req, const Rectf& rect, const Color& color)
{
  std::vector<float> verts;
  verts.reserve(6 * 8);
  push_quad(verts, rect.get_left(), rect.get_top(), rect.get_right(), rect.get_bottom(),
            0.0f, 0.0f, 1.0f, 1.0f, color, draw_req.alpha);
  draw_vertices(draw_req.blend, false, m_video_system.white_texture(), nullptr,
                Vector(0.0f, 0.0f), 0, verts.data(), verts.size());
}

void
GPUPainter::draw_inverse_ellipse(const DrawingRequest& draw_req)
{
  // Phase 2: needs stencil/lightmap interplay (Phase 3). No-op.
  (void)draw_req;
}

void
GPUPainter::draw_line(const DrawingRequest& draw_req)
{
  auto&& request = std::get<LineRequest>(draw_req.request);
  const float v[16] = {
    request.pos.x, request.pos.y, 0.0f, 0.0f,
    request.color.red, request.color.green, request.color.blue, request.color.alpha * draw_req.alpha,
    request.dest_pos.x, request.dest_pos.y, 0.0f, 0.0f,
    request.color.red, request.color.green, request.color.blue, request.color.alpha * draw_req.alpha,
  };
  draw_vertices(draw_req.blend, true, m_video_system.white_texture(), nullptr,
                Vector(0.0f, 0.0f), 0, v, 16);
}

void
GPUPainter::draw_triangle(const DrawingRequest& draw_req)
{
  auto&& request = std::get<TriangleRequest>(draw_req.request);
  const float v[24] = {
    request.pos1.x, request.pos1.y, 0.0f, 0.0f,
    request.color.red, request.color.green, request.color.blue, request.color.alpha * draw_req.alpha,
    request.pos2.x, request.pos2.y, 0.0f, 0.0f,
    request.color.red, request.color.green, request.color.blue, request.color.alpha * draw_req.alpha,
    request.pos3.x, request.pos3.y, 0.0f, 0.0f,
    request.color.red, request.color.green, request.color.blue, request.color.alpha * draw_req.alpha,
  };
  draw_vertices(draw_req.blend, false, m_video_system.white_texture(), nullptr,
                Vector(0.0f, 0.0f), 0, v, 24);
}
