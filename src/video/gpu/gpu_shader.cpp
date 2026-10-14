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
GPUShader::load_shader_bytes(const std::string& physfs_path)
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

GPUShader::GPUShader(SDL_GPUDevice* device, const std::string& physfs_base_path,
                     SDL_GPUShaderStage stage,
                     Uint32 num_samplers, Uint32 num_uniform_buffers) :
  m_device(device),
  m_shader(nullptr)
{
  // Prefer the device's native format; the path carries no extension.
  struct FormatPick { SDL_GPUShaderFormat format; const char* ext; const char* entry; };
  static const FormatPick kPicks[] = {
    { SDL_GPU_SHADERFORMAT_MSL, ".msl", "main0" },
    { SDL_GPU_SHADERFORMAT_DXIL, ".dxil", "main" },
    { SDL_GPU_SHADERFORMAT_SPIRV, ".spv", "main" },
  };
  const SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(m_device);
  const FormatPick* pick = nullptr;
  for (const auto& candidate : kPicks)
  {
    if (supported & candidate.format)
    {
      pick = &candidate;
      break;
    }
  }
  if (!pick)
    throw std::runtime_error("GPU device supports no known shader format");

  const std::string path = physfs_base_path + pick->ext;
  std::string bytes = load_shader_bytes(path);
  if (bytes.empty())
    throw std::runtime_error("Missing shader file: " + path);
  if (pick->format == SDL_GPU_SHADERFORMAT_SPIRV && bytes.size() % 4 != 0)
    throw std::runtime_error("Bad SPIR-V file: " + path);

  SDL_GPUShaderCreateInfo info{};
  info.code_size = bytes.size();
  info.code = reinterpret_cast<const Uint8*>(bytes.data());
  info.entrypoint = pick->entry;
  info.format = pick->format;
  info.stage = stage;
  info.num_samplers = num_samplers;
  info.num_storage_textures = 0;
  info.num_storage_buffers = 0;
  info.num_uniform_buffers = num_uniform_buffers;
  m_shader = SDL_CreateGPUShader(m_device, &info);
  if (!m_shader)
    throw std::runtime_error("SDL_CreateGPUShader failed for " + path);
}

GPUShader::~GPUShader()
{
  if (m_shader)
    SDL_ReleaseGPUShader(m_device, m_shader);
}
