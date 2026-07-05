#include "Game.h"
#include "Player.h"

#include <SDL3/SDL.h>

void MenuController::Init(GameContext* _game_context, Transform* _owner) {
        game_context = _game_context;
        owner        = _owner;

        owner->position = { 0.0f, 12'000.0f, 0.0f };

        owner->rotation = Quat{ glm::normalize(Vec3{ 0.0f, 1.0f, -0.8f }), normalize(owner->position) };
        world_up        = Vec3{ 0.0f, 1.0f, 0.0f };
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
        title_text.SetText(&game_context->ui_system, "The Lonely\n Surveyor");

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

        name_text.Init(&game_context->ui_system, FontID::Default, 460);
        name_text.SetText(&game_context->ui_system, "Benas Montrimas");
        name_text.SetColour(&game_context->ui_system, Vec4{ 0.8f, 0.8f, 0.8f, 0.0f });

        camera.Init(game_context);
}

void MainMenu::DrawIntro(float delta_time) {
        if (time_active < 25.0f) {
                time_active += delta_time;

                float aspect = (float)game_context->window.width / (float)game_context->window.height;

                if (game_context->window.width < game_context->window.height) {
                        logo_image.transform.position = {
                                -1.0f,
                                -0.5f + (1.0f - aspect),
                                0.0f,
                        };

                        logo_image.transform.scale = {
                                2.0f,
                                1.0f * aspect,
                                1.0f,
                        };
                } else {
                        float aspect = (float)game_context->window.height / (float)game_context->window.width;

                        logo_image.transform.position = {
                                -1.0f + (1.0f - aspect),
                                -0.5f,
                                0.0f,
                        };

                        logo_image.transform.scale = {
                                2.0f * aspect,
                                1.0f,
                                1.0f,
                        };
                }

                // == Set Name Pos == //

                name_text.text_size = 160 * aspect;
                name_text.SetText(&game_context->ui_system, "Benas Montrimas");

                name_text.transform.position = {
                        -(name_text.width / game_context->window.width) / 2.0f,
                        -(name_text.height / game_context->window.height),
                        0.0f,
                };

                if (time_active > 1.0f and time_active < 4.0f) {
                        logo_image.colour0 = { 1.0f, 1.0f, 1.0f, glm::pow(((time_active - 1.0f) / 4.0f), 2.0f) };
                }

                if (time_active > 7.0f and time_active < 11.0f) {
                        logo_image.colour0 = { 1.0f, 1.0f, 1.0f, 1.0f - pow(((time_active - 10.0f) / 4.0f), 2.0f) };
                }

                if (time_active > 11.0f and time_active < 14.0f) {
                        name_text.SetColour(&game_context->ui_system, Vec4{ 0.8f, 0.8f, 0.8f, pow((time_active - 11.0f) / 2.0f, 2.0f) });
                }

                if (time_active > 17.0f and time_active < 20.0f) {
                        name_text.SetColour(&game_context->ui_system, Vec4{ 0.8f, 0.8f, 0.8f, 1.0f - pow((time_active - 17.0f) / 3.0f, 2.0f) });
                }

                if (time_active > 11.0f and time_active < 20.0f) name_text.Draw(&game_context->ui_system, true);


                if (time_active > 13.0f and time_active < 20.0f) {
                        presents_text.transform.position = {
                                -(presents_text.width / game_context->window.width) / 2.0f,
                                (presents_text.height / game_context->window.height),
                                0.0f,
                        };

                        presents_text.SetColour(&game_context->ui_system, Vec4{ 0.8f, 0.8f, 0.8f, std::clamp((time_active - 13.0f) / 1.0f, 0.0f, 0.5f) });
                        if (time_active > 17.0f) {
                                presents_text.SetColour(&game_context->ui_system,
                                                        Vec4{ 0.8f, 0.8f, 0.8f, 1.0f - std::clamp((time_active - 17.0f) / 3.0f, 0.0f, 1.0f) });
                        }
                        presents_text.Draw(&game_context->ui_system, true);
                }

                if (time_active > 20.0f) {
                        logo_background.colour0 = { 0.0f, 0.0f, 0.0f, 1.0f - pow((time_active - 20.0f) / 5.0f, 4) };

                        logo_background.colour1 = { 0.0f, 0.0f, 0.0f, 1.0f - pow((time_active - 20.0f) / 5.0f, 4) };
                }

                game_context->model_system.Draw(logo_background);

                if (time_active < 11.0f) game_context->model_system.Draw(logo_image);
        }
}

bool MainMenu::Update(float delta_time) {
        DrawIntro(delta_time);
        // if (time_active < 20.0f) return true;

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

        camera.Update();
        camera_controller.Update(delta_time);

        return true;
}

void Rock::Update(Planet* planet, const Transform& transform) {
        float distance_from_player_squared = glm::length2(model.transform.position - transform.position);
        if (distance_from_player_squared > 40000000.0f) {
                model.transform.position = transform.position - glm::ballRand(2000.0f);
                Ray r{
                        .origin    = Vec3{ 0.0f },
                        .direction = glm::normalize(model.transform.position),
                };
                float distance           = planet->CheckIntersection(r, 0.0f);
                model.transform.position = distance * r.direction;
        }

        // if (distance_from_player_squared < 5.0f) {
        //         Ray r {
        //                 .origin = Vec3{0.0f},
        //                 .direction = glm::normalize(model.transform.position)
        //         };
        //         float distance = planet->CheckIntersection(r, 0.0f);
        //         model.transform.position = distance * r.direction;
        // }
}

void Game::Init() {
        game_context.Init();

        mouse_focus_action = game_context.input_system.RegisterAction("MouseFocus", ActionType::Scalar);
        mouse_focus_action->AddKey(KeyCode::F1);

        pause_action = game_context.input_system.RegisterAction("Pause", ActionType::Scalar);
        pause_action->AddKey(KeyCode::Escape);

        menu_up_action = game_context.input_system.RegisterAction("MenuUp", ActionType::Scalar);
        menu_up_action->AddKey(KeyCode::Up);
        menu_up_action->AddKey(KeyCode::W);

        menu_down_action = game_context.input_system.RegisterAction("MenuDown", ActionType::Scalar);
        menu_down_action->AddKey(KeyCode::Down);
        menu_down_action->AddKey(KeyCode::S);

        menu_accept_action = game_context.input_system.RegisterAction("MenuSelect", ActionType::Scalar);
        menu_accept_action->AddKey(KeyCode::Space);
        menu_accept_action->AddKey(KeyCode::Enter);

        interact_action = game_context.input_system.RegisterAction("Interact", ActionType::Scalar);
        interact_action->AddKey(KeyCode::E);

        // ===== Planet ===== //

        player.Init(&game_context);
        planet.Init(&game_context, &player.transform);

        // ===== Mesh Data =====

        // Skysphere
        {
                sky_sphere_model = game_context.model_system.LoadModel("Assets/Models/SkySphere.obj", 1);

                skymap.Load(game_context.vulkan_device, game_context.graphics_command_pool, game_context.graphics_queue, "Assets/SkyMaps/SpacePlanet.ktx",
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

        // rock mesh
        {
                rock_model = game_context.model_system.LoadModel("Assets/Models/rock.obj", 100'000);

                rock_texture.Load(game_context.vulkan_device, game_context.graphics_command_pool, game_context.graphics_queue, "Assets/Textures/Rock.ktx",
                                  game_context.vulkan_allocator);
                game_context.AddTexture(rock_texture, 40);

                Material material{ .type = MaterialType::Basic,
                                   .base = {
                                           40,
                                   } };

                game_context.model_system[rock_model].material = material;

                rocks = (Rock*)malloc(sizeof(Rock) * 1'000);
                for (u32 i = 0; i < 1'000; i++) {
                        rocks[i].model                    = game_context.model_system.CreateModelInstance(rock_model);
                        rocks[i].model.transform.scale    = 10.0f + glm::ballRand(5.0f);
                        rocks[i].model.transform.rotation = Quat(glm::ballRand(5.0f));
                }
        }

        // ===== Pause Menu ===== //

        pause_text.Init(&game_context.ui_system, FontID::Title, 200);
        pause_text.SetText(&game_context.ui_system, "Pause");

        // ===== Menu Buttons ===== //

        // == Start == //

        pause_buttons[(u32)PauseButtonID::Resume].Init(&game_context.ui_system, FontID::Default, 100);
        pause_buttons[(u32)PauseButtonID::Resume].SetText(&game_context.ui_system, "Resume");

        selected_button = PauseButtonID::Resume;

        // == Quit == //

        pause_buttons[(u32)PauseButtonID::Quit].Init(&game_context.ui_system, FontID::Default, 100);
        pause_buttons[(u32)PauseButtonID::Quit].SetText(&game_context.ui_system, "Main Menu");

        rock_pickup_sound = game_context.sound_system.LoadSound("Assets/Sounds/Rock.wav");

        pick_up_text.Init(&game_context.ui_system, FontID::Default, 80);
        pick_up_text.SetText(&game_context.ui_system, "Press E to pick up");


        picked_up_text.Init(&game_context.ui_system, FontID::Default, 100);
        picked_up_text.transform.position = {
                -0.9f, -0.9f, 0.0f
        };

        pause_background.transform.position = {
                -0.5f,
                -0.5f,
                0.001f,
        };
        pause_background.transform.scale = {
                1,
                1,
                1,
        };

        pause_background.colour0 = {
                0.0f,
                0.0f,
                0.0f,
                1.0f,
        };

        pause_background.colour1 = {
                0.0f,
                0.0f,
                0.0f,
                1.0f,
        };
}

void Game::Shutdown() {
        vkDeviceWaitIdle(game_context.vulkan_device);

        // game_context.models[1].Destroy(game_context);
        // game_context.models[2].Destroy(game_context);

        skymap.Destroy(&game_context);

        planet.Shutdown();

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

        sky_sphere.transform.rotation = glm::angleAxis(glm::radians(10.0f), glm::normalize(glm::vec3(1.f, 1.f, 0.f)));

        while (running) {
                // ===== Delta Time ===== //

                float delta_time = float(SDL_GetTicks() - last_time) / 1000.0f;
                float real_time  = delta_time;

                frame_time += delta_time;
                frame_count++;
                if (frame_time >= 1.0f) {
                        std::println("FPS: {}", frame_count);
                        frame_count = 0;
                        frame_time -= 1.0f;
                }

                if (delta_time > 1.0f / 30.0f) delta_time = 1.0f / 30.0f;
                last_time = SDL_GetTicks();

                if (pause_action->IsPressed()) {
                        // delta_time = 0.0f;
                        is_paused       = !is_paused;
                        selected_button = PauseButtonID::Resume;
                }

                if (is_paused and !main_menu_active) {
                        delta_time = 0.0f;

                        if (menu_up_action->IsPressed()) {
                                selected_button = (PauseButtonID)(((u32)selected_button + (u32)PauseButtonID::Count - 1) % (u32)PauseButtonID::Count);
                        }

                        if (menu_down_action->IsPressed()) {
                                selected_button = (PauseButtonID)(((u32)selected_button + 1) % (u32)PauseButtonID::Count);
                        }

                        if (menu_accept_action->IsPressed()) {
                                switch (selected_button) {
                                        case PauseButtonID::Resume:
                                                is_paused = false;
                                                break;
                                        case PauseButtonID::Quit:
                                                main_menu_active = true;
                                                break;
                                }
                        }


                        constexpr Vec4 selected_button_colour   = { 1.0f, 1.0f, 1.0f, 1.0f };
                        constexpr Vec4 unselected_button_colour = { 0.1f, 0.1f, 0.1f, 1.0f };

                        game_context.model_system.Draw(pause_background);

                        pause_text.transform.position = {
                                -(pause_text.width / game_context.window.width) / 2.0f,
                                -(pause_text.height / game_context.window.height) * 2,
                                0.0f,
                        };

                        pause_text.Draw(&game_context.ui_system);

                        for (u32 i = 0; i < (u32)PauseButtonID::Count; i++) {
                                // Set posiiton

                                if ((PauseButtonID)i == selected_button) {
                                        pause_buttons[i].SetColour(&game_context.ui_system, selected_button_colour);
                                } else {
                                        pause_buttons[i].SetColour(&game_context.ui_system, unselected_button_colour);
                                }

                                pause_buttons[i].transform.position = {
                                        -(pause_buttons[i].width / game_context.window.width) / 2.0f,
                                        (pause_buttons[i].height / game_context.window.height) * i * 2,
                                        0.0f,
                                };

                                pause_buttons[i].Draw(&game_context.ui_system);
                        }
                }

                // ===== Input ===== //

                running = game_context.input_system.Update();

                if (mouse_focus_action->IsPressed())
                        SDL_SetWindowRelativeMouseMode(game_context.window.window, !SDL_GetWindowRelativeMouseMode(game_context.window.window));

                // ===== Gameplay Update ===== //

                Update(delta_time);

                if (main_menu_active) {
                        delta_time                    = 0.0f;
                        camera                        = &main_menu.camera;
                        main_menu_active              = main_menu.Update(real_time);
                        sky_sphere.transform.position = main_menu.camera.game_object.position;

                        if (!main_menu_active) {
                                player.Start(&planet);
                                camera    = &player.camera;
                                is_paused = false;
                        }
                } else {
                        sky_sphere.transform.position = player.transform.position;
                        player.Update(delta_time);


                        for (u32 i = 0; i < 1'000; i++) {
                                rocks[i].Update(&planet, player.transform);
                                game_context.model_system.Draw(rocks[i].model);

                                if (glm::length(player.transform.position - rocks[i].model.transform.position) < 30.0f) {
                                        pick_up_text.transform.position = {
                                                -(pick_up_text.width / game_context.window.width) / 2.0f,
                                                0.2f,
                                                0.0f,
                                        };

                                        pick_up_text.Draw(&game_context.ui_system);

                                        if (interact_action->IsPressed()) {
                                                rocks[i].model.transform.position = {};
                                                game_context.sound_system.PlaySound(rock_pickup_sound);

                                                rocks_collected++;

                                                show_pick_ups_time = 2.0f;

                                                std::string pickup_text = "Rocks Collected: " + std::to_string(rocks_collected);
                                                picked_up_text.SetText(&game_context.ui_system, pickup_text);
                                        }
                                }
                        }
                }

                if (show_pick_ups_time > 0.0f) {
                        picked_up_text.Draw(&game_context.ui_system);
                        show_pick_ups_time -= real_time;
                }

                for (u32 i = 0; i < planet.chunks_to_render.size(); i++) {
                        ModelInstance instance{
                                .model_id   = planet.chunks[planet.chunks_to_render[i]].model_id,
                                .transform  = {},
                                .user_value = {},
                        };

                        game_context.model_system.Draw(instance);
                }

                static float a = 0.0f;
                a += delta_time * 10.0f;
                sky_sphere.transform.rotation =
                        (glm::angleAxis(glm::radians(delta_time * 0.3f), glm::normalize(glm::vec3(1.f, 1.f, 0.f))) * sky_sphere.transform.rotation);
                // sky_sphere.transform.rotation = rotate(sky_sphere.transform.rotation, Vec3{delta_time, 0.0f, 0.0f});

                game_context.model_system.Draw(sky_sphere);

                assert(camera and "Camera must be set");
                game_context.Render(*camera);
                // ===== Render ===== //
                game_context.sound_system.Update();
        }
}
