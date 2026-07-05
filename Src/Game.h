#pragma once

#include "Camera.h"
#include "GameContext.h"
#include "InputSystem.h"
#include "Model.h"
#include "Planet.h"
#include "Player.h"
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
        void DrawIntro(float delta_time);
        bool Update(float delta_time);
        void Stop();

        GameContext* game_context;

        RenderedText title_text;
        RenderedText name_text;

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

struct Rock {
        ModelInstance model;

        void Update(Planet* planet, const Transform& transform);
};

enum class PauseButtonID {
        Resume,
        Quit,

        Count,
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
        ModelID rock_model;

        Texture skymap;
        Texture rock_texture;

        // Model Instances
        ModelInstance sky_sphere;
        ModelInstance test_box;

        // Actions
        Action* mouse_focus_action;
        Action* pause_action;
        Action* menu_up_action;
        Action* menu_down_action;
        Action* menu_accept_action;
        Action* interact_action;

        Camera* camera;

        Player player;

        // ===== Pause Menu ===== //
        bool is_paused = false;

        ModelInstance pause_background;

        RenderedText pause_text;
        RenderedText pause_buttons[(u32)PauseButtonID::Count];

        PauseButtonID selected_button;

        // == //
        Rock*        rocks;
        Sound        rock_pickup_sound;
        RenderedText pick_up_text;

        float        show_pick_ups_time;
        RenderedText picked_up_text;

        u32 rocks_collected{ 0 };
};