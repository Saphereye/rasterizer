// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <GL/freeglut.h>
#include "camera.h"
#include "mesh.h"
#include <algorithm>
#include <assimp/Importer.hpp>
#include <ft2build.h>
#include <map>
#include FT_FREETYPE_H
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cstdio>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
// clang-format on

struct Character
{
    unsigned int textureID; // ID handle of the glyph texture
    glm::ivec2   size;      // Size of glyph
    glm::ivec2   bearing;   // Offset from baseline to left/top of glyph
    unsigned int advance;   // Horizontal offset to advance to next glyph
};

std::map<char, Character> characters;

void load_font(const char* fontPath)
{
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library\n";
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, fontPath, 0, &face))
    {
        std::cerr << "ERROR::FREETYPE: Failed to load font\n";
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, 48); // Set font size

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Disable byte-alignment restriction

    for (unsigned char c = 0; c < 128; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cerr << "Failed to load glyph: " << c << "\n";
            continue;
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RED,
                     face->glyph->bitmap.width,
                     face->glyph->bitmap.rows,
                     0,
                     GL_RED,
                     GL_UNSIGNED_BYTE,
                     face->glyph->bitmap.buffer);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character ch = { texture,
                         { face->glyph->bitmap.width, face->glyph->bitmap.rows },
                         { face->glyph->bitmap_left, face->glyph->bitmap_top },
                         (GLuint)face->glyph->advance.x };
        characters.insert(std::pair<char, Character>(c, ch));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

unsigned int textVAO, textVBO;

