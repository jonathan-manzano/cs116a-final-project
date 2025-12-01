// fragment shader with height-based procedural texturing
#version 410 core

in float heightVal;
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 viewPos;

// Height-based terrain zones
vec3 getTerrainColor(float height)
{
    // Define elevation zones with smooth transitions
    
    // Water/Beach (0.0 - 0.15)
    vec3 waterColor = vec3(0.1, 0.3, 0.5);
    vec3 sandColor = vec3(0.76, 0.70, 0.50);
    
    // Grass/Forest (0.15 - 0.5)
    vec3 grassColor = vec3(0.2, 0.5, 0.2);
    vec3 forestColor = vec3(0.13, 0.37, 0.13);
    
    // Mountain/Rock (0.5 - 0.8)
    vec3 rockColor = vec3(0.5, 0.5, 0.5);
    vec3 darkRockColor = vec3(0.3, 0.3, 0.3);
    
    // Snow (0.8 - 1.0)
    vec3 snowColor = vec3(0.9, 0.9, 0.95);
    
    vec3 color;
    
    if (height < 0.1)
    {
        // Water to beach transition
        float t = height / 0.1;
        color = mix(waterColor, sandColor, t);
    }
    else if (height < 0.25)
    {
        // Beach to grass transition
        float t = (height - 0.1) / 0.15;
        color = mix(sandColor, grassColor, t);
    }
    else if (height < 0.5)
    {
        // Grass to forest
        float t = (height - 0.25) / 0.25;
        color = mix(grassColor, forestColor, t);
    }
    else if (height < 0.7)
    {
        // Forest to rock
        float t = (height - 0.5) / 0.2;
        color = mix(forestColor, rockColor, t);
    }
    else if (height < 0.85)
    {
        // Rock variations
        float t = (height - 0.7) / 0.15;
        color = mix(rockColor, darkRockColor, t);
    }
    else
    {
        // Rock to snow
        float t = (height - 0.85) / 0.15;
        color = mix(darkRockColor, snowColor, t);
    }
    
    return color;
}

// Simple pseudo-random noise function for texture detail
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

void main()
{
    // Get base terrain color from height
    vec3 baseColor = getTerrainColor(heightVal);
    
    // Add procedural texture detail using UV coordinates
    float detailScale = 25.0; // Reduced for performance
    float noiseVal = noise(TexCoord * detailScale);
    
    // Add another layer of detail at different scale
    float largeScale = 6.0; // Reduced
    float largeNoise = noise(TexCoord * largeScale);
    
    // Combine noise layers for more interesting texture
    float combinedNoise = mix(noiseVal, largeNoise, 0.5);
    
    // Mix in noise for texture variation - reduced intensity
    baseColor = mix(baseColor, baseColor * combinedNoise, 0.18);
    
    // Calculate normal once (used for both slope and lighting)
    vec3 norm = normalize(Normal);
    
    // Add color variation based on slope (steeper = darker)
    float slope = 1.0 - abs(norm.y);
    baseColor = mix(baseColor, baseColor * 0.7, slope * 0.3);
    
    // ===== LIGHTING (BLINN-PHONG) =====
    
    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.2));
    vec3 lightColor = vec3(1.0);
    
    // Ambient lighting
    vec3 ambient = 0.3 * baseColor;
    
    // Diffuse lighting (reuse norm from above)
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseColor * lightColor;
    
    // Blinn-Phong specular lighting
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir); // Blinn-Phong uses halfway vector
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
    
    // Vary specular intensity based on terrain type
    float specularIntensity = heightVal > 0.85 ? 0.4 : 0.1; // Snow is more reflective
    vec3 specular = specularIntensity * spec * lightColor;
    
    // Combine all lighting
    vec3 result = ambient + diffuse + specular;
    
    FragColor = vec4(result, 1.0);
}
