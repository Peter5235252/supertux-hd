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
#include <utility>

#include "video/blend.hpp"
#include "video/gpu/gpu_shader.hpp"

/** Caches SDL_GPUGraphicsPipelines keyed by (Blend, line-strip flag).

    All pipelines share the textured-quad shaders and the interleaved
    vertex layout {pos.xy, uv.xy, color.rgba}; only blend state and
    primitive topology vary. Mirrors the glBlendFunc mapping from the
    GL backend: NONE=(1,0) BLEND=(SA,1-SA) ADD=(SA,1) MOD=(DST,0). */
class GPUPipeline final
{
public:
  GPUPipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat target_format);
  ~GPUPipeline();

  SDL_GPUGraphicsPipeline* for_blend(Blend blend, bool lines = false);

  /** Post-process pipelines (fullscreen triangle, no vertex input):
      easu=true selects the FSR EASU upscale shader, else RCAS sharpen. */
  SDL_GPUGraphicsPipeline* for_post(bool easu);

  static SDL_GPUVertexInputState vertex_input_state();
  static SDL_GPUVertexInputState empty_vertex_input_state();

private:
  SDL_GPUGraphicsPipeline* build(Blend blend, bool lines);
  SDL_GPUGraphicsPipeline* build_post(bool easu);

private:
  SDL_GPUDevice* m_device;
  SDL_GPUTextureFormat m_target_format;
  GPUShader m_vert;
  GPUShader m_frag;
  GPUShader m_fullscreen_vert;
  GPUShader m_easu_frag;
  GPUShader m_rcas_frag;
  std::map<std::pair<Blend, bool>, SDL_GPUGraphicsPipeline*> m_cache;
  std::map<bool, SDL_GPUGraphicsPipeline*> m_post_cache;

private:
  GPUPipeline(const GPUPipeline&) = delete;
  GPUPipeline& operator=(const GPUPipeline&) = delete;
};
