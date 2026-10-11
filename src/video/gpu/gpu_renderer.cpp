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

#include "video/gpu/gpu_renderer.hpp"

#include "video/gpu/gpu_painter.hpp"
#include "video/gpu/gpu_video_system.hpp"

GPURenderer::GPURenderer(GPUVideoSystem& video_system) :
  m_video_system(video_system),
  m_painter(new GPUPainter(video_system))
{
}

GPURenderer::~GPURenderer()
{
}

Painter&
GPURenderer::get_painter()
{
  return *m_painter;
}

Rect
GPURenderer::get_rect() const
{
  return m_video_system.get_viewport().get_rect();
}

Size
GPURenderer::get_logical_size() const
{
  return m_video_system.get_viewport().get_screen_size();
}
