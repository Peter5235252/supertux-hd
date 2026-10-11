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

#include <SDL3/SDL.h>

#include "video/sampler.hpp"

GPUTexture::GPUTexture(int width, int height) :
  m_texture_width(width),
  m_texture_height(height),
  m_image_width(width),
  m_image_height(height)
{
}

GPUTexture::GPUTexture(const SDL_Surface& image, const Sampler& sampler) :
  Texture(sampler),
  m_texture_width(image.w),
  m_texture_height(image.h),
  m_image_width(image.w),
  m_image_height(image.h)
{
}

void
GPUTexture::reload(const SDL_Surface& image)
{
  m_texture_width = image.w;
  m_texture_height = image.h;
  m_image_width = image.w;
  m_image_height = image.h;
}
