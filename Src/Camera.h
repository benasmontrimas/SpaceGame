#pragma once

#include "Base.h"
#include "InputSystem.h"

struct GameObject {
        Vec3 position {0, 0, -10};
        Vec3 rotation;
        Vec3 scale;

        Vec3 forward_direction;
        Vec3 up_direction;
        Vec3 right_direction; // Can work this out
};

struct Camera {
        GameObject game_object;

        float fov {60};
        float aspect_ratio{1.0f};
        float near {0.1};
        float far {1000};

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

        float movement_speed {10};

        GameContext* game_context;
        GameObject* owner;

        void Init(GameContext& _game_context, GameObject& _owner);
        void Shutdown();
        void Update(float delta_time);
};