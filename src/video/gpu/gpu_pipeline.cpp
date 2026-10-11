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

#include "video/gpu/gpu_pipeline.hpp"

#include <stdexcept>

namespace {

SDL_GPUColorTargetBlendState
blend_state(Blend blend)
{
  SDL_GPUColorTargetBlendState state{};
  state.color_write_mask = 0xF;
  switch (blend)
  {
    case Blend::NONE:
      state.enable_blend = false;
      break;
    case Blend::BLEND:
      state.enable_blend = true;
      state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
      state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
      state.color_blend_op = SDL_GPU_BLENDOP_ADD;
      state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
      state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
      state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
      break;
    case Blend::ADD:
      state.enable_blend = true;
      state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
      state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
      state.color_blend_op = SDL_GPU_BLENDOP_ADD;
      state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
      state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
      state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
      break;
    case Blend::MOD:
      state.enable_blend = true;
      state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
      state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
      state.color_blend_op = SDL_GPU_BLENDOP_ADD;
      state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_DST_ALPHA;
      state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
      state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
      break;
  }
  return state;
}

} // namespace

GPUPipeline::GPUPipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat target_format) :
  m_device(device),
  m_target_format(target_format),
  m_vert(device, "shader/gpu_textured.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0),
  m_frag(device, "shader/gpu_textured.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 3, 1),
  m_cache()
{
}

GPUPipeline::~GPUPipeline()
{
  for (const auto& entry : m_cache)
    SDL_ReleaseGPUGraphicsPipeline(m_device, entry.second);
}

SDL_GPUVertexInputState
GPUPipeline::vertex_input_state()
{
  static SDL_GPUVertexBufferDescription buffer_desc{};
  buffer_desc.slot = 0;
  buffer_desc.pitch = 8 * sizeof(float);
  buffer_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  buffer_desc.instance_step_rate = 0;

  // Must outlive use: static storage is fine (constant layout).
  static SDL_GPUVertexAttribute attrs[3];
  attrs[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0};
  attrs[1] = {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 2 * sizeof(float)};
  attrs[2] = {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 4 * sizeof(float)};

  SDL_GPUVertexInputState state{};
  state.num_vertex_buffers = 1;
  state.vertex_buffer_descriptions = &buffer_desc;
  state.num_vertex_attributes = 3;
  state.vertex_attributes = attrs;
  return state;
}

SDL_GPUGraphicsPipeline*
GPUPipeline::for_blend(Blend blend, bool lines)
{
  auto key = std::make_pair(blend, lines);
  auto it = m_cache.find(key);
  if (it != m_cache.end())
    return it->second;
  SDL_GPUGraphicsPipeline* pipeline = build(blend, lines);
  m_cache[key] = pipeline;
  return pipeline;
}

SDL_GPUGraphicsPipeline*
GPUPipeline::build(Blend blend, bool lines)
{
  SDL_GPUColorTargetDescription target{};
  target.format = m_target_format;
  target.blend_state = blend_state(blend);

  SDL_GPUGraphicsPipelineCreateInfo info{};
  info.vertex_shader = m_vert.get();
  info.fragment_shader = m_frag.get();
  info.vertex_input_state = vertex_input_state();
  info.primitive_type = lines ?
    SDL_GPU_PRIMITIVETYPE_LINESTRIP : SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
  info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
  info.depth_stencil_state.enable_depth_test = false;
  info.depth_stencil_state.enable_depth_write = false;
  info.target_info.num_color_targets = 1;
  info.target_info.color_target_descriptions = &target;

  SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &info);
  if (!pipeline)
    throw std::runtime_error("SDL_CreateGPUGraphicsPipeline failed");
  return pipeline;
}
