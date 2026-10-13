//  SuperTux
//  Copyright (C) 2006 Matthias Braun <matze@braunis.de>
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

#include "video/texture_manager.hpp"

#include <SDL3_image/SDL_image.h>
#include <assert.h>
#include <sstream>

#include <physfs.h>

#include "math/rect.hpp"
#include "physfs/physfs_sdl.hpp"
#include "supertux/gameconfig.hpp"
#include "supertux/globals.hpp"
#include "util/file_system.hpp"
#include "util/log.hpp"
#include "util/reader_document.hpp"
#include "util/reader_mapping.hpp"
#include "video/color.hpp"
#include "video/gl.hpp"
#include "video/sampler.hpp"
#include "video/sdl_surface.hpp"
#include "video/texture.hpp"
#include "video/video_system.hpp"

namespace {

GLenum string2wrap(const std::string& text)
{
  if (text == "clamp-to-edge")
  {
    return GL_CLAMP_TO_EDGE;
  }
  else if (text == "repeat")
  {
    return GL_REPEAT;
  }
  else if (text == "mirrored-repeat")
  {
    return GL_MIRRORED_REPEAT;
  }
  else
  {
    log_warning << "unknown texture wrap: " << text << std::endl;
    return GL_CLAMP_TO_EDGE;
  }
}

GLenum string2filter(const std::string& text)
{
  if (text == "nearest")
  {
    return GL_NEAREST;
  }
  else if (text == "linear")
  {
    return GL_LINEAR;
  }
  else
  {
    log_warning << "unknown texture filter: " << text << std::endl;
    return GL_LINEAR;
  }
}

SDLSurfacePtr create_image_surface(const std::string& filename)
{
  // HD fork: prefer AI-upscaled variant mounted at "hd/" when enabled.
  // World-space size is preserved via Surface::m_hd_scale (see get_hd_scale).
  // Gate on the manifest scale (not just file existence) so partial batches
  // that haven't been logged yet safely fall back to originals.
  if (g_config && g_config->hd_textures)
  {
    int hd_scale = 1;
    try {
      auto* mgr = TextureManager::current();
      if (mgr) hd_scale = mgr->get_hd_scale(filename);
    } catch (...) {}
    if (hd_scale > 1)
    {
      const std::string hd_path = std::string("hd/") + filename;
      if (PHYSFS_exists(hd_path.c_str()))
      {
        try {
          auto surface = SDLSurface::from_file(hd_path);
          log_debug << "HD texture loaded: " << hd_path << " x" << hd_scale
                    << " (" << surface->w << "x" << surface->h << ")" << std::endl;
          return surface;
        } catch (const std::exception& err) {
          log_warning << "HD texture '" << hd_path << "' failed, falling back to '" << filename << "': " << err.what() << std::endl;
        }
      }
    }
  }

  if (PHYSFS_exists(filename.c_str()))
    return SDLSurface::from_file(filename);

  // The image doesn't exist, so attempt to load a ".deprecated" version
  log_warning << "Image '" << filename << "' doesn't exist. Attempting to load \".deprecated\" version." << std::endl;
  return SDLSurface::from_file(FileSystem::strip_extension(filename) + ".deprecated" +
                               FileSystem::extension(filename));
}

} // namespace

const std::string TextureManager::s_dummy_texture = "images/engine/missing.png";

TextureManager::TextureManager() :
  m_image_textures(),
  m_surfaces(),
  m_load_successful(false)
{
}

TextureManager::~TextureManager()
{
  for (const auto& texture : m_image_textures)
  {
    if (!texture.second.expired())
    {
      log_warning << "Texture '" << std::get<0>(texture.first) << "' not freed" << std::endl;
    }
  }
  m_image_textures.clear();
  m_surfaces.clear();
}

TexturePtr
TextureManager::get(const ReaderMapping& mapping, const std::optional<Rect>& region)
{
  std::string filename;
  if (!mapping.get("file", filename))
  {
    log_warning << "'file' tag missing" << std::endl;
  }
  else
  {
    filename = FileSystem::join(mapping.get_doc().get_directory(), filename);
  }

  std::optional<Rect> rect;
  std::vector<int> rect_v;
  if (mapping.get("rect", rect_v))
  {
    if (rect_v.size() == 4)
    {
      rect = Rect(rect_v[0], rect_v[1], rect_v[2], rect_v[3]);
    }
    else
    {
      log_warning << "'rect' requires four elements" << std::endl;
    }
  }

  GLenum wrap_s = GL_CLAMP_TO_EDGE;
  GLenum wrap_t = GL_CLAMP_TO_EDGE;

  std::vector<std::string> wrap_v;
  if (mapping.get("wrap", wrap_v))
  {
    if (wrap_v.size() == 1)
    {
      wrap_s = string2wrap(wrap_v[0]);
      wrap_t = string2wrap(wrap_v[0]);
    }
    else if (wrap_v.size() == 2)
    {
      wrap_s = string2wrap(wrap_v[0]);
      wrap_t = string2wrap(wrap_v[1]);
    }
    else
    {
      log_warning << "unknown number of wrap arguments" << std::endl;
    }
  }

  GLenum filter = GL_LINEAR;
  std::string filter_s;
  if (mapping.get("filter", filter_s))
  {
    filter = string2filter(filter_s);
  }

  Vector animate(0.0f, 0.0f);
  std::vector<float> animate_v;
  if (mapping.get("animate", animate_v))
  {
    if (animate_v.size() == 2)
    {
      animate.x = animate_v[0];
      animate.y = animate_v[1];
    }
  }

  if (region)
  {
    if (!rect)
    {
      rect = region;
    }
    else
    {
      rect->left += region->left;
      rect->top += region->top;

      rect->right = rect->left + region->get_width();
      rect->bottom = rect->top + region->get_height();
    }
  }

  return get(filename, rect, Sampler(filter, wrap_s, wrap_t, animate));
}

TexturePtr
TextureManager::get(const std::string& _filename)
{
  std::string filename = FileSystem::normalize(_filename);
  Texture::Key key(filename, Rect(0, 0, 0, 0));
  auto i = m_image_textures.find(key);

  TexturePtr texture;
  if (i != m_image_textures.end())
    texture = i->second.lock();

  if (!texture) {
    texture = create_image_texture(filename, Sampler());
    texture->m_cache_key = key;
    m_image_textures[key] = texture;
  }

  return texture;
}

TexturePtr
TextureManager::get(const std::string& _filename,
                    const std::optional<Rect>& rect,
                    const Sampler& sampler)
{
  std::string filename = FileSystem::normalize(_filename);
  Texture::Key key = Texture::Key(filename, rect ? *rect : Rect());

  auto i = m_image_textures.find(key);

  TexturePtr texture;
  if (i != m_image_textures.end())
    texture = i->second.lock();

  if (!texture)
  {
    if (rect)
    {
      texture = create_image_texture(filename, *rect, sampler);
    }
    else
    {
      texture = create_image_texture(filename, sampler);
    }
    texture->m_cache_key = key;
    m_image_textures[key] = texture;
  }

  return texture;
}

void
TextureManager::reap_cache_entry(const Texture::Key& key)
{
  auto i = m_image_textures.find(key);
  if (i == m_image_textures.end())
  {
    log_warning << "no cache entry for '" << std::get<0>(key) << "'" << std::endl;
  }
  else
  {
    assert(i->second.expired());
    m_image_textures.erase(i);
  }
}

TexturePtr
TextureManager::create_image_texture(const std::string& filename, const Rect& rect, const Sampler& sampler)
{
  m_load_successful = true;
  try
  {
    SDLSurfacePtr surface = create_image_surface_raw(filename, rect, sampler);
    return VideoSystem::current()->new_texture(*surface, sampler);
  }
  catch(const std::exception& err)
  {
    log_warning << "Couldn't load texture '" << filename << "' (now using dummy texture): " << err.what() << std::endl;
    m_load_successful = false;
    return create_dummy_texture();
  }
}

const SDL_Surface&
TextureManager::get_surface(const std::string& filename)
{
  auto i = m_surfaces.find(filename);
  if (i != m_surfaces.end())
  {
    return *i->second;
  }

  SDLSurfacePtr surface = create_image_surface(filename);
  const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(surface.get()->format);
  if (format->Rmask == 0 &&
      format->Gmask == 0 &&
      format->Bmask == 0 &&
      format->Amask == 0)
  {
    SDL_Surface* surf;
    log_debug << "Wrong surface format for image " << filename << ". Compensating." << std::endl;
    surf = SDL_ConvertSurface(const_cast<SDL_Surface*>(surface.get()), SDL_PIXELFORMAT_RGBA8888);
    surface.reset(surf);
  }
  return *(m_surfaces[filename] = std::move(surface));
}

SDLSurfacePtr
TextureManager::create_image_surface_raw(const std::string& filename, const Rect& rect, const Sampler& sampler)
{
  assert(rect.valid());

  // HD fork: rects in data files are in logical pixels; scale up for HD bitmaps.
  Rect hd_rect = rect;
  const int hd_scale = get_hd_scale(filename);
  if (hd_scale > 1)
    hd_rect = Rect(rect.left * hd_scale, rect.top * hd_scale,
                   rect.right * hd_scale, rect.bottom * hd_scale);

  const SDL_Surface& src_surface = get_surface(filename);
  const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(src_surface.format);

  const SDL_Surface& surface = src_surface;

  SDLSurfacePtr subimage;
  if (!Rect(0, 0, surface.w, surface.h).contains(hd_rect))
  {
    log_warning << filename << ": invalid subregion requested: image="
                << surface.w << "x" << surface.h << ", rect=" << hd_rect << std::endl;

    subimage = SDLSurfacePtr(
      SDL_CreateSurface(hd_rect.get_width(), hd_rect.get_height(), src_surface.format)
    );

    Rect clipped_rect(std::max(0, hd_rect.left),
                      std::max(0, hd_rect.top),
                      std::min(subimage->w, hd_rect.right),
                      std::min(subimage->w, hd_rect.bottom));

    SDL_Rect srcrect = clipped_rect.to_sdl();
    SDL_BlitSurface(const_cast<SDL_Surface*>(&surface), &srcrect, subimage.get(), nullptr);
  }
  else
  {
    subimage = SDLSurfacePtr(SDL_CreateSurfaceFrom(
                              hd_rect.get_width(), hd_rect.get_height(),
                                src_surface.format,
                                static_cast<uint8_t*>(surface.pixels) +
                                  hd_rect.top * surface.pitch +
                                  hd_rect.left * format->bytes_per_pixel, surface.pitch));

    if (!subimage)
    {
      throw std::runtime_error("SDL_CreateSurfaceFrom() call failed");
    }
  }

  return subimage;
}

TexturePtr
TextureManager::create_image_texture(const std::string& filename, const Sampler& sampler)
{
  m_load_successful = true;
  try
  {
    SDLSurfacePtr surface = create_image_surface(filename);
    return VideoSystem::current()->new_texture(*surface, sampler);
  }
  catch (const std::exception& err)
  {
    log_warning << "Couldn't load texture '" << filename << "' (now using dummy texture): " << err.what() << std::endl;
    m_load_successful = false;
    return create_dummy_texture();
  }
}

SDLSurfacePtr
TextureManager::create_dummy_surface()
{
  // on error, try loading placeholder file
  try
  {
    SDLSurfacePtr surface = create_image_surface(s_dummy_texture);
    return surface;
  }
  catch (const std::exception& err)
  {
    // on error (when loading placeholder), try using empty surface,
    // when that fails to, just give up
    SDLSurfacePtr surface(SDL_CreateSurface(128, 128, SDL_GetPixelFormatForMasks(8, 0, 0, 0, 0)));
    if (!surface)
    {
      throw;
    }
    else
    {
      log_warning << "Couldn't load texture '" << s_dummy_texture << "' (now using empty one): " << err.what() << std::endl;
      return surface;
    }
  }
}

TexturePtr
TextureManager::create_dummy_texture() const
{
  SDLSurfacePtr surface = create_dummy_surface();
  return VideoSystem::current()->new_texture(*surface);
}

void
TextureManager::reload()
{
  // Reload surfaces
  for (auto& surface : m_surfaces)
  {
    SDLSurfacePtr surface_new;
    try
    {
      surface_new = create_image_surface(surface.first);
    }
    catch (const std::exception& err)
    {
      log_warning << "Couldn't load texture '" << surface.first << "' (now using dummy texture): " << err.what() << std::endl;
      surface_new = create_dummy_surface();
    }
    surface.second.reset(surface_new);
  }

  // Reload textures
  for (auto& texture : m_image_textures)
  {
    auto texture_ptr = texture.second.lock();

    SDLSurfacePtr surface;
    if (std::get<1>(texture.first).empty()) // No specific rect for texture
    {
      try
      {
        surface = create_image_surface(std::get<0>(texture.first));
      }
      catch (const std::exception& err)
      {
        log_warning << "Couldn't load texture '" << std::get<0>(texture.first) << "' (now using dummy texture): " << err.what() << std::endl;
        surface = create_dummy_surface();
      }
    }
    else // Texture has a specific rect
    {
      try
      {
        surface = create_image_surface_raw(std::get<0>(texture.first), std::get<1>(texture.first), texture_ptr->get_sampler());
      }
      catch (const std::exception& err)
      {
        log_warning << "Couldn't load texture '" << std::get<0>(texture.first) << "' (now using dummy texture): " << err.what() << std::endl;
        surface = create_dummy_surface();
      }
    }

    texture_ptr->reload(*surface);
  }
}

int
TextureManager::get_hd_scale(const std::string& filename)
{
  ensure_hd_manifest_loaded();
  auto it = m_hd_scales.find(FileSystem::normalize(filename));
  return (it != m_hd_scales.end()) ? it->second : 1;
}

void
TextureManager::ensure_hd_manifest_loaded()
{
  if (m_hd_manifest_loaded)
    return;
  m_hd_manifest_loaded = true;
  // Manifest written by tools/hd_upscale.py: lines "<rel> <model> x<scale>",
  // where rel is relative to data/images. Mounted as "hd/images/UPSCALE_LOG.txt".
  static const char* kCandidates[] = {
    "hd/images/UPSCALE_LOG.txt",
    "images/UPSCALE_LOG.txt",
  };
  for (const char* path : kCandidates)
  {
    if (!PHYSFS_exists(path))
      continue;
    try {
      SDLSurfacePtr dummy;
      // Read as raw bytes via PHYSFS.
      PHYSFS_File* f = PHYSFS_openRead(path);
      if (!f)
        continue;
      std::string data;
      char buf[4096];
      PHYSFS_sint64 n;
      while ((n = PHYSFS_readBytes(f, buf, sizeof(buf))) > 0)
        data.append(buf, static_cast<size_t>(n));
      PHYSFS_close(f);
      size_t pos = 0;
      while (pos < data.size())
      {
        size_t eol = data.find('\n', pos);
        std::string line = data.substr(pos, eol == std::string::npos ? eol : eol - pos);
        pos = (eol == std::string::npos) ? data.size() : eol + 1;
        // format: "background/antarctic/foo.png realesrgan-x4plus x2"
        size_t sp = line.find_last_of(" \t");
        if (sp == std::string::npos || sp + 2 >= line.size() || line[sp + 1] != 'x')
          continue;
        int scale = atoi(line.c_str() + sp + 2);
        if (scale < 1 || scale > 8)
          continue;
        size_t first_sp = line.find_first_of(" \t");
        std::string rel = line.substr(0, first_sp);
        std::string key = FileSystem::normalize(std::string("images/") + rel);
        m_hd_scales[key] = scale;
      }
      log_info << "HD manifest '" << path << "': " << m_hd_scales.size() << " scaled textures" << std::endl;
      return;
    } catch (const std::exception& err) {
      log_warning << "Failed to parse HD manifest '" << path << "': " << err.what() << std::endl;
    }
  }
}

void
TextureManager::debug_print(std::ostream& out) const
{  size_t total_texture_pixels = 0;
  out << "textures:begin" << std::endl;
  for(const auto& it : m_image_textures)
  {
    const auto& key = it.first;

    if (it.second.lock()) {
      total_texture_pixels += std::get<1>(key).get_area();
    }

    out << "  texture "
        << " filename:" << std::get<0>(key) << " " << std::get<1>(key)
        << " " << "use_count:" << it.second.use_count() << std::endl;
  }
  out << "textures:end" << std::endl;

  size_t total_surface_pixels = 0;
  out << "surfaces:begin" << std::endl;
  for(const auto& it : m_surfaces)
  {
    const auto& filename = it.first;
    const auto& surface = it.second;

    total_surface_pixels += surface->w * surface->h;
    out << "  surface filename:" << filename << " " << surface->w << "x" << surface->h << std::endl;
  }
  out << "surfaces:end" << std::endl;

  out << "total texture count:" << m_image_textures.size() << std::endl;
  out << "total texture pixels:" << total_texture_pixels << std::endl;

  out << "total surface count:" << m_surfaces.size() << std::endl;
  out << "total surface pixels:" << total_surface_pixels << std::endl;
}
