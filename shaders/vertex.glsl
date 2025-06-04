#version 420 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

out vec3      FragPos;
out vec3      Normal;
flat out vec3 randColorSeed;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform int  useRandomColor;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal  = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
    
    // Use triangle ID instead of vertex position for better randomization
    // Each triangle (3 vertices) gets the same seed
    int triangleID = gl_VertexID / 3;
    
    // Convert triangle ID to a vec3 seed that will give good randomization
    randColorSeed = vec3(
        float(triangleID),
        float(triangleID * 17),    // Different multipliers to avoid patterns
        float(triangleID * 31)
    );
}
