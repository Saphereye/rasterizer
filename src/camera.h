#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

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
    float     Yaw, Pitch, BaseSpeed = 5.0f, Sensitivity = 0.1f;
    float     SceneScale = 1.0f; // New: scene scale factor
    float     SpeedMultiplier = 1.0f; // New: dynamic speed multiplier
    glm::vec3 SceneCenter = glm::vec3(0.0f); // New: scene center for distance calculation
    
    Camera(glm::vec3 pos, glm::vec3 up, float yaw, float pitch) :
            Front(glm::vec3(0.0f, 0.0f, -1.0f)), BaseSpeed(2.5f), Sensitivity(0.1f)
    {
        Position = pos;
        WorldUp  = up;
        Yaw      = yaw;
        Pitch    = pitch;
        update_vectors();
    }
    
    // New: Set scene parameters for adaptive speed
    void set_scene_params(const glm::vec3& center, float maxExtent)
    {
        SceneCenter = center;
        SceneScale = maxExtent;
        update_speed_multiplier();
    }
    
    glm::mat4 get_view_matrix()
    {
        return glm::lookAt(Position, Position + Front, Up);
    }
    
    void process_keyboard(Camera_Movement dir, float dt)
    {
        update_speed_multiplier(); // Update speed based on current position
        float velocity = BaseSpeed * SpeedMultiplier * dt * 2.0f;
        
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
            Position += WorldUp * velocity; // Use WorldUp instead of just y
            break;
        case DOWN:
            Position -= WorldUp * velocity;
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
    
    // New: Get current movement speed for debugging
    float get_current_speed() const
    {
        return BaseSpeed * SpeedMultiplier;
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
    
    // New: Update speed multiplier based on distance from scene center
    void update_speed_multiplier()
    {
        if (SceneScale <= 0.0f) return; // Avoid division by zero
        
        // Calculate distance from camera to scene center
        float distanceToCenter = glm::length(Position - SceneCenter);
        
        // Base the speed on both scene scale and distance from center
        // Minimum speed multiplier based on scene scale
        float scaleBasedMultiplier = SceneScale / 10.0f; // Adjust divisor as needed
        
        // Distance-based multiplier (speed increases with distance)
        float distanceBasedMultiplier = std::max(1.0f, distanceToCenter / SceneScale);
        
        // Combine both factors with reasonable bounds
        SpeedMultiplier = scaleBasedMultiplier * distanceBasedMultiplier;
        
        // Clamp to reasonable bounds
        SpeedMultiplier = std::clamp(SpeedMultiplier, 0.1f, 1000.0f);
    }
};
