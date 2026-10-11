// Fullscreen-triangle vertex shader for post passes (EASU/RCAS).
// No vertex buffers: positions derived from gl_VertexIndex.
#version 450

layout(location = 0) out vec2 var_uv;

void main()
{
  float x = float((gl_VertexIndex << 1) & 2);
  float y = float(gl_VertexIndex & 2);
  var_uv = vec2(x, y);
  gl_Position = vec4(x * 2.0 - 1.0, y * 2.0 - 1.0, 0.0, 1.0);
}
