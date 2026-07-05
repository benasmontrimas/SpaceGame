#pragma once

#include "Base.h"
#include "InputSystem.h"

struct GameContext;

struct Camera {
        void Init(GameContext* _game_context);
        void Update();

        Mat4 GetViewMatrix() const;
        Mat4 GetProjectionMatrix() const;

        GameContext* game_context;
        Transform    game_object{ .position = { 10, 10'000, 0 } };

        float fov{ 60.0f };
        float aspect_ratio{ 1.0f };
        float near{ 0.1f };
        float far{ 100'000.0f };
};

struct DefaultController {
        Action* forward_action;
        Action* backward_action;
        Action* left_action;
        Action* right_action;
        Action* up_action;
        Action* down_action;

        float movement_speed{ 500.0f };
        float mouse_sensitivity{ 0.001f }; // Probably want as part of input

        GameContext* game_context;
        Transform*   owner;

        Vec3 world_up;

        void Init(GameContext& _game_context, Transform& _owner);
        void Shutdown();
        void Update(float delta_time);
};
