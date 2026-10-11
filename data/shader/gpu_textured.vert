// SuperTux-HD Vulkan backend: textured-quad vertex shader (SDL_GPU).
// Positions arrive in physical pixels; MVP maps to Vulkan NDC
// (y-down positions -> NDC, framebuffer origin top-left).
#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 var_uv;
layout(location = 1) out vec4 var_color;

void main()
{
  // Positions arrive pre-transformed to Vulkan NDC (see GPUPainter).
  var_uv = in_uv;
  var_color = in_color;
  gl_Position = vec4(in_pos, 0.0, 1.0);
}
