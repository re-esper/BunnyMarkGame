#version 450

layout (binding = 1) uniform sampler2D uTexture;

layout (location = 0) in vec2 inTexcoord;

layout (location = 0) out vec4 outColor;

void main() 
{
	outColor = texture(uTexture, inTexcoord);
}