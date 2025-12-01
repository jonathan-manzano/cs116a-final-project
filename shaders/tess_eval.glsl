// tessellation evaluation shader
#version 410 core

layout (triangles, equal_spacing, ccw) in;

in vec3 tcPos[];
in vec3 tcNormal[];

out float heightVal;
out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // interpolate position using barycentric coordinates
    vec3 p0 = gl_TessCoord.x * tcPos[0];
    vec3 p1 = gl_TessCoord.y * tcPos[1];
    vec3 p2 = gl_TessCoord.z * tcPos[2];
    vec3 pos = p0 + p1 + p2;
    
    // interpolate normal using barycentric coordinates
    vec3 n0 = gl_TessCoord.x * tcNormal[0];
    vec3 n1 = gl_TessCoord.y * tcNormal[1];
    vec3 n2 = gl_TessCoord.z * tcNormal[2];
    vec3 normal = normalize(n0 + n1 + n2);
    
    // transform to world space
    vec4 worldPos = model * vec4(pos, 1.0);
    FragPos = worldPos.xyz;
    
    // transform normal to world space
    Normal = mat3(transpose(inverse(model))) * normal;
    
    // calculate final position
    gl_Position = projection * view * worldPos;
    
    // pass normalized height (0-1) to fragment shader
    heightVal = pos.y / 0.3; // normalize to 0-1 range (matching original heightScale)
}
