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

#include "video/texture.hpp"

/** Phase 0: Texture stub. Records dimensions so surfaces/layout work;
    no GPU upload yet (Phase 1 transfers via SDL_GPUTransferBuffer). */
class GPUTexture final : public Texture
{
public:
  GPUTexture(int width, int height);
  GPUTexture(const SDL_Surface& image, const Sampler& sampler);
  ~GPUTexture() override {}

  virtual void reload(const SDL_Surface& image) override;

  virtual int get_texture_width() const override { return m_texture_width; }
  virtual int get_texture_height() const override { return m_texture_height; }

  virtual int get_image_width() const override { return m_image_width; }
  virtual int get_image_height() const override { return m_image_height; }

private:
  int m_texture_width;
  int m_texture_height;
  int m_image_width;
  int m_image_height;

private:
  GPUTexture(const GPUTexture&) = delete;
  GPUTexture& operator=(const GPUTexture&) = delete;
};
