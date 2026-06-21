#pragma once

#include "Base.h"
#include "InputSystem.h"

// I think we want to make everything attach to game objects.
// If we want something to have a mesh we have to pass the owning game object.
// That was it can use its transform for rendering, and it will sync with other systems.

// Then just store an array of children so that we can have objects relative to other objects
struct GameObject {
        Vec3 position{ 0, 0, -10 };
        Vec3 rotation;
        Vec3 scale;

        Vec3 forward_direction;
        Vec3 up_direction;
        Vec3 right_direction; // Can work this out
};

struct Camera {
        GameObject game_object;

        float fov{ 60.0f };
        float aspect_ratio{ 1.0f };
        float near{ 0.1f };
        float far{ 5'000.0f };

        void Update(GameContext& game_context);

        Mat4 GetViewMatrix() const;
        Mat4 GetProjectionMatrix() const;
};

struct GameContext;

// Controller to just move the camera around.
// What do we control?
struct DefaultController {
        Action* forward_action;
        Action* backward_action;
        Action* left_action;
        Action* right_action;
        Action* up_action;
        Action* down_action;

        float movement_speed{ 10 };

        GameContext* game_context;
        GameObject*  owner;

        void Init(GameContext& _game_context, GameObject& _owner);
        void Shutdown();
        void Update(float delta_time);
};
