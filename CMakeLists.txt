cmake_minimum_required(VERSION 3.10)
project(Rasterizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch dependencies
include(FetchContent)

# GLAD
FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG v0.1.36
)
FetchContent_MakeAvailable(glad)

# GLFW
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# Add Assimp
FetchContent_Declare(
  assimp
  GIT_REPOSITORY https://github.com/assimp/assimp.git
  GIT_TAG v5.2.5
)
FetchContent_MakeAvailable(assimp)

# FreeType
FetchContent_Declare(
  freetype
  GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
  GIT_TAG VER-2-12-1
)
FetchContent_MakeAvailable(freetype)

# Add executable
add_executable(Rasterizer
  src/main.cpp
  src/camera.h
)

# Link FreeGLUT
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
target_include_directories(Rasterizer PRIVATE ${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIR})
target_link_libraries(Rasterizer ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES})

target_include_directories(Rasterizer PRIVATE ${glad_SOURCE_DIR}/include)
target_link_libraries(Rasterizer glad glfw glm assimp freetype)
