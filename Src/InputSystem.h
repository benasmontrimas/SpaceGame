#pragma once

#include "Base.h"

#include <string>

constexpr u32 max_input_actions = 1'000;

enum class KeyCode : u32 {
        Unknown = 0,

        A = 4,
        B = 5,
        C = 6,
        D = 7,
        E = 8,
        F = 9,
        G = 10,
        H = 11,
        I = 12,
        J = 13,
        K = 14,
        L = 15,
        M = 16,
        N = 17,
        O = 18,
        P = 19,
        Q = 20,
        R = 21,
        S = 22,
        T = 23,
        U = 24,
        V = 25,
        W = 26,
        X = 27,
        Y = 28,
        Z = 29,

        _1 = 30,
        _2 = 31,
        _3 = 32,
        _4 = 33,
        _5 = 34,
        _6 = 35,
        _7 = 36,
        _8 = 37,
        _9 = 38,
        _0 = 39,

        Enter     = 40,
        Escape    = 41,
        Backspace = 42,
        Tab       = 43,
        Space     = 44,

        Minus              = 45,
        Equals             = 46,
        LeftSquareBracket  = 47,
        RightSquareBracket = 48,
        Hash               = 49,

        Semicolon  = 51,
        Apostrophe = 52,
        Tilde      = 53,

        Comma    = 54,
        FullStop = 55,
        Slash    = 56,

        CapLock = 57,

        F1  = 58,
        F2  = 59,
        F3  = 60,
        F4  = 61,
        F5  = 62,
        F6  = 63,
        F7  = 64,
        F8  = 65,
        F9  = 66,
        F10 = 67,
        F11 = 68,
        F12 = 69,

        PrintScreen = 70,
        ScrollLock  = 71,
        Paise       = 72,
        Insert      = 73,
        Home        = 74,
        PageUp      = 75,
        Delete      = 76,
        End         = 77,
        PageDown    = 78,
        Right       = 79,
        Left        = 80,
        Down        = 81,
        Up          = 82,

        NumLockClear = 83,

        KPDivide   = 84,
        KPMultiply = 85,
        KPMinus    = 86,
        KPPlus     = 87,
        KPEnter    = 88,
        KP1        = 89,
        KP2        = 90,
        KP3        = 91,
        KP4        = 92,
        KP5        = 93,
        KP6        = 94,
        KP7        = 95,
        KP8        = 96,
        KP9        = 97,
        KP0        = 98,
        KPFullStop = 99,

        Application = 101,
        Power       = 102,

        KPEquals = 103,
        F13      = 104,
        F14      = 105,
        F15      = 106,
        F16      = 107,
        F17      = 108,
        F18      = 109,
        F19      = 110,
        F20      = 111,
        F21      = 112,
        F22      = 113,
        F23      = 114,
        F24      = 115,

        KPComma = 133,

        LCTRL  = 224,
        LSHIFT = 225,
        LALT   = 226,
        LGUI   = 227,
        RCTRL  = 228,
        RSHIFT = 229,
        RALT   = 230,
        RGUI   = 231,

        Count = 232,
};

