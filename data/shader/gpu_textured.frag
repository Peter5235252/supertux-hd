// SuperTux-HD Vulkan backend: textured-quad fragment shader (SDL_GPU).
// Direct port of data/shader/shader330.frag: uniform samplers live in
// set 2 (bindings 0..2), scalar/vector uniforms in a set-1 block
// (std140: mat3 = 48B, then backbuffer/game_time/animate,
// displacement_animate/is_displacement/blur).
#version 450

layout(set = 2, binding = 0) uniform sampler2D diffuse_texture;
layout(set = 2, binding = 1) uniform sampler2D displacement_texture;
layout(set = 2, binding = 2) uniform sampler2D framebuffer_texture;

layout(set = 3, binding = 0) uniform FragBlock {
  mat3 fragcoord2uv;
  float backbuffer;
  float game_time;
  vec2 animate;
  vec2 displacement_animate;
  int is_displacement;
  int blur;
};

layout(location = 0) in vec2 var_uv;
layout(location = 1) in vec4 var_color;

layout(location = 0) out vec4 fragColor;

void main(void)
{
  if (backbuffer == 0.0 || is_displacement == 0 || blur != 0)
  {
    vec4 color = var_color * texture(diffuse_texture, var_uv.st + (animate * game_time));
    if (blur != 0)
    {
      vec2 uv = (fragcoord2uv * gl_FragCoord.xyw).xy;
      uv.y = 1.0 - uv.y;
      vec2 texel = vec2(fragcoord2uv[0].x, fragcoord2uv[1].y);
      float num = 0.0;
      vec4 sum = vec4(0.0);

      for (float y = -float(blur); y <= float(blur); ++y)
      {
        for (float x = -float(blur); x <= float(blur); ++x)
        {
          vec2 offset = vec2(x, y) * texel;
          sum += texture(framebuffer_texture, uv + offset);
          num++;
        }
      }

      fragColor = vec4(mix(color.rgb, (sum / num).rgb * 1.05, color.a), 1.0);
    }
    else
    {
      fragColor = color;
    }
  }
  else
  {
    vec4 pixel = texture(displacement_texture, var_uv.st + (displacement_animate * game_time));
    vec2 displacement = (pixel.rg - vec2(0.5, 0.5)) * 255.0;
    float alpha = pixel.a;

    vec2 uv = (fragcoord2uv * (gl_FragCoord.xyw + vec3(displacement.xy * alpha, 0.0))).xy;
    uv = vec2(uv.x, 1.0 - uv.y);
    vec4 back_color = texture(framebuffer_texture, uv);

    vec4 color = var_color * texture(diffuse_texture, var_uv.st + (animate * game_time));
    fragColor = vec4(mix(color.rgb, back_color.rgb, alpha), color.a);
  }
}
