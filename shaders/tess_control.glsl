// tessellation control shader
#version 410 core

layout (vertices = 3) out;

in vec3 vPos[];
in vec3 vNormal[];
in vec2 vTexCoord[];

out vec3 tcPos[];
out vec3 tcNormal[];
out vec2 tcTexCoord[];

uniform vec3 viewPos;
uniform float minTessLevel = 1.0;
uniform float maxTessLevel = 8.0;
uniform float minDistance = 2.0;   // Adjusted for 60x60 map
uniform float maxDistance = 50.0;  // Adjusted for 60x60 map

float calcTessLevel(vec3 p0, vec3 p1)
{
    // Calculate distance from camera to edge midpoint
    vec3 midpoint = (p0 + p1) * 0.5;
    float distance = length(viewPos - midpoint);
    
    // Map distance to tessellation level
    float t = clamp((distance - minDistance) / (maxDistance - minDistance), 0.0, 1.0);
    float tessLevel = mix(maxTessLevel, minTessLevel, t);
    
    return tessLevel;
}

void main()
{
    // Pass through position, normal, and UV
    tcPos[gl_InvocationID] = vPos[gl_InvocationID];
    tcNormal[gl_InvocationID] = vNormal[gl_InvocationID];
    tcTexCoord[gl_InvocationID] = vTexCoord[gl_InvocationID];
    
    // Only first invocation per patch sets tessellation levels
    if (gl_InvocationID == 0)
    {
        // Outer tessellation levels (edges of triangle)
        gl_TessLevelOuter[0] = calcTessLevel(vPos[1], vPos[2]);
        gl_TessLevelOuter[1] = calcTessLevel(vPos[2], vPos[0]);
        gl_TessLevelOuter[2] = calcTessLevel(vPos[0], vPos[1]);
        
        // Inner tessellation level (interior of triangle)
        gl_TessLevelInner[0] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[1] + gl_TessLevelOuter[2]) / 3.0;
    }
}
