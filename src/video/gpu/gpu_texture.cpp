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

#include "video/gpu/gpu_texture.hpp"

#include <cmath>

#include <SDL3/SDL.h>

#include "util/log.hpp"
#include "video/sampler.hpp"

namespace {

int
mip_level_count(int w, int h)
{
  return static_cast<int>(std::floor(std::log2(static_cast<float>(std::max(w, h))))) + 1;
}

} // namespace

GPUTexture::GPUTexture(SDL_GPUDevice* device, int width, int height) :
  m_device(device),
  m_handle(nullptr),
  m_texture_width(width),
  m_texture_height(height),
  m_image_width(width),
  m_image_height(height)
{
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  info.usage = static_cast<SDL_GPUTextureUsageFlags>(
    SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
  info.width = static_cast<Uint32>(width);
  info.height = static_cast<Uint32>(height);
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  m_handle = SDL_CreateGPUTexture(m_device, &info);
  if (!m_handle)
  {
    log_warning << "SDL_CreateGPUTexture failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
  }
}

GPUTexture::GPUTexture(SDL_GPUDevice* device, const SDL_Surface& image, const Sampler& sampler) :
  Texture(sampler),
  m_device(device),
  m_handle(nullptr),
  m_texture_width(image.w),
  m_texture_height(image.h),
  m_image_width(image.w),
  m_image_height(image.h)
{
  upload(image);
}

GPUTexture::~GPUTexture()
{
  if (m_handle)
    SDL_ReleaseGPUTexture(m_device, m_handle);
}

void
GPUTexture::reload(const SDL_Surface& image)
{
  m_texture_width = image.w;
  m_texture_height = image.h;
  m_image_width = image.w;
  m_image_height = image.h;
  upload(image);
}

void
GPUTexture::upload(const SDL_Surface& image)
{
  if (m_handle)
  {
    SDL_ReleaseGPUTexture(m_device, m_handle);
    m_handle = nullptr;
  }

  // Normalize to RGBA8888 so rows are tightly packed 4-byte pixels.
  SDL_Surface* converted = nullptr;
  const SDL_Surface* src = &image;
  if (image.format != SDL_PIXELFORMAT_RGBA8888)
  {
    converted = SDL_ConvertSurface(const_cast<SDL_Surface*>(&image), SDL_PIXELFORMAT_RGBA8888);
    if (!converted)
    {
      log_warning << "SDL_ConvertSurface failed: " << SDL_GetError() << std::endl;
      SDL_ClearError();
      return;
    }
    src = converted;
  }

  const int w = src->w;
  const int h = src->h;

  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  // COLOR_TARGET is required for SDL_GenerateMipmapsForGPUTexture.
  info.usage = static_cast<SDL_GPUTextureUsageFlags>(
    SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
  info.width = static_cast<Uint32>(w);
  info.height = static_cast<Uint32>(h);
  info.layer_count_or_depth = 1;
  info.num_levels = static_cast<Uint32>(mip_level_count(w, h));
  info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  m_handle = SDL_CreateGPUTexture(m_device, &info);
  if (!m_handle)
  {
    log_warning << "SDL_CreateGPUTexture failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
    if (converted)
      SDL_DestroySurface(converted);
    return;
  }

  SDL_GPUTransferBufferCreateInfo tb_info{};
  tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  tb_info.size = static_cast<Uint32>(w * h * 4);
  SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(m_device, &tb_info);
  if (!transfer)
  {
    log_warning << "SDL_CreateGPUTransferBuffer failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
    if (converted)
      SDL_DestroySurface(converted);
    return;
  }

  void* mapped = SDL_MapGPUTransferBuffer(m_device, transfer, false);
  if (!mapped)
  {
    log_warning << "SDL_MapGPUTransferBuffer failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
    SDL_ReleaseGPUTransferBuffer(m_device, transfer);
    if (converted)
      SDL_DestroySurface(converted);
    return;
  }

  const Uint8* src_pixels = static_cast<const Uint8*>(src->pixels);
  const int src_pitch = src->pitch;
  const int dst_pitch = w * 4;
  if (src_pitch == dst_pitch)
  {
    memcpy(mapped, src_pixels, static_cast<size_t>(dst_pitch * h));
  }
  else
  {
    Uint8* dst_pixels = static_cast<Uint8*>(mapped);
    for (int y = 0; y < h; ++y)
      memcpy(dst_pixels + y * dst_pitch, src_pixels + y * src_pitch, dst_pitch);
  }
  SDL_UnmapGPUTransferBuffer(m_device, transfer);
  if (converted)
    SDL_DestroySurface(converted);

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device);
  if (!cmd)
  {
    log_warning << "SDL_AcquireGPUCommandBuffer failed: " << SDL_GetError() << std::endl;
    SDL_ClearError();
    SDL_ReleaseGPUTransferBuffer(m_device, transfer);
    return;
  }

  SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cmd);
  SDL_GPUTextureTransferInfo src_info{};
  src_info.transfer_buffer = transfer;
  src_info.offset = 0;
  src_info.pixels_per_row = static_cast<Uint32>(w);
  src_info.rows_per_layer = static_cast<Uint32>(h);
  SDL_GPUTextureRegion dst_region{};
  dst_region.texture = m_handle;
  dst_region.mip_level = 0;
  dst_region.layer = 0;
  dst_region.x = 0;
  dst_region.y = 0;
  dst_region.z = 0;
  dst_region.w = static_cast<Uint32>(w);
  dst_region.h = static_cast<Uint32>(h);
  dst_region.d = 1;
  SDL_UploadToGPUTexture(pass, &src_info, &dst_region, false);
  SDL_EndGPUCopyPass(pass);

  SDL_GenerateMipmapsForGPUTexture(cmd, m_handle);

  SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
  if (fence)
  {
    SDL_WaitForGPUFences(m_device, true, &fence, 1);
    SDL_ReleaseGPUFence(m_device, fence);
  }
  SDL_ReleaseGPUTransferBuffer(m_device, transfer);

  log_debug << "GPUTexture uploaded " << w << "x" << h
            << " (" << info.num_levels << " mip levels)" << std::endl;
}