constexpr const char* KeyName[(u32)KeyCode::Count] = {
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",

        "A",
        "B",
        "C",
        "D",
        "E",
        "F",
        "G",
        "H",
        "I",
        "J",
        "K",
        "L",
        "M",
        "N",
        "O",
        "P",
        "Q",
        "R",
        "S",
        "T",
        "U",
        "V",
        "W",
        "X",
        "Y",
        "Z",

        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
        "9",
        "0",

        "Enter",
        "Escape",
        "Backspace",
        "Tab",
        "Space",

        "Minus",
        "Equals",
        "LeftSquareBracket",
        "RightSquareBracket",
        "Hash",

        "Unknown",

        "Semicolon",
        "Apostrophe",
        "Tilde",

        "Comma",
        "FullStop",
        "Slash",

        "CapLock",

        "F1",
        "F2",
        "F3",
        "F4",
        "F5",
        "F6",
        "F7",
        "F8",
        "F9",
        "F10",
        "F11",
        "F12",

        "Print Screen",
        "ScrollLock",
        "Paise",
        "Insert",
        "Home",
        "PageUp",
        "Delete",
        "End",
        "PageDown",
        "Right",
        "Left",
        "Down",
        "Up",

        "NumLockClear",

        "KP Divide",
        "KP Multiply",
        "KP Minus",
        "KP Plus",
        "KP Enter",
        "KP 1",
        "KP 2",
        "KP 3",
        "KP 4",
        "KP 5",
        "KP 6",
        "KP 7",
        "KP 8",
        "KP 9",
        "KP 0",
        "KP Full Stop",

        "Unknown",

        "Unknown",
        "Unknown",

        "KP Equals",
        "F13",
        "F14",
        "F15",
        "F16",
        "F17",
        "F18",
        "F19",
        "F20",
        "F21",
        "F22",
        "F23",
        "F24",

        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",

        "Unknown",
        "Unknown",
        "Unknown",

        "KP Comma",
        "Unknown",

        "Unknown",

        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",

        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",

        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",

        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",

        "Unknown",
        "Unknown",

        "LCTRL",
        "LSHIFT",
        "LALT",
        "LGUI",
        "RCTRL",
        "RSHIFT",
        "RALT",
        "RGUI",
};

enum class KeyState {
        None,
        Released, // Active for 1 frame
        Held,
        Pressed, // Active for 1 frame
};

struct Key {
        KeyState state;
};

enum class ActionType {
        Scalar,
        Axis,
};

enum class MouseButtonCode {
        Unknown = 0,
        Left    = 1,
        Middle  = 2,
        Right   = 3,
        X1      = 4,
        X2      = 5,

        Count = 6,
};

using ButtonState = KeyState;

struct MouseButton {
        ButtonState state;
};

struct Mouse {
        Vec2 position;
        Vec2 delta;
        Vec2 wheel_delta;

        MouseButton buttons[(u32)MouseButtonCode::Count];
};

union ActionValue {
        float scalar;
        Vec2  axis;
};

enum class InputType {
        Keyboard,
        Mouse,
};

struct MouseInput {
        MouseButtonCode button;
};

struct KeyboardInput {
        KeyCode key;
};

union Input {
        MouseInput    mouse;
        KeyboardInput keyboard;
};

struct InputAction {
        InputType type;
        Input     input;

        KeyCode GetKeyCode() const {
                return input.keyboard.key;
        }

        MouseButtonCode GetButtonCode() const {
                return input.mouse.button;
        }

        bool operator==(const InputAction& other) {
                // I can just treat both as mouse right? theyre just u32s.
                return (type == other.type) and (input.mouse.button == other.input.mouse.button);
        }

        bool operator!=(const InputAction& other) {
                return !(*this == other);
        }
};

struct Action {
        std::string              name;
        std::vector<InputAction> inputs;
        ActionValue              value;
        ActionValue              last_value;
        ActionType               type;

        float GetScalar() {
                assert(type == ActionType::Scalar);
                return value.scalar;
        }

        Vec2 GetAxis() {
                assert(type == ActionType::Axis);
                return value.axis;
        }

        bool IsPressed() {
                return last_value.scalar == 0 and value.scalar > 0;
        }

        bool IsReleased() {
                return value.scalar == 0 and last_value.scalar > 0;
        }

        void AddInput(InputAction input) {
                inputs.push_back(input);
        }

        void AddKey(KeyCode key) {
                inputs.emplace_back(InputType::Keyboard, Input{ .keyboard = KeyboardInput{ .key = key } });
        }

        void ReplaceInput(InputAction old_input, InputAction new_input) {
                for (u32 i = 0; i < inputs.size(); i++) {
                        if (inputs[i] != old_input) continue;

                        inputs[i] = new_input;
                        return;
                }
        }
};

struct GameContext;
struct Window;

struct InputSystem {
        Action actions[max_input_actions];
        u32    action_count;
        Key    keys[(u32)KeyCode::Count];
        Mouse  mouse;

        Window* window; // Need to handle resize and move

        void Init(Window* window);
        void Shutdown();
        bool Update();

        Action* RegisterAction(const std::string& name, ActionType type);
        Action* GetAction(const std::string& name);
};