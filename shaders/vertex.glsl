// vertex shader
#version 410 core

layout (location = 0) in vec3 aPos; //reads a vec3 from attribute index 0 and stores in shader variable aPos

uniform mat4 model; //object transform
uniform mat4 view;  //camera transform
uniform mat4 projection;   //perspective projection

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}