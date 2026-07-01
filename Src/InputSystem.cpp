#include "InputSystem.h"

#include "Game.h"

#include <SDL3/SDL.h>

#include <print>

void InputSystem::Init(Window* _window) {
        window = _window;
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "1");
}

void InputSystem::Shutdown() {
}

bool InputSystem::Update() {
        // Update all key states

        for (u32 i = 0; i < (u32)KeyCode::Count; i++) {
                Key& key = keys[i];

                switch (key.state) {
                        case KeyState::Pressed:
                                key.state = KeyState::Held;
                                break;
                        case KeyState::Released:
                                key.state = KeyState::None;
                                break;
                }
        }

        for (u32 i = 0; i < (u32)MouseButtonCode::Count; i++) {
                MouseButton& button = mouse.buttons[i];

                switch (button.state) {
                        case ButtonState::Pressed:
                                button.state = ButtonState::Held;
                                break;
                        case ButtonState::Released:
                                button.state = ButtonState::None;
                                break;
                }
        }

        mouse.delta       = { 0, 0 };
        mouse.wheel_delta = { 0, 0 };

        // Poll new events
        for (SDL_Event event; SDL_PollEvent(&event);) {

                // ===== EVENT SWITCH =====
                switch (event.type) {
                        // ===== SYSTEM ===== //
                        case SDL_EVENT_QUIT:
                                return false;

                        // ===== MOUSE ===== //
                        case SDL_EVENT_MOUSE_MOTION:
                                {
                                        mouse.position.x = event.motion.x;
                                        mouse.position.y = event.motion.y;


                                        mouse.delta.x += event.motion.xrel;
                                        mouse.delta.y += event.motion.yrel;
                                }
                                break;
                        case SDL_EVENT_MOUSE_WHEEL:
                                {
                                        mouse.wheel_delta.x = event.wheel.x;
                                        mouse.wheel_delta.y = event.wheel.y;
                                }
                                break;
                        case SDL_EVENT_MOUSE_BUTTON_DOWN:
                                {
                                        u32 button_index                  = event.button.button;
                                        mouse.buttons[button_index].state = ButtonState::Pressed;
                                }
                                break;
                        case SDL_EVENT_MOUSE_BUTTON_UP:
                                {
                                        u32 button_index                  = event.button.button;
                                        mouse.buttons[button_index].state = ButtonState::Released;
                                }
                                break;

                        // ===== KEYBOARD ===== //
                        case SDL_EVENT_KEY_DOWN:
                                {
                                        u32 scan_code = (u32)event.key.scancode;

                                        // Some keys are not handled.
                                        if (scan_code >= (u32)KeyCode::Count) continue;

                                        keys[scan_code].state = KeyState::Pressed;
                                }
                                break;
                        case SDL_EVENT_KEY_UP:
                                {
                                        u32 scan_code = (u32)event.key.scancode;

                                        // Some keys are not handled.
                                        if (scan_code >= (u32)KeyCode::Count) continue;

                                        keys[scan_code].state = KeyState::Released;
                                }
                                break;

                        // ===== WINDOW ===== //
                        case SDL_EVENT_WINDOW_RESIZED:
                                {
                                        // How do we give this information back?
                                        window->width  = event.window.data1;
                                        window->height = event.window.data2;
                                }
                                break;
                }
                // ===== END SWITCH ===== //


                if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                        window->is_resized = true;
                }
        }

        // Update Actions based on new values

        for (u32 i = 0; i < max_input_actions; i++) {
                KeyState state  = KeyState::None;
                Action&  action = actions[i];

                action.value = {};

                for (u32 key_i = 0; key_i < action.inputs.size(); key_i++) {
                        const InputAction& input_action = action.inputs[key_i];

                        switch (input_action.type) {
                                // ===== Keyboard ===== //
                                case InputType::Keyboard:
                                        {
                                                const Key& key = keys[(u32)input_action.GetKeyCode()];
                                                if (key.state > KeyState::Released) action.value.scalar = 1.0f;
                                                // else action.value.scalar = 0.0f;
                                        }
                                        break;
                                // ===== Mouse ===== //
                                case InputType::Mouse:
                                        {
                                                const MouseButton& button = mouse.buttons[(u32)input_action.GetButtonCode()];
                                                if (button.state > ButtonState::Released) action.value.scalar = 1.0f;
                                                // else action.value.scalar = 0.0f;
                                        }
                                        break;
                                // ===== Default ===== //
                                default:
                                        std::println("Warning: Unhandled Input Type");
                        }
                }
        }

        return true;
}

Action* InputSystem::RegisterAction(const std::string& name, ActionType type) {
        assert(action_count < max_input_actions);

        actions[action_count] = Action{};

        actions[action_count].name  = name;
        actions[action_count].value = {};
        actions[action_count].type  = type;

        return &actions[action_count++];
}

Action* InputSystem::GetAction(const std::string& name) {
        for (u32 i = 0; i < action_count; i++) {
                if (actions[i].name == name) return &actions[i];
        }

        return nullptr;
}