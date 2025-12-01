// fragment shader
#version 410 core

in float heightVal; 
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 viewPos;

void main()
{
    // base color from height
    vec3 lowColor  = vec3(0.1, 0.3, 0.1); // dark green
    vec3 highColor = vec3(0.8, 0.8, 0.7); // whitish
    vec3 baseColor = mix(lowColor, highColor, heightVal);

    vec3 lightDir   = normalize(vec3(0.3, 1.0, 0.2));
    vec3 lightColor = vec3(1.0);

    // ambient
    vec3 ambient = 0.25 * baseColor;

    // diffuse
    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseColor;

    // specular
    vec3 viewDir    = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular   = 0.15 * spec * lightColor;

    vec3 color = ambient + diffuse + specular;
    FragColor = vec4(color, 1.0);
}
