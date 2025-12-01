// vertex shader (tessellation pipeline)
#version 410 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 vPos;
out vec3 vNormal;

void main()
{
    // just pass through to tessellation control shader
    vPos = aPos;
    vNormal = aNormal;
}
