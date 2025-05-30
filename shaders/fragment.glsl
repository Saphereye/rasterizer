#version 330 core
out vec4 FragColor;
in vec3 Normal;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3)); // Directional light
    float diff = max(dot(normalize(Normal), lightDir), 0.0);  // Changed FragNormal to Normal
    vec3 diffuse = diff * vec3(0.3, 0.6, 1.0); // Light color
    FragColor = vec4(diffuse, 1.0);
}
