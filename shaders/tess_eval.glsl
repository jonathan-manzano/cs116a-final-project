// tessellation evaluation shader
#version 410 core

layout (triangles, equal_spacing, ccw) in;

in vec3 tcPos[];
in vec3 tcNormal[];
in vec2 tcTexCoord[];

out float heightVal;
out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Simple noise function for displacement detail
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Multi-octave noise for terrain detail
float fbm(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    // Reduced to 3 octaves for better performance
    for (int i = 0; i < 3; i++)
    {
        value += amplitude * noise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    
    return value;
}

void main()
{
    // Interpolate position using barycentric coordinates
    vec3 p0 = gl_TessCoord.x * tcPos[0];
    vec3 p1 = gl_TessCoord.y * tcPos[1];
    vec3 p2 = gl_TessCoord.z * tcPos[2];
    vec3 pos = p0 + p1 + p2;
    
    // Interpolate UV coordinates using barycentric coordinates
    vec2 uv0 = gl_TessCoord.x * tcTexCoord[0];
    vec2 uv1 = gl_TessCoord.y * tcTexCoord[1];
    vec2 uv2 = gl_TessCoord.z * tcTexCoord[2];
    TexCoord = uv0 + uv1 + uv2;
    
    // Add procedural displacement for terrain detail
    float detailScale = 12.0; // Reduced for less detail
    float detailNoise = fbm(TexCoord * detailScale);
    
    // Apply displacement only to mid-high elevations (not water)
    float heightFactor = pos.y / 2.5; // Updated to match HEIGHT_SCALE
    float displacementAmount = smoothstep(0.15, 1.0, heightFactor);
    
    // Add subtle vertical displacement - reduced for less spikiness
    pos.y += detailNoise * 0.01 * displacementAmount; // Reduced from 0.015
    
    // Interpolate normal using barycentric coordinates
    vec3 n0 = gl_TessCoord.x * tcNormal[0];
    vec3 n1 = gl_TessCoord.y * tcNormal[1];
    vec3 n2 = gl_TessCoord.z * tcNormal[2];
    vec3 normal = normalize(n0 + n1 + n2);
    
    // Transform to world space
    vec4 worldPos = model * vec4(pos, 1.0);
    FragPos = worldPos.xyz;
    
    // Transform normal to world space
    Normal = mat3(transpose(inverse(model))) * normal;
    
    // Calculate final position
    gl_Position = projection * view * worldPos;
    
    // Pass normalized height (0-1) to fragment shader
    heightVal = pos.y / 2.5; // Updated to match HEIGHT_SCALE
}
