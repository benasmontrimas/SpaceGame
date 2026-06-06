#include "InputSystem.h"

#include "Game.h"

#include <SDL3/SDL.h>

#include <iostream>

void InputSystem::Init() {
}

void InputSystem::Shutdown() {
}

bool InputSystem::Update() {
        // Update all key states

        // Poll new events
        for (SDL_Event event; SDL_PollEvent(&event);) {
                if (event.type == SDL_EVENT_QUIT) {
                        return false;
                        break;
                }

                if (event.type == SDL_EVENT_MOUSE_MOTION) {
                        if (event.button.button == SDL_BUTTON_LEFT) {
                                // Mouse movement
                        }
                }

                if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                        // game_context.camera_position.z += (float)event.wheel.y * elapsed_time * 10.0f;
                }

                if (event.type == SDL_EVENT_KEY_DOWN) {
                        // if (event.key.key == SDLK_ESCAPE) return false;
                        u32 k = (u32)event.key.scancode;

                        if (k >= KeyCode::Count) {
                                std::println("Key: {}", k);
                                continue;
                        }

                        std::println("Key: {} : {}", (u32)event.key.scancode, KeyName[event.key.scancode]);
                        std::cout << std::flush;
                }

                if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                        // game_context.swapchain_needs_resizing = true;
                }
        }

        // Update Actions based on new values

        return true;
}