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

#include "video/texture.hpp"

/** Phase 1: real GPU texture. Pixel data is uploaded through an
    SDL_GPUTransferBuffer and a full mip chain is generated, so minified
    4K assets filter smoothly. Sampling state (filter/wrap) lives in the
    base Sampler; the SDL_GPUSampler object is created by the painter in
    Phase 2. */
class GPUTexture final : public Texture
{
public:
  GPUTexture(SDL_GPUDevice* device, int width, int height);
  GPUTexture(SDL_GPUDevice* device, const SDL_Surface& image, const Sampler& sampler);
  ~GPUTexture() override;

  virtual void reload(const SDL_Surface& image) override;

  virtual int get_texture_width() const override { return m_texture_width; }
  virtual int get_texture_height() const override { return m_texture_height; }

  virtual int get_image_width() const override { return m_image_width; }
  virtual int get_image_height() const override { return m_image_height; }

  SDL_GPUTexture* get_handle() const { return m_handle; }

private:
  void upload(const SDL_Surface& image);

private:
  SDL_GPUDevice* m_device;
  SDL_GPUTexture* m_handle;
  int m_texture_width;
  int m_texture_height;
  int m_image_width;
  int m_image_height;

private:
  GPUTexture(const GPUTexture&) = delete;
  GPUTexture& operator=(const GPUTexture&) = delete;
};
