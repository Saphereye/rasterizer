#version 420 core
in vec3      FragPos;
in vec3      Normal;
flat in vec3 randColorSeed;

out vec4 FragColor;

uniform vec3 baseColor;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform int  useShading;
uniform int  useDepthBuffer;
uniform int  useRandomColor;

vec3 randColor(vec3 seed)
{
    // Improved random function with better distribution
    // Use different prime numbers and offsets for each component
    vec3 p = vec3(
        dot(seed, vec3(127.1, 311.7, 74.7)),
        dot(seed, vec3(269.5, 183.3, 246.1)),
        dot(seed, vec3(113.5, 271.9, 124.6))
    );
    
    vec3 fracted = fract(sin(p) * 43758.5453123);
    
    // Optional: Make colors more vibrant by avoiding very dark colors
    return 0.3 + 0.7 * fracted;  // Range: 0.3 to 1.0 instead of 0.0 to 1.0
}

void main()
{
    if (useDepthBuffer == 1)
    {
        // Depth buffer visualization
        float depth = gl_FragCoord.z / gl_FragCoord.w;
        depth       = (depth - 0.0) /
                (1.0 - 0.0); // Normalize depth assuming near=0.0 and far=1.0
        FragColor = vec4(vec3(depth), 1.0);
    }
    else if (useShading == 1)
    {
        // Phong shading
        vec3 norm     = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        
        // Choose color based on mode
        vec3 materialColor = (useRandomColor == 1) ? randColor(randColorSeed) : baseColor;
        
        // Ambient
        float ambientStrength = 0.3;
        vec3  ambient         = ambientStrength * materialColor;
        
        // Diffuse
        float diff    = max(dot(norm, lightDir), 0.0);
        vec3  diffuse = diff * materialColor;
        
        // Specular
        float specularStrength = 0.5;
        vec3  viewDir          = normalize(viewPos - FragPos);
        vec3  reflectDir       = reflect(-lightDir, norm);
        float spec             = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3  specular         = specularStrength * spec * vec3(1.0);
        
        vec3 result = ambient + diffuse + specular;
        FragColor   = vec4(result, 1.0);
    }
    else if (useRandomColor == 1)
    {
        // Random color based on seed
        vec3 randomColor = randColor(randColorSeed);
        FragColor        = vec4(randomColor, 1.0);
    }
    else
    {
        // Use the base color directly
        FragColor = vec4(baseColor, 1.0);
    }
}
