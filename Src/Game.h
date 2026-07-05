#pragma once

#include "Camera.h"
#include "GameContext.h"
#include "InputSystem.h"
#include "Model.h"
#include "Planet.h"
#include "Resources.h"
#include "SoundSystem.h"
#include "UISystem.h"

enum class MenuButtonID {
        Start,
        Quit,

        Count,
};

struct MenuController {
        void Init(GameContext* _game_context, Transform* _owner);
        void Update(float delta_time);
        void Shutdown();

        GameContext* game_context;
        Transform*   owner;

        Quat movement_rot;

        Vec3  world_up;
        float movement_speed{ 200.0f };
};

struct MainMenu {
        void Start(GameContext* _game_context);
        bool Update(float delta_time);
        void Stop();

        GameContext* game_context;

        RenderedText title_text;

        RenderedText menu_buttons[(u32)MenuButtonID::Count];
        MenuButtonID selected_button;

        ModelInstance logo_image;
        ModelInstance logo_background;
        RenderedText  presents_text;

        Action* menu_up_action;
        Action* menu_down_action;
        Action* menu_accept_action;
        Action* exit_action;

        Sound ui_sound;

        float time_active{};

        Camera         camera;
        MenuController camera_controller;
};

struct Game {
        void Init();
        void Shutdown();
        void Update(float delta_time);
        void Run();

        GameContext game_context;

        Planet planet;

        ModelID sky_sphere_model;
        ModelID box_model;

        Texture skymap;

        // Model Instances
        ModelInstance sky_sphere;
        ModelInstance test_box;

        // Actions
        Action* mouse_focus_action;

        Camera            camera;
        DefaultController camera_controller;
};