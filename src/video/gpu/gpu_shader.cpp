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

#include "video/gpu/gpu_shader.hpp"

#include <physfs.h>

#include "util/log.hpp"

std::string
GPUShader::load_spv_bytes(const std::string& physfs_path)
{
  PHYSFS_File* file = PHYSFS_openRead(physfs_path.c_str());
  if (!file)
  {
    log_warning << "Couldn't open shader '" << physfs_path << "'" << std::endl;
    return {};
  }
  std::string bytes;
  char buf[4096];
  PHYSFS_sint64 n = 0;
  while ((n = PHYSFS_readBytes(file, buf, sizeof(buf))) > 0)
    bytes.append(buf, static_cast<size_t>(n));
  PHYSFS_close(file);
  return bytes;
}

GPUShader::GPUShader(SDL_GPUDevice* device, const std::string& physfs_path,
                     SDL_GPUShaderStage stage,
                     Uint32 num_samplers, Uint32 num_uniform_buffers) :
  m_device(device),
  m_shader(nullptr)
{
  if (!(SDL_GetGPUShaderFormats(m_device) & SDL_GPU_SHADERFORMAT_SPIRV))
    throw std::runtime_error("GPU device lacks SPIR-V support");

  std::string bytes = load_spv_bytes(physfs_path);
  if (bytes.empty() || bytes.size() % 4 != 0)
    throw std::runtime_error("Bad SPIR-V file: " + physfs_path);

  SDL_GPUShaderCreateInfo info{};
  info.code_size = bytes.size();
  info.code = reinterpret_cast<const Uint8*>(bytes.data());
  info.entrypoint = "main";
  info.format = SDL_GPU_SHADERFORMAT_SPIRV;
  info.stage = stage;
  info.num_samplers = num_samplers;
  info.num_storage_textures = 0;
  info.num_storage_buffers = 0;
  info.num_uniform_buffers = num_uniform_buffers;
  m_shader = SDL_CreateGPUShader(m_device, &info);
  if (!m_shader)
    throw std::runtime_error("SDL_CreateGPUShader failed for " + physfs_path);
}

GPUShader::~GPUShader()
{
  if (m_shader)
    SDL_ReleaseGPUShader(m_device, m_shader);
}
