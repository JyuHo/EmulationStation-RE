#pragma once

#include <string_view>

// GLSL sources of the renderer (OpenGL 3.3 core). Attribute locations match Renderer::Vertex:
// 0 = pos, 1 = tex, 2 = color (normalized unsigned bytes).
namespace Renderer::Shaders {

inline constexpr std::string_view vertex = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
layout(location = 2) in vec4 aColor;
uniform mat4 uProjection;
uniform mat4 uModelView;
out vec2 vTex;
out vec4 vColor;
void main()
{
	vTex = aTex;
	vColor = aColor;
	gl_Position = uProjection * uModelView * vec4(aPos, 0.0, 1.0);
}
)";

// images and fonts: GL_MODULATE of the fixed pipeline (colour x texel); the font atlases are
// swizzled to (1, 1, 1, r)
inline constexpr std::string_view fragmentTexture = R"(#version 330 core
in vec2 vTex;
in vec4 vColor;
uniform sampler2D uTexture;
out vec4 fragColor;
void main()
{
	fragColor = vColor * texture(uTexture, vTex);
}
)";

// video frames: YUV 4:2:0 planes (BT.601, limited range) to RGB
inline constexpr std::string_view fragmentYuv = R"(#version 330 core
in vec2 vTex;
in vec4 vColor;
uniform sampler2D uY;
uniform sampler2D uU;
uniform sampler2D uV;
out vec4 fragColor;
void main()
{
	float y = 1.164383 * (texture(uY, vTex).r - 0.0627451);
	float u = texture(uU, vTex).r - 0.5;
	float v = texture(uV, vTex).r - 0.5;
	vec3 rgb = vec3(y + 1.596027 * v, y - 0.391762 * u - 0.812968 * v, y + 2.017232 * u);
	fragColor = vColor * vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)";

// rectangles and lines
inline constexpr std::string_view fragmentColor = R"(#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main()
{
	fragColor = vColor;
}
)";

} // namespace Renderer::Shaders
