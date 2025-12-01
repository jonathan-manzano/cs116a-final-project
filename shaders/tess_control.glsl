// tessellation control shader
#version 410 core

layout (vertices = 3) out; // output 3 control points per patch (triangle)

in vec3 vPos[];
in vec3 vNormal[];

out vec3 tcPos[];
out vec3 tcNormal[];

uniform vec3 viewPos;
uniform float minTessLevel = 1.0;
uniform float maxTessLevel = 8.0;  // REDUCED from 64.0 to 8.0
uniform float minDistance = 1.0;   // Start tessellating closer
uniform float maxDistance = 15.0;  // Stop tessellating sooner

float calcTessLevel(vec3 p0, vec3 p1)
{
    // calculate distance from camera to edge midpoint
    vec3 midpoint = (p0 + p1) * 0.5;
    float distance = length(viewPos - midpoint);
    
    // map distance to tessellation level
    // closer = higher tessellation, farther = lower tessellation
    float t = clamp((distance - minDistance) / (maxDistance - minDistance), 0.0, 1.0);
    float tessLevel = mix(maxTessLevel, minTessLevel, t);
    
    return tessLevel;
}

void main()
{
    // pass through position and normal to next stage
    tcPos[gl_InvocationID] = vPos[gl_InvocationID];
    tcNormal[gl_InvocationID] = vNormal[gl_InvocationID];
    
    // only first invocation per patch sets tessellation levels
    if (gl_InvocationID == 0)
    {
        // outer tessellation levels (edges of triangle)
        gl_TessLevelOuter[0] = calcTessLevel(vPos[1], vPos[2]); // edge opposite to vertex 0
        gl_TessLevelOuter[1] = calcTessLevel(vPos[2], vPos[0]); // edge opposite to vertex 1
        gl_TessLevelOuter[2] = calcTessLevel(vPos[0], vPos[1]); // edge opposite to vertex 2
        
        // inner tessellation level (interior of triangle)
        gl_TessLevelInner[0] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[1] + gl_TessLevelOuter[2]) / 3.0;
    }
}
