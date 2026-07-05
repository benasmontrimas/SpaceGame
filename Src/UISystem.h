#pragma once

#include "Base.h"
#include "Model.h"

struct GameContext;

struct RenderedLetter {
        char        letter;
        Transform2D transform;
        Vec4        colour;
};

struct UISystem {
        static constexpr u64 max_letter_count  = 100'000;
        static constexpr u64 frame_buffer_size = max_letter_count * sizeof(InstanceDrawData);

        GameContext* game_context;
        ModelID      letter_model_id;
        ModelID      ui_model_id;
        ModelID      image_model_id;

        Texture title_font_atlas;
        Texture default_font_atlas;
        Texture image_texture;

        FT_Library ft_library;

        FT_Face title_font;
        FT_Face default_font;

        void Init(GameContext* _game_context);
        void Shutdown();
        void AddText(ModelInstance* model_instance, u32 text_letter_count);

        void CreateUIBoxModel();
        void CreateImageBox();
        void CreateUIImageModel();
        void CreateLetterModel();
};

enum class FontID {
        Title,
        Default,
};

struct RenderedText {
        std::string                 text{};
        Transform                   transform;
        std::vector<RenderedLetter> letter_data;
        Vec4                        default_colour{ 1, 1, 1, 1 };

        float  text_size;
        FontID font_id;
        // FT_Face font;

        float width;
        float height;

        void Init(UISystem* ui_system, FontID _font_id, float size);
        void SetText(UISystem* ui_system, const std::string& new_text);
        void SetSubText(UISystem* ui_system, const std::string& new_sub_text, u32 start);
        void SetColour(UISystem* ui_system, Vec4 colour);
        void SetColour(UISystem* ui_system, u32 letter_index, Vec4 colour); // NOTE: Count letter index from text, as we dont have spaces and new lines.

        void Draw(UISystem* ui_system);
};