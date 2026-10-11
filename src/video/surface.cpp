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

#include "video/surface.hpp"

#include <sstream>

#include "util/reader_document.hpp"
#include "util/reader_mapping.hpp"
#include "util/file_system.hpp"
#include "util/log.hpp"
#include "util/string_util.hpp"
#include "video/texture.hpp"
#include "video/texture_manager.hpp"
#include "video/video_system.hpp"

SurfacePtr
Surface::from_reader(const ReaderMapping& mapping, const std::optional<Rect>& rect, const std::string& filename)
{
  TexturePtr diffuse_texture;
  std::optional<ReaderMapping> diffuse_texture_mapping;
  if (mapping.get("diffuse-texture", diffuse_texture_mapping))
  {
    diffuse_texture = TextureManager::current()->get(*diffuse_texture_mapping, rect);
  }

  TexturePtr displacement_texture;
  std::optional<ReaderMapping> displacement_texture_mapping;
  if (mapping.get("displacement-texture", displacement_texture_mapping))
  {
    displacement_texture = TextureManager::current()->get(*displacement_texture_mapping, rect);
  }

  Flip flip = NO_FLIP;
  std::vector<bool> flip_v;
  if (mapping.get("flip", flip_v))
  {
    flip ^= flip_v[0] ? HORIZONTAL_FLIP : NO_FLIP;
    flip ^= flip_v[1] ? VERTICAL_FLIP : NO_FLIP;
  }

  auto surface = new Surface(diffuse_texture, displacement_texture, flip, filename);
  // HD fork: same logical-size handling as from_file. Resolve the texture
  // path the same way TextureManager::get(ReaderMapping) does.
  if (diffuse_texture_mapping)
  {
    std::string tex_file;
    if (diffuse_texture_mapping->get("file", tex_file))
    {
      tex_file = FileSystem::join(diffuse_texture_mapping->get_doc().get_directory(), tex_file);
      if (TextureManager::current())
        surface->set_hd_scale(TextureManager::current()->get_hd_scale(tex_file));
    }
  }
  return SurfacePtr(surface);
}

SurfacePtr
Surface::from_file(const std::string& filename, const std::optional<Rect>& rect)
{
  if (StringUtil::has_suffix(filename, ".surface"))
  {
    ReaderDocument doc = ReaderDocument::from_file(filename);
    ReaderObject object = doc.get_root();
    if (object.get_name() != "supertux-surface")
    {
      std::ostringstream msg;
      msg << filename << ": error: not a 'supertux-surface' file";
      throw std::runtime_error(msg.str());
    }
    else
    {
      return Surface::from_reader(object.get_mapping(), rect, filename);
    }
  }
  else
  {
    auto textureManager = TextureManager::current();
    TexturePtr texture = rect ?
      textureManager->get(filename, *rect) : textureManager->get(filename);

    SurfacePtr surface(new Surface(texture, TexturePtr(), NO_FLIP, filename));
    // HD fork: keep world-space size constant for upscaled textures.
    if (textureManager)
    {
      const int hd_scale = textureManager->get_hd_scale(filename);
      surface->set_hd_scale(hd_scale);
      if (filename.find("tux") != std::string::npos)
        log_debug << "HDDBG from_file '" << filename << "' scale=" << hd_scale
                  << " tex=" << texture->get_image_width() << "x" << texture->get_image_height()
                  << std::endl;
    }
    return surface;
  }
}

Surface::Surface(const TexturePtr& diffuse_texture,
                 const TexturePtr& displacement_texture,
                 Flip flip, const std::string& filename) :
  m_diffuse_texture(diffuse_texture),
  m_displacement_texture(displacement_texture),
  m_region(0, 0, m_diffuse_texture->get_image_width(), m_diffuse_texture->get_image_height()),
  m_flip(flip),
  m_source_filename(filename)
{
}

Surface::Surface(const TexturePtr& diffuse_texture,
                 const TexturePtr& displacement_texture,
                 const Rect& region,
                 Flip flip, const std::string& filename) :
  m_diffuse_texture(diffuse_texture),
  m_displacement_texture(displacement_texture),
  m_region(region),
  m_flip(flip),
  m_source_filename(filename)
{
}

SurfacePtr
Surface::from_texture(const TexturePtr& texture)
{
  return SurfacePtr(new Surface(texture, TexturePtr(), NO_FLIP));
}

Surface::~Surface()
{
}

SurfacePtr
Surface::clone(Flip flip) const
{
  SurfacePtr surface(new Surface(m_diffuse_texture,
                                 m_displacement_texture,
                                 m_region,
                                 m_flip ^ flip));
  // HD fork: mirrored/flipped actions (e.g. every "*-left" sprite frame)
  // must keep the scale, otherwise HD textures draw at HD pixel size.
  surface->m_hd_scale = m_hd_scale;
  return surface;
}

SurfacePtr
Surface::region(const Rect& rect) const
{
  // HD fork: incoming rects are in logical pixels (e.g. 32x32 tiles as
  // authored); convert to texture pixels and keep the scale.
  const Rect hd_rect(rect.left * m_hd_scale, rect.top * m_hd_scale,
                     rect.right * m_hd_scale, rect.bottom * m_hd_scale);
  SurfacePtr surface(new Surface(m_diffuse_texture,
                                 m_displacement_texture,
                                 hd_rect,
                                 m_flip));
  surface->m_hd_scale = m_hd_scale;
  return surface;
}

TexturePtr
Surface::get_texture() const
{
  return m_diffuse_texture;
}

TexturePtr
Surface::get_displacement_texture() const
{
  return m_displacement_texture;
}

int
Surface::get_width() const
{
  return m_region.get_width() / m_hd_scale;
}

int
Surface::get_height() const
{
  return m_region.get_height() / m_hd_scale;
}
