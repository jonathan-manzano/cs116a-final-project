// skybox fragment shader with procedural gradient
#version 410 core

out vec4 FragColor;

in vec3 TexCoords;

uniform vec3 sunDirection = vec3(0.3, 0.5, 0.2);

vec3 getSkyColor(vec3 direction)
{
    // Normalize direction
    vec3 dir = normalize(direction);
    
    // Sky colors
    vec3 skyTop = vec3(0.2, 0.4, 0.8);      // Deep blue at top
    vec3 skyHorizon = vec3(0.6, 0.75, 0.9); // Light blue at horizon
    vec3 sunColor = vec3(1.0, 0.95, 0.8);   // Warm sun color
    
    // Calculate vertical gradient (based on Y component)
    float heightFactor = dir.y;
    
    // Smooth gradient from horizon to sky
    float t = smoothstep(-0.1, 0.6, heightFactor);
    vec3 skyColor = mix(skyHorizon, skyTop, t);
    
    // Add sun
    vec3 sun = normalize(sunDirection);
    float sunDot = max(dot(dir, sun), 0.0);
    float sunIntensity = pow(sunDot, 256.0); // Sharp sun disk
    float sunGlow = pow(sunDot, 8.0) * 0.3;  // Soft glow around sun
    
    // Mix sun into sky
    skyColor = mix(skyColor, sunColor, sunIntensity);
    skyColor += sunColor * sunGlow;
    
    // Add subtle ground color below horizon
    if (heightFactor < 0.0)
    {
        vec3 groundColor = vec3(0.3, 0.25, 0.2); // Brownish ground
        float groundMix = smoothstep(0.0, -0.2, heightFactor);
        skyColor = mix(skyColor, groundColor, groundMix);
    }
    
    return skyColor;
}

void main()
{
    vec3 color = getSkyColor(TexCoords);
    FragColor = vec4(color, 1.0);
}
