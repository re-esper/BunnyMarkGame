#version 450 core

layout (location = 0) in vec4 inPositionTexcoord;
layout (location = 1) in vec4 inSpriteScaleRotation;
layout (location = 2) in vec2 inSpritePosition;

layout (binding = 0) uniform UBO {
	mat4 projection;
} ubo;

layout(location = 0) out vec2 outTexcoord;

void main()
{
	outTexcoord = inPositionTexcoord.zw;
	vec2 position = inPositionTexcoord.xy * mat2(inSpriteScaleRotation.xyzw) + inSpritePosition;
	gl_Position = ubo.projection * vec4(position, 0.0, 1.0);
}