// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "camera.h"
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cstdio>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <sstream>
#include <vector>
// clang-format on

struct BoundingBox
{
    glm::vec3 min;
    glm::vec3 max;
};

BoundingBox compute_bounding_box(const std::vector<float>& vertices)
{
    BoundingBox bbox;
    bbox.min = glm::vec3(FLT_MAX);
    bbox.max = glm::vec3(-FLT_MAX);

    for (size_t i = 0; i < vertices.size(); i += 3)
    {
        glm::vec3 v(vertices[i], vertices[i + 1], vertices[i + 2]);
        bbox.min = glm::min(bbox.min, v);
        bbox.max = glm::max(bbox.max, v);
    }

    return bbox;
}

Camera camera({ 0, 0, 3 }, { 0, 1, 0 }, -90, 0);
float  lastX = 400, lastY = 300;
bool   firstMouse = true;
float  deltaTime = 0, lastFrame = 0;

void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    glViewport(0, 0, w, h);
}

void mouse_callback(GLFWwindow*, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX      = xpos;
        lastY      = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed
    lastX         = xpos;
    lastY         = ypos;
    camera.process_mouse_movement(xoffset * 0.5, yoffset * 0.5);
}

void process_input(GLFWwindow* win)
{
    float dt = deltaTime * 0.5;
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
        camera.process_keyboard(FORWARD, dt);
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
        camera.process_keyboard(BACKWARD, dt);
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS)
        camera.process_keyboard(LEFT, dt);
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS)
        camera.process_keyboard(RIGHT, dt);
    if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.process_keyboard(UP, dt);
    if (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.process_keyboard(DOWN, dt);
    if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) std::exit(0);
}

std::string loadShader(const char* path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        std::cerr << "Failed to open shader file: " << path << '\n';
        std::exit(EXIT_FAILURE);
    }
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

unsigned int create_shader_program()
{
    std::string vSrc = loadShader("../shaders/vertex.glsl");
    std::string fSrc = loadShader("../shaders/fragment.glsl");

    auto compile = [](const std::string& src, GLenum type)
    {
        const char*  code   = src.c_str();
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &code, nullptr);
        glCompileShader(shader);

        int  success;
        char infoLog[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 512, NULL, infoLog);
            std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n"
                      << infoLog << std::endl;
        }

        return shader;
    };

    unsigned int vShader = compile(vSrc, GL_VERTEX_SHADER);
    unsigned int fShader = compile(fSrc, GL_FRAGMENT_SHADER);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vShader);
    glAttachShader(program, fShader);
    glLinkProgram(program);

    int  success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                  << infoLog << std::endl;
    }

    glDeleteShader(vShader);
    glDeleteShader(fShader);
    return program;
}

struct VertexFormat {
    bool hasNormals;
    bool hasTexCoords;
    int stride; // floats per vertex
};

VertexFormat analyzeScene(const aiScene* scene) {
    VertexFormat fmt = {false, false, 3}; // minimum: position only

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];
        if (mesh->HasNormals()) fmt.hasNormals = true;
        if (mesh->HasTextureCoords(0)) fmt.hasTexCoords = true;
    }

    fmt.stride = 3; // position
    if (fmt.hasNormals) fmt.stride += 3;
    if (fmt.hasTexCoords) fmt.stride += 2;

    std::cout << "Scene format: pos=3, normals=" << (fmt.hasNormals ? 3 : 0)
              << ", texcoords=" << (fmt.hasTexCoords ? 2 : 0)
              << ", stride=" << fmt.stride << std::endl;

    return fmt;
}

void extractVertices(aiNode* node, const aiScene* scene, std::vector<float>& vertices) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        std::cout << "Processing mesh " << i << ": " << mesh->mNumFaces << " faces, "
                  << mesh->mNumVertices << " vertices" << std::endl;

        // Process each face and extract vertices in order
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];

            // Each face should be a triangle (due to aiProcess_Triangulate)
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                unsigned int vertexIndex = face.mIndices[k];

                // Position
                aiVector3D pos = mesh->mVertices[vertexIndex];
                vertices.push_back(pos.x);
                vertices.push_back(pos.y);
                vertices.push_back(pos.z);

                // Normal (with fallback)
                if (mesh->HasNormals()) {
                    aiVector3D normal = mesh->mNormals[vertexIndex];
                    vertices.push_back(normal.x);
                    vertices.push_back(normal.y);
                    vertices.push_back(normal.z);
                } else {
                    // Generate a simple face normal or use default
                    vertices.push_back(0.0f);
                    vertices.push_back(1.0f);
                    vertices.push_back(0.0f);
                }
            }
        }
    }

    // Process child nodes
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        extractVertices(node->mChildren[i], scene, vertices);
    }
}

// Updated main function - simpler approach
int main(int argc, char** argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1600, 1200, "Window", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_model.obj>" << std::endl;
        return -1;
    }

    std::string modelPath = argv[1];
    unsigned int shader = create_shader_program();

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        modelPath,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return -1;
    }

    std::vector<float> vertices;
    std::cout << "Number of meshes: " << scene->mNumMeshes << std::endl;
    extractVertices(scene->mRootNode, scene, vertices);

    int totalVertices = vertices.size() / 6;  // 6 floats per vertex (pos + normal)
    std::cout << "Total vertices extracted: " << totalVertices << std::endl;
    std::cout << "Total triangles: " << totalVertices / 3 << std::endl;

    // Compute bounding box from position data only
    BoundingBox bbox;
    bbox.min = glm::vec3(FLT_MAX);
    bbox.max = glm::vec3(-FLT_MAX);

    for (size_t i = 0; i < vertices.size(); i += 6) {  // Every 6 floats, take first 3
        glm::vec3 v(vertices[i], vertices[i + 1], vertices[i + 2]);
        bbox.min = glm::min(bbox.min, v);
        bbox.max = glm::max(bbox.max, v);
    }

    glm::vec3 center = (bbox.min + bbox.max) * 0.5f;
    glm::vec3 size = bbox.max - bbox.min;
    float maxExtent = std::max({size.x, size.y, size.z});

    // Camera setup
    float verticalFov = glm::radians(45.0f);
    float distance = (maxExtent * 0.5f) / std::tan(verticalFov * 0.5f);
    glm::vec3 camPos = center + glm::vec3(0.0f, 0.0f, 1.0f) * distance;

    camera = Camera(camPos, glm::vec3(0, 1, 0), -90.f, 0.f);
    camera.Front = glm::normalize(center - camPos);

    std::cout << "Bounding box: min(" << bbox.min.x << ", " << bbox.min.y << ", " << bbox.min.z
              << ") max(" << bbox.max.x << ", " << bbox.max.y << ", " << bbox.max.z << ")" << std::endl;
    std::cout << "Center: (" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;
    std::cout << "Camera position: (" << camPos.x << ", " << camPos.y << ", " << camPos.z << ")" << std::endl;

    // OpenGL setup
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window)) {
        float time = glfwGetTime();
        deltaTime = time - lastFrame;
        lastFrame = time;

        process_input(window);
        glClearColor(0.1f, 0.1f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader);
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = camera.get_view_matrix();
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1600.f / 1200.f, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &model[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, &proj[0][0]);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, totalVertices);  // Use vertex count, not array size

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
