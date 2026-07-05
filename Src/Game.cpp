#include "Game.h"
#include "Player.h"

#include <SDL3/SDL.h>

void MenuController::Init(GameContext* _game_context, Transform* _owner) {
        game_context = _game_context;
        owner        = _owner;

        owner->position = { 0.0f, 12'000.0f, 0.0f };

        owner->rotation = Quat{ glm::normalize(Vec3{ 0.0f, 1.0f, -0.8f }), normalize(owner->position) };
        world_up        = glm::normalize(owner->position);
}

void MenuController::Update(float delta_time) {
        Vec3 new_world_up = glm::normalize(owner->position);

        Quat world_rot{ world_up, new_world_up };
        world_up        = new_world_up;
        owner->rotation = world_rot * owner->rotation;
        movement_rot    = world_rot * movement_rot;

        owner->rotation = glm::normalize(owner->rotation);
        movement_rot    = glm::normalize(movement_rot);

        Vec3 movement = Vec3{ 0.0f, 0.0f, 1.0f } * movement_speed * delta_time;
        owner->position += rotate(movement_rot, movement);
}

void MainMenu::Start(GameContext* _game_context) {
        game_context = _game_context;

        // ===== Title Text ===== //

        title_text.Init(&game_context->ui_system, FontID::Title, 512);
        title_text.SetText(&game_context->ui_system, "The  Lonely\n  Surveyor");

        // ===== Menu Buttons ===== //

        // == Start == //

        menu_buttons[(u32)MenuButtonID::Start].Init(&game_context->ui_system, FontID::Default, 160);
        menu_buttons[(u32)MenuButtonID::Start].SetText(&game_context->ui_system, "Start");

        selected_button = MenuButtonID::Start;

        // == Quit == //

        menu_buttons[(u32)MenuButtonID::Quit].Init(&game_context->ui_system, FontID::Default, 160);
        menu_buttons[(u32)MenuButtonID::Quit].SetText(&game_context->ui_system, "Go back to earth");

        // ===== Actions ===== //

        menu_up_action = game_context->input_system.RegisterAction("MenuUp", ActionType::Scalar);
        menu_up_action->AddKey(KeyCode::Up);
        menu_up_action->AddKey(KeyCode::W);

        menu_down_action = game_context->input_system.RegisterAction("MenuDown", ActionType::Scalar);
        menu_down_action->AddKey(KeyCode::Down);
        menu_down_action->AddKey(KeyCode::S);

        menu_accept_action = game_context->input_system.RegisterAction("MenuSelect", ActionType::Scalar);
        menu_accept_action->AddKey(KeyCode::Space);
        menu_accept_action->AddKey(KeyCode::Enter);

        exit_action = game_context->input_system.RegisterAction("Quit", ActionType::Scalar);
        exit_action->AddKey(KeyCode::Escape);

        // ===== Camera ===== //

        camera_controller.Init(game_context, &camera.game_object);

        // ===== Sounds ===== //

        ui_sound = game_context->sound_system.LoadSound("Assets/Sounds/Click.wav");

        logo_image = game_context->model_system.CreateModelInstance(game_context->ui_system.image_model_id);

        logo_image.colour0 = { 1.0f, 1.0f, 1.0f, 0.0f };

        logo_background                    = game_context->model_system.CreateModelInstance(game_context->ui_system.ui_model_id);
        logo_background.transform.position = {
                -2.0f,
                -2.0f,
                0.001f,
        };

        logo_background.transform.scale = {
                4.0f,
                4.0f,
                1.0f,
        };

        logo_background.colour0 = {
                0.0f,
                0.0f,
                0.0f,
                1.0f,
        };

        logo_background.colour1 = {
                0.0f,
                0.0f,
                0.0f,
                1.0f,
        };

        time_active = 0;

        presents_text.Init(&game_context->ui_system, FontID::Default, 160);
        presents_text.SetText(&game_context->ui_system, "PRESENTS");
        presents_text.SetColour(&game_context->ui_system, Vec4{ 0.8f, 0.8f, 0.8f, 0.0f });
}

bool MainMenu::Update(float delta_time) {
        // First 5 seconds is the logo

        if (time_active < 25.0f) {
                time_active += delta_time;

                float aspect = (float)game_context->window.width / (float)game_context->window.height;

                if (game_context->window.width < game_context->window.height) {
                        logo_image.transform.position = {
                                -1.0f,
                                -0.6f + (1.0f - aspect),
                                0.0f,
                        };

                        logo_image.transform.scale = {
                                2.0f,
                                1.2f * aspect,
                                1.0f,
                        };
                } else {
                        float aspect = (float)game_context->window.height / (float)game_context->window.width;

                        logo_image.transform.position = {
                                -1.0f + (1.0f - aspect),
                                -0.6f,
                                0.0f,
                        };

                        logo_image.transform.scale = {
                                2.0f * aspect,
                                1.2f,
                                1.0f,
                        };
                }

                if (time_active > 1.0f and time_active < 6.0f) {
                        logo_image.colour0 = { 1.0f, 1.0f, 1.0f, glm::pow(((time_active - 1.0f) / 5.0f), 2.0f) };
                }

                if (time_active > 10.0f and time_active < 15.0f) {
                        logo_image.colour0 = { 1.0f, 1.0f, 1.0f, 1.0f - pow(((time_active - 10.0f) / 5.0f), 2.0f) };
                }

                if (time_active > 20.0f) {
                        logo_background.colour0 = { 0.0f, 0.0f, 0.0f, 1.0f - pow((time_active - 20.0f) / 5.0f, 4) };

                        logo_background.colour1 = { 0.0f, 0.0f, 0.0f, 1.0f - pow((time_active - 20.0f) / 5.0f, 4) };
                }

                if (time_active > 15.0f and time_active < 20.0f) {
                        presents_text.transform.position = {
                                -(presents_text.width / game_context->window.width) / 2.0f,
                                -0.2,
                                0.0f,
                        };

                        presents_text.SetColour(&game_context->ui_system, Vec4{ 0.8f, 0.8f, 0.8f, std::clamp((time_active - 15.0f) / 1.5f, 0.0f, 1.0f) });
                        if (time_active > 17.0f) {
                                presents_text.SetColour(&game_context->ui_system,
                                                        Vec4{ 0.8f, 0.8f, 0.8f, 1.0f - std::clamp((time_active - 17.0f) / 3.0f, 0.0f, 1.0f) });
                        }
                        presents_text.Draw(&game_context->ui_system);
                }

                game_context->model_system.Draw(logo_background);

                if (time_active < 15.0f) game_context->model_system.Draw(logo_image);
        }

        // ===== Update UI ===== //

        constexpr Vec4 selected_button_colour   = { 1.0f, 1.0f, 1.0f, 1.0f };
        constexpr Vec4 unselected_button_colour = { 0.1f, 0.1f, 0.1f, 1.0f };

        for (u32 i = 0; i < (u32)MenuButtonID::Count; i++) {
                if ((MenuButtonID)i == selected_button) {
                        menu_buttons[i].SetColour(&game_context->ui_system, selected_button_colour);
                } else {
                        menu_buttons[i].SetColour(&game_context->ui_system, unselected_button_colour);
                }

                menu_buttons[i].transform.position = {
                        -(menu_buttons[i].width / game_context->window.width) / 2.0f,
                        0.25f + ((float(i) * (menu_buttons[i].height * 0.75f / game_context->window.height)) * 2.0f),
                        0.01f,
                };

                menu_buttons[i].Draw(&game_context->ui_system);
        }

        // == Set Positions == //

        title_text.transform.position = {
                -(title_text.width / game_context->window.width) / 2.0f,
                -0.9,
                0.01f,
        };

        title_text.Draw(&game_context->ui_system);

        // ===== Actions ===== //

        if (menu_up_action->IsPressed()) {
                selected_button = (MenuButtonID)(((u32)selected_button + ((u32)MenuButtonID::Count - 1)) % (u32)MenuButtonID::Count);
                game_context->sound_system.PlaySound(ui_sound);
        }

        if (menu_down_action->IsPressed()) {
                selected_button = (MenuButtonID)(((u32)selected_button + 1) % (u32)MenuButtonID::Count);
                game_context->sound_system.PlaySound(ui_sound);
        }

        if (exit_action->IsPressed()) {
                exit(0);
        }

        if (menu_accept_action->IsPressed()) {
                switch (selected_button) {
                        case MenuButtonID::Start:
                                return false;
                                break;
                        case MenuButtonID::Quit:
                                exit(0);
                }
        }

        // ===== Camera ===== //

        camera.Update(*game_context);
        camera_controller.Update(delta_time);

        return true;
}

void Game::Init() {
        game_context.Init();

        mouse_focus_action = game_context.input_system.RegisterAction("MouseFocus", ActionType::Scalar);
        mouse_focus_action->AddKey(KeyCode::F1);


        camera_controller.Init(game_context, camera.game_object);

        // ===== Planet ===== //

        planet.Init(&game_context, &camera.game_object);

        // ===== Mesh Data =====

        // Skysphere
        {
                sky_sphere_model = game_context.model_system.LoadModel("Assets/Models/SkySphere.obj", 1);

                skymap.Load(game_context.vulkan_device, game_context.graphics_command_pool, game_context.graphics_queue, "Assets/SkyMaps/SpaceLDR.ktx",
                            game_context.vulkan_allocator);
                game_context.AddTexture(skymap, 0);

                Material sky_map_material{
                        .type   = MaterialType::SkyMap,
                        .skymap = { .texture_id = 0 },
                };

                game_context.model_system[sky_sphere_model].material = sky_map_material;

                sky_sphere                 = game_context.model_system.CreateModelInstance(sky_sphere_model);
                sky_sphere.transform.scale = { 10.0f, 10.0f, 10.0f };
        }

        // test mesh
        {
                box_model = game_context.model_system.LoadModel("Assets/Models/test.obj", 1);

                Material material{
                        .type = MaterialType::Basic,
                };

                game_context.model_system[box_model].material = material;

                test_box                 = game_context.model_system.CreateModelInstance(box_model);
                test_box.transform.scale = { 1.0f, 1.0f, 1.0f };
        }
}

void Game::Shutdown() {
        vkDeviceWaitIdle(game_context.vulkan_device);

        // game_context.models[1].Destroy(game_context);
        // game_context.models[2].Destroy(game_context);

        skymap.Destroy(&game_context);

        planet.Shutdown();
        camera_controller.Shutdown();

        game_context.Shutdown();
}

void Game::Update(float delta_time) {
        game_context.transfer_engine.Update();
        planet.Update();
}

void Game::Run() {
        u64  last_time{ SDL_GetTicks() };
        bool running = true;

        u64   frame_count = 0;
        float frame_time  = 0;

        MainMenu main_menu;
        main_menu.Start(&game_context);

        Sound music = game_context.sound_system.LoadSound("Assets/Sounds/Space.mp3", true);
        game_context.sound_system.PlayMusic(music);

        bool main_menu_active = true;

        PlayerController player_controller;
        player_controller.Init(&game_context, &planet, &camera.game_object);

        while (running) {
                // ===== Delta Time ===== //

                float delta_time = float(SDL_GetTicks() - last_time) / 1000.0f;

                frame_time += delta_time;
                frame_count++;
                if (frame_time >= 1.0f) {
                        std::println("FPS: {}", frame_count);
                        frame_count = 0;
                        frame_time -= 1.0f;
                }

                if (delta_time > 1.0f / 30.0f) delta_time = 1.0f / 30.0f;
                last_time = SDL_GetTicks();

                // ===== Input ===== //

                running = game_context.input_system.Update();

                if (mouse_focus_action->IsPressed())
                        SDL_SetWindowRelativeMouseMode(game_context.window.window, !SDL_GetWindowRelativeMouseMode(game_context.window.window));

                // ===== Gameplay Update ===== //

                Update(delta_time);

                if (main_menu_active) {
                        camera.game_object            = main_menu.camera.game_object;
                        main_menu_active              = main_menu.Update(delta_time);
                        sky_sphere.transform.position = main_menu.camera.game_object.position;
                } else {
                        // camera_controller.Update(delta_time);
                        player_controller.Update(delta_time);
                        sky_sphere.transform.position = camera.game_object.position;
                }
                camera.Update(game_context);



                for (u32 i = 0; i < planet.chunks_to_render.size(); i++) {
                        ModelInstance instance{
                                .model_id   = planet.chunks[planet.chunks_to_render[i]].model_id,
                                .transform  = {},
                                .user_value = {},
                        };

                        game_context.model_system.Draw(instance);
                }

                game_context.model_system.Draw(sky_sphere);


                // ===== Render ===== //

                game_context.Render(camera);
                game_context.sound_system.Update();
        }
}