void render_text(const unsigned int& shader,
                 std::string         text,
                 float               x,
                 float               y,
                 float               scale,
                 glm::vec3           color)
{
    glUniform3f(
        glGetUniformLocation(shader, "textColor"), color.r, color.g, color.b);
    glm::mat4 projection = glm::ortho(0.0f, 1600.0f, 0.0f, 1200.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"),
                       1,
                       GL_FALSE,
                       &projection[0][0]);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    for (char c : text)
    {
        Character ch = characters[c];

        float xpos = x + ch.bearing.x * scale;
        float ypos = y - (ch.size.y - ch.bearing.y) * scale;

        float w = ch.size.x * scale;
        float h = ch.size.y * scale;

        float vertices[6][4] = { { xpos, ypos + h, 0.0f, 0.0f },
                                 { xpos, ypos, 0.0f, 1.0f },
                                 { xpos + w, ypos, 1.0f, 1.0f },

                                 { xpos, ypos + h, 0.0f, 0.0f },
                                 { xpos + w, ypos, 1.0f, 1.0f },
                                 { xpos + w, ypos + h, 1.0f, 0.0f } };

        glBindTexture(GL_TEXTURE_2D, ch.textureID);

        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.advance >> 6) * scale; // Advance is in 1/64 pixels
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

struct BoundingBox
{
    glm::vec3 min;
    glm::vec3 max;
};

BoundingBox compute_bounding_box(const std::vector<float>& vertices)
{
    BoundingBox bbox{};
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

// Rendering modes
enum RenderMode : std::uint8_t
{
    SHADED = 0,
    WIREFRAME,
    RANDOM,
};

const auto MODE_COUNT = 3;

Camera     camera({ 0, 0, 3 }, { 0, 1, 0 }, -90, 0);
double     lastX = 400, lastY = 300;
auto       firstMouse = true;
double     deltaTime = 0, lastFrame = 0;
RenderMode currentMode   = SHADED;
bool       showDebugInfo = false;

void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    glViewport(0, 0, w, h);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
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

    // Check if middle mouse button is pressed for rotation
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
    {
        camera.process_mouse_movement(xoffset * 0.5, yoffset * 0.5);
    }
}

// Scroll callback for zoom functionality
void scroll_callback(GLFWwindow*, double, double yoffset)
{
    camera.process_keyboard(yoffset > 0 ? FORWARD : BACKWARD, /*dt=*/0.1F);
}

auto currentFPS   = 0.0; // Global variable to store current FPS
bool isFullscreen = false;

// Declare these as global/static variables outside or above the function
bool fPressed   = false;
bool tabPressed = false;
bool ePressed   = false;

int windowedPosX, windowedPosY, windowedWidth, windowedHeight;
void process_input(GLFWwindow* win)
{
    auto dt = deltaTime;

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

    // Toggle fullscreen on F key press
if (glfwGetKey(win, GLFW_KEY_F) == GLFW_PRESS)
{
    if (!fPressed)
    {
        fPressed = true;
        isFullscreen = !isFullscreen;
        
        if (isFullscreen)
        {
            // Store current windowed mode position and size
            glfwGetWindowPos(win, &windowedPosX, &windowedPosY);
            glfwGetWindowSize(win, &windowedWidth, &windowedHeight);
            
            // Get primary monitor and its video mode
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            
            // Set to true fullscreen (borderless fullscreen)
            glfwSetWindowMonitor(win, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        else
        {
            // Restore to windowed mode with previous position and size
            glfwSetWindowMonitor(win, nullptr, windowedPosX, windowedPosY, windowedWidth, windowedHeight, GLFW_DONT_CARE);
        }
    }
}
else
{
    fPressed = false;
}

    // Handle Tab key for render mode switching
    if (glfwGetKey(win, GLFW_KEY_TAB) == GLFW_PRESS)
    {
        if (!tabPressed)
        {
            currentMode = static_cast<RenderMode>((currentMode + 1) % MODE_COUNT);
            tabPressed = true;

            const char* modeNames[] = { "Shaded", "Wireframe", "Random" };
            std::cout << "Render mode: " << modeNames[currentMode] << '\n';
        }
    }
    else
    {
        tabPressed = false;
    }

    // Handle E key for debug info toggle
    if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS)
    {
        if (!ePressed)
        {
            showDebugInfo = !showDebugInfo;
            ePressed      = true;
        }
    }
    else
    {
        ePressed = false;
    }
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

unsigned int create_shader_program(const char* vertex_shader_path,
                                   const char* fragment_shader_path)
{
    std::string vSrc = loadShader(vertex_shader_path);
    std::string fSrc = loadShader(fragment_shader_path);

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

// Updated main function
int main(int argc, char** argv)
{

    glutInit(&argc, argv);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, /*value=*/4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, /*value=*/2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        /*width=*/1920,
        /*height=*/1080,
        /*title=*/"Rasterizer",
        /*monitor=*/nullptr,
        /*share=*/nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetScrollCallback(window, scroll_callback);
    showDebugInfo = false;
    std::cout << "Debug info enabled. Rendering text to screen." << '\n';

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_model.obj>" << '\n';
        return -1;
    }

    std::string modelPath = argv[1];

    auto mesh_shader = create_shader_program(
        "../shaders/vertex.glsl", "../shaders/fragment.glsl");

    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    load_font("../assets/sample.ttf");
    auto text_shader = create_shader_program(
        "../shaders/text_vertex.glsl", "../shaders/text_fragment.glsl");

    Assimp::Importer importer;
    const aiScene*   scene = importer.ReadFile(
        modelPath,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals |
            aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << '\n';
        return -1;
    }

    // Extract vertices
    std::vector<float> vertices;
    std::cout << "Number of meshes: " << scene->mNumMeshes << '\n';
    extractVertices(scene->mRootNode, scene, vertices);

    int totalVertices = vertices.size() / 6; // 6 floats per vertex (pos + normal)
    std::cout << "Total vertices extracted: " << totalVertices << '\n';
    std::cout << "Total triangles: " << totalVertices / 3 << '\n';

    // Compute bounding box from position data only
    BoundingBox bbox;
    bbox.min = glm::vec3(FLT_MAX);
    bbox.max = glm::vec3(-FLT_MAX);

    for (size_t i = 0; i < vertices.size(); i += 6)
    { // Every 6 floats, take first 3
        glm::vec3 v(vertices[i], vertices[i + 1], vertices[i + 2]);
        bbox.min = glm::min(bbox.min, v);
        bbox.max = glm::max(bbox.max, v);
    }

    glm::vec3 center    = (bbox.min + bbox.max) * 0.5f;
    glm::vec3 size      = bbox.max - bbox.min;
    float     maxExtent = std::max({ size.x, size.y, size.z });

    // Improved camera positioning with better distance calculation
    float verticalFov = glm::radians(45.0f);
    float aspectRatio = 1600.0f / 1200.0f;

    // Calculate distance to fit the entire bounding box in view
    // Account for both vertical and horizontal FOV
    float verticalDistance = (maxExtent * 0.5f) / std::tan(verticalFov * 0.5f);
    float horizontalFov    = 2.0f *
                          std::atan(std::tan(verticalFov * 0.5f) * aspectRatio);
    float horizontalDistance = (maxExtent * 0.5f) /
                               std::tan(horizontalFov * 0.5f);

    // Use the larger distance to ensure the object fits in both dimensions
    float distance = std::max(verticalDistance, horizontalDistance);

    // Add some padding (multiply by 1.5) to ensure the object is
    // comfortably in view
    distance *= 1.5f;

    // Ensure minimum distance to avoid being too close
    distance = std::max(distance, maxExtent * 2.0f);

    glm::vec3 camPos = center + glm::vec3(0.0f, 0.0f, 1.0f) * distance;

    camera       = Camera(camPos, glm::vec3(0, 1, 0), -90.f, 0.f);
    camera.Front = glm::normalize(center - camPos);

    // Set scene parameters for adaptive camera speed
    camera.set_scene_params(center, maxExtent);

    std::cout << "Bounding box: min(" << bbox.min.x << ", " << bbox.min.y
              << ", " << bbox.min.z << ") max(" << bbox.max.x << ", "
              << bbox.max.y << ", " << bbox.max.z << ")" << '\n';
    std::cout << "Center: (" << center.x << ", " << center.y << ", " << center.z
              << ")" << '\n';
    std::cout << "Max extent: " << maxExtent << '\n';
    std::cout << "Calculated distance: " << distance << '\n';
    std::cout << "Camera position: (" << camPos.x << ", " << camPos.y << ", "
              << camPos.z << ")" << '\n';
    std::cout << "Initial camera speed multiplier: "
              << camera.get_current_speed() << '\n';
    std::cout << "\nControls:" << '\n';
    std::cout << "WASD - Move camera" << '\n';
    std::cout << "Space/Shift - Move up/down" << '\n';
    std::cout << "Mouse - Look around" << '\n';
    std::cout << "Tab - Switch render modes (Shaded/Wireframe/Random)" << '\n';
    std::cout << "E - Toggle debug info" << '\n';
    std::cout << "Q - Quit" << '\n';

    // Setup for main model
    unsigned int VAO, VBO, bboxVAO, bboxVBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(float),
                 vertices.data(),
                 GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Bounding box setup
    std::vector<float> bboxVertices = {
        bbox.min.x, bbox.min.y, bbox.min.z, bbox.max.x, bbox.min.y, bbox.min.z,
        bbox.max.x, bbox.min.y, bbox.min.z, bbox.max.x, bbox.max.y, bbox.min.z,
        bbox.max.x, bbox.max.y, bbox.min.z, bbox.min.x, bbox.max.y, bbox.min.z,
        bbox.min.x, bbox.max.y, bbox.min.z, bbox.min.x, bbox.min.y, bbox.min.z,
        bbox.min.x, bbox.min.y, bbox.max.z, bbox.max.x, bbox.min.y, bbox.max.z,
        bbox.max.x, bbox.min.y, bbox.max.z, bbox.max.x, bbox.max.y, bbox.max.z,
        bbox.max.x, bbox.max.y, bbox.max.z, bbox.min.x, bbox.max.y, bbox.max.z,
        bbox.min.x, bbox.max.y, bbox.max.z, bbox.min.x, bbox.min.y, bbox.max.z,
        bbox.min.x, bbox.min.y, bbox.min.z, bbox.min.x, bbox.min.y, bbox.max.z,
        bbox.max.x, bbox.min.y, bbox.min.z, bbox.max.x, bbox.min.y, bbox.max.z,
        bbox.max.x, bbox.max.y, bbox.min.z, bbox.max.x, bbox.max.y, bbox.max.z,
        bbox.min.x, bbox.max.y, bbox.min.z, bbox.min.x, bbox.max.y, bbox.max.z,
    };

    glGenVertexArrays(1, &bboxVAO);
    glGenBuffers(1, &bboxVBO);
    glBindVertexArray(bboxVAO);

    glBindBuffer(GL_ARRAY_BUFFER, bboxVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 bboxVertices.size() * sizeof(float),
                 bboxVertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glEnable(GL_DEPTH_TEST);

    // Improved projection matrix with dynamic near/far planes
    float nearPlane = distance * 0.01F; // 1% of distance
    float farPlane  = distance * 10.0F; // 10x distance

    // Ensure reasonable bounds
    nearPlane = std::max(nearPlane, 0.001F);
    farPlane  = std::max(farPlane, nearPlane * 1000.0F);

    // FPS calculation variables
    double fpsTimer   = 0.0;
    auto   frameCount = 0;

    while (!glfwWindowShouldClose(window))
    {
        auto time = glfwGetTime();
        deltaTime = time - lastFrame;
        lastFrame = time;

        // Calculate FPS
        fpsTimer += deltaTime;
        frameCount++;
        if (fpsTimer >= 1.0)
        {
            currentFPS = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer   = 0.0;
        }

        process_input(window);

        glClearColor(0.1, 0.1, 0.1, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(mesh_shader);
        glm::mat4 model = glm::mat4(1.0);
        glm::mat4 view  = camera.get_view_matrix();
        glm::mat4 proj  = glm::perspective(
            verticalFov, 16.0F / 9.0F, nearPlane, farPlane);

        glUniformMatrix4fv(
            glGetUniformLocation(mesh_shader, "model"), 1, GL_FALSE, &model[0][0]);
        glUniformMatrix4fv(
            glGetUniformLocation(mesh_shader, "view"), 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(mesh_shader, "projection"),
                           1,
                           GL_FALSE,
                           &proj[0][0]);

        // Set shading parameters based on current mode
        if (currentMode == SHADED)
        {
            glUniform3fv(glGetUniformLocation(mesh_shader, "lightPos"),
                         1,
                         &camera.Position[0]);
            glUniform3fv(glGetUniformLocation(mesh_shader, "viewPos"),
                         1,
                         &camera.Position[0]);
            glUniform1i(glGetUniformLocation(mesh_shader, "useShading"), 1);
        }
        else
        {
            glUniform1i(glGetUniformLocation(mesh_shader, "useShading"), 0);
        }

        // Render based on current mode
        switch (currentMode)
        {
        case SHADED:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glUniform1i(glGetUniformLocation(mesh_shader, "useRandomColor"), 0);
            glUniform3f(
                glGetUniformLocation(mesh_shader, "baseColor"), 0.3, 0.6, 1.0);
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, totalVertices);
            break;

        case WIREFRAME:
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glUniform1i(glGetUniformLocation(mesh_shader, "useRandomColor"), 0);
            glUniform3f(
                glGetUniformLocation(mesh_shader, "baseColor"), 0.8, 0.8, 0.8);
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, totalVertices);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;
        case RANDOM:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glUniform1i(glGetUniformLocation(mesh_shader, "useRandomColor"), 1);
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, totalVertices);
            break;
        }

        if (showDebugInfo)
        {
            // Switch to text shader and set up for 2D rendering
            glUseProgram(text_shader);
            glDisable(GL_DEPTH_TEST); // Disable depth testing for UI overlay

            std::ostringstream debugText;
            debugText << "FPS: " << std::fixed << std::setprecision(1)
                      << currentFPS;
            render_text(text_shader,
                        debugText.str(),
                        10.0f,
                        1150.0f,
                        0.5f,
                        glm::vec3(1.0f, 1.0f, 1.0f));

            debugText.str("");
            debugText << "Pos: (" << std::fixed << std::setprecision(1)
                      << camera.Position.x << ", " << camera.Position.y << ", "
                      << camera.Position.z << ")";
            render_text(text_shader,
                        debugText.str(),
                        10.0f,
                        1125.0f,
                        0.5f,
                        glm::vec3(1.0f, 1.0f, 1.0f));

            debugText.str("");
            debugText << "Speed: " << std::fixed << std::setprecision(2)
                      << camera.get_current_speed();
            render_text(text_shader,
                        debugText.str(),
                        10.0f,
                        1100.0f,
                        0.5f,
                        glm::vec3(1.0f, 1.0f, 1.0f));

            const char* modeNames[] = { "Shaded", "Wireframe", "Random" };
            debugText.str("");
            debugText << "Mode: " << modeNames[currentMode];
            render_text(text_shader,
                        debugText.str(),
                        10.0f,
                        1075.0f,
                        0.5f,
                        glm::vec3(1.0f, 1.0f, 1.0f));

            // Info about model on screen like number of vertices etc. 
            debugText.str("");
            debugText << "Vertices: " << totalVertices;
            render_text(text_shader,
                        debugText.str(),
                        10.0f,
                        1050.0f,
                        0.5f,
                        glm::vec3(1.0f, 1.0f, 1.0f));
            
            debugText.str("");
            debugText << "Model Name: " << scene->GetShortFilename(modelPath.c_str());
            render_text(text_shader,
                        debugText.str(),
                        10.0f,
                        1025.0f,
                        0.5f,
                        glm::vec3(1.0f, 1.0f, 1.0f));

            // Controls help
            render_text(text_shader,
                        "Controls:",
                        10.0f,
                        950.0f,
                        0.4f,
                        glm::vec3(0.8f, 0.8f, 0.8f));
            render_text(text_shader,
                        "WASD - Move",
                        10.0f,
                        920.0f,
                        0.3f,
                        glm::vec3(0.7f, 0.7f, 0.7f));
            render_text(text_shader,
                        "Space/Shift - Up/Down",
                        10.0f,
                        895.0f,
                        0.3f,
                        glm::vec3(0.7f, 0.7f, 0.7f));
            render_text(text_shader,
                        "Mouse - Look",
                        10.0f,
                        870.0f,
                        0.3f,
                        glm::vec3(0.7f, 0.7f, 0.7f));
            render_text(text_shader,
                        "Tab - Mode",
                        10.0f,
                        845.0f,
                        0.3f,
                        glm::vec3(0.7f, 0.7f, 0.7f));
            render_text(text_shader,
                        "E - Debug",
                        10.0f,
                        820.0f,
                        0.3f,
                        glm::vec3(0.7f, 0.7f, 0.7f));

            glEnable(GL_DEPTH_TEST); // Re-enable depth testing

            // Switch back to mesh shader for bounding box
            glUseProgram(mesh_shader);

            // Set up matrices for bounding box
            glUniformMatrix4fv(glGetUniformLocation(mesh_shader, "model"),
                               1,
                               GL_FALSE,
                               &model[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(mesh_shader, "view"),
                               1,
                               GL_FALSE,
                               &view[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(mesh_shader, "projection"),
                               1,
                               GL_FALSE,
                               &proj[0][0]);

            // Render bounding box
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glUniform3f(
                glGetUniformLocation(mesh_shader, "baseColor"), 1.0, 0.0, 0.0);
            glUniform1i(glGetUniformLocation(mesh_shader, "useShading"), 0);
            glUniform1i(glGetUniformLocation(mesh_shader, "useRandomColor"), 0);
            glBindVertexArray(bboxVAO);
            glDrawArrays(GL_LINES, 0, 24);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
