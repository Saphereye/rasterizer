#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum Camera_Movement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN,
};

class Camera
{
public:
    glm::vec3 Position, Front, Up, Right, WorldUp;
    float     Yaw, Pitch, Speed = 2.5f, Sensitivity = 0.1f;

    Camera(glm::vec3 pos, glm::vec3 up, float yaw, float pitch) :
            Front(glm::vec3(0.0f, 0.0f, -1.0f)), Speed(2.5f), Sensitivity(0.1f)
    {
        Position = pos;
        WorldUp  = up;
        Yaw      = yaw;
        Pitch    = pitch;
        update_vectors();
    }

    glm::mat4 get_view_matrix()
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    void process_keyboard(Camera_Movement dir, float dt)
    {
        float velocity = Speed * dt;
        switch (dir)
        {
        case FORWARD:
            Position += Front * velocity;
            break;
        case BACKWARD:
            Position -= Front * velocity;
            break;
        case LEFT:
            Position -= Right * velocity;
            break;
        case RIGHT:
            Position += Right * velocity;
            break;
        case UP:
            Position.y += Speed * velocity;
            break;
        case DOWN:
            Position.y -= Speed * velocity;
            break;
        default:
            break;
        }
    }

    void process_mouse_movement(float xoffset, float yoffset)
    {
        xoffset *= Sensitivity;
        yoffset *= Sensitivity;
        Yaw += xoffset;
        Pitch += yoffset;
        Pitch = glm::clamp(Pitch, -89.0f, 89.0f);
        update_vectors();
    }

private:
    void update_vectors()
    {
        glm::vec3 f;
        f.x   = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        f.y   = sin(glm::radians(Pitch));
        f.z   = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(f);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up    = glm::normalize(glm::cross(Right, Front));
    }
};
