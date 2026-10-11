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

#include <string>

/** Loads precompiled SPIR-V (checked into data/shader/) and creates an
    SDL_GPUShader. Recompiling: glslc data/shader/gpu_*.vert -o ...spv */
class GPUShader final
{
public:
  GPUShader(SDL_GPUDevice* device, const std::string& physfs_path,
            SDL_GPUShaderStage stage,
            Uint32 num_samplers, Uint32 num_uniform_buffers);
  ~GPUShader();

  SDL_GPUShader* get() const { return m_shader; }

  static std::string load_spv_bytes(const std::string& physfs_path);

private:
  SDL_GPUDevice* m_device;
  SDL_GPUShader* m_shader;

private:
  GPUShader(const GPUShader&) = delete;
  GPUShader& operator=(const GPUShader&) = delete;
};
