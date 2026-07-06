#pragma once

#include "Base.h"
#include "Camera.h"
#include "InputSystem.h"
#include "Model.h"

struct GameContext;
struct Planet;

// Should player own this? Component? Seperate completely?
struct PlayerController {
        void Init(GameContext* _game_context, Planet* _planet, Transform* _owner);
        void Update(float delta_time);

        GameContext* game_context;
        Planet*      planet;
        Transform*   owner;

        Action* forward_action;
        Action* backward_action;
        Action* left_action;
        Action* right_action;
        Action* jump_action;

        float movement_speed{ 100.0f };
        float mouse_sensitivity{ 0.001f }; // Probably want as part of input

        Quat world_rotation;
        Vec3 world_up;
        Vec3 velocity;

        bool is_grounded;
};

struct Player {
        void Init(GameContext* _game_context);
        void Start(Planet* planet);
        void Update(float delta_time);

        GameContext* game_context;

        Camera           camera;
        Transform        transform;
        PlayerController controller;
};