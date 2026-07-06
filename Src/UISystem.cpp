#include "UISystem.h"

#include "Game.h"

void UISystem::CreateUIBoxModel() {
        constexpr u32 index_count  = 6;
        constexpr u32 vertex_count = 4;

        // Counter clock wise
        Vec3 normal{ 0.0f, 0.0f, -1.0f };

        // bottom left
        VertexDrawData v0{
                .position = { 0.0f, 0.0f, 0.0f },
                .normal   = normal,
                .uv       = { 0.0f, 0.0f },
        };

        // bottom right
        VertexDrawData v1 {
                .position = { 1.0f, 0.0f, 0.0f }, .normal = normal,
                .uv = {
                        1.0f,
                        0.0f,
                },
        };

        // top right
        VertexDrawData v2{
                .position = { 1.0f, 1.0f, 0.0f },
                .normal   = normal,
                .uv       = { 1.0f, 1.0f },
        };

        // top left
        VertexDrawData v3{
                .position = { 0.0f, 1.0f, 0.0f },
                .normal   = normal,
                .uv       = { 0.0f, 1.0f },
        };

        VertexDrawData vertices[vertex_count] = { v0, v1, v2, v3 };
        u32            indices[index_count]   = { 0, 2, 1, 0, 3, 2 };

        ui_model_id = game_context->model_system.CreateModel(&vertices[0], vertex_count, &indices[0], index_count, max_letter_count, true);

        Material ui_material{
                .type = MaterialType::UI,
        };

        game_context->model_system[ui_model_id].material = ui_material;
}

void UISystem::CreateUIImageModel() {
        constexpr u32 index_count  = 6;
        constexpr u32 vertex_count = 4;

        // Counter clock wise
        Vec3 normal{ 0.0f, 0.0f, -1.0f };

        // bottom left
        VertexDrawData v0{
                .position = { 0.0f, 0.0f, 0.0f },
                .normal   = normal,
                .uv       = { 0.0f, 0.0f },
        };

        // bottom right
        VertexDrawData v1 {
                .position = { 1.0f, 0.0f, 0.0f }, .normal = normal,
                .uv = {
                        1.0f,
                        0.0f,
                },
        };

        // top right
        VertexDrawData v2{
                .position = { 1.0f, 1.0f, 0.0f },
                .normal   = normal,
                .uv       = { 1.0f, 1.0f },
        };

        // top left
        VertexDrawData v3{
                .position = { 0.0f, 1.0f, 0.0f },
                .normal   = normal,
                .uv       = { 0.0f, 1.0f },
        };

        VertexDrawData vertices[vertex_count] = { v0, v1, v2, v3 };
        u32            indices[index_count]   = { 0, 2, 1, 0, 3, 2 };

        image_model_id = game_context->model_system.CreateModel(&vertices[0], vertex_count, &indices[0], index_count, 1'000, true);

        image_texture.Load(game_context->vulkan_device, game_context->graphics_command_pool, game_context->graphics_queue, "Assets/Textures/BoondogsShed.ktx",
                           game_context->vulkan_allocator);

        game_context->AddTexture(image_texture, 10);

        Material ui_material{ .type     = MaterialType::UIImage,
                              .ui_image = {
                                      10,
                              } };

        game_context->model_system[image_model_id].material = ui_material;
}

void UISystem::CreateLetterModel() {
        // ===== Initialise Letter Quad ===== //

        constexpr u32 index_count  = 6;
        constexpr u32 vertex_count = 4;

        // Counter clock wise
        Vec3 normal{ 0.0f, 0.0f, -1.0f };

        constexpr float texture_coord_size = 1.0f;

        // bottom left
        VertexDrawData v0{
                .position = { 0.0f, 0.0f, 0.0f },
                .normal   = normal,
                .uv       = { 0.0f, 0.0f },
        };

        // bottom right
        VertexDrawData v1 {
                .position = { 1.0f, 0.0f, 0.0f }, .normal = normal,
                .uv = {
                        texture_coord_size,
                        0.0,
                },
        };

        // top right
        VertexDrawData v2{
                .position = { 1.0f, 1.0f, 0.0f },
                .normal   = normal,
                .uv       = { texture_coord_size, texture_coord_size },
        };

        // top left
        VertexDrawData v3{
                .position = { 0.0f, 1.0f, 0.0f },
                .normal   = normal,
                .uv       = { 0.0f, texture_coord_size },
        };

        VertexDrawData vertices[vertex_count] = { v0, v1, v2, v3 };
        u32            indices[index_count]   = { 0, 2, 1, 0, 3, 2 };

        transparent_letter_model_id = game_context->model_system.CreateModel(&vertices[0], vertex_count, &indices[0], index_count, max_letter_count, true);
        letter_model_id             = game_context->model_system.CreateModel(&vertices[0], vertex_count, &indices[0], index_count, max_letter_count);

        Material text_material{
                .type = MaterialType::Text,
                .text = { .texture_start = (99) },
        };

        game_context->model_system[transparent_letter_model_id].material = text_material;
        game_context->model_system[letter_model_id].material             = text_material;
}

// ===== TEXT RENDERING ===== //

void UISystem::Init(GameContext* _game_context) {
        game_context = _game_context;

        // ===== Textures ===== //

        title_font_atlas.Load(game_context->vulkan_device, game_context->graphics_command_pool, game_context->graphics_queue, "Assets/Fonts/TitleAtlas.ktx",
                              game_context->vulkan_allocator);

        game_context->AddTexture(title_font_atlas, 99);

        default_font_atlas.Load(game_context->vulkan_device, game_context->graphics_command_pool, game_context->graphics_queue, "Assets/Fonts/DefaultAtlas.ktx",
                                game_context->vulkan_allocator);

        game_context->AddTexture(default_font_atlas, 98);

        CreateUIBoxModel();
        CreateUIImageModel();
        CreateLetterModel();

        // ===== Init Free Type ===== //

        FT_Error error_code = FT_Init_FreeType(&ft_library);
        if (error_code != 0) {
                std::println("Failed initialising FreeType: {}", error_code);
                exit(1);
        }

        error_code = FT_New_Face(ft_library, "Assets/Fonts/Aboreto_Regular.2.ttf", 0, &title_font);
        if (error_code != 0) {
                std::println("Failed loading font: {}", error_code);
                exit(1);
        }

        error_code = FT_New_Face(ft_library, "Assets/Fonts/AdwaitaMono-Bold.ttf", 0, &default_font);
        if (error_code != 0) {
                std::println("Failed loading font: {}", error_code);
                exit(1);
        }
}

void UISystem::Shutdown() {
        title_font_atlas.Destroy(game_context);
        default_font_atlas.Destroy(game_context);

        FT_Done_Face(title_font);
        FT_Done_Face(default_font);
        FT_Done_FreeType(ft_library);
}

// Maybe just collect and write when we render. Easier to track as well.
void UISystem::AddText(ModelInstance* model_instance, u32 text_letter_count) {
        for (u32 i = 0; i < text_letter_count; i++) {
                game_context->model_system.Draw(model_instance[i]);
        }
}

void RenderedText::Init(UISystem* ui_system, FontID _font_id, float size) {
        text_size = size;

        font_id = _font_id;

        transform.position = { -1, -1, 0 };
}

void RenderedText::SetText(UISystem* ui_system, const std::string& new_text) {
        width  = 0;
        height = text_size;

        FT_F26Dot6 char_width  = u32(text_size) << 6;
        FT_F26Dot6 char_height = u32(text_size) << 6;

        FT_Face font;

        switch (font_id) {
                case FontID::Title:
                        font = ui_system->title_font;
                        break;
                case FontID::Default:
                        font = ui_system->default_font;
                        break;
                default:
                        std::println("Unexpected font id");
                        exit(1);
        }

        FT_Set_Char_Size(font, char_width, char_height, 0, 0);

        // == //

        text         = new_text;
        u32 text_len = text.length();

        letter_data.clear();
        letter_data.reserve(text_len); // Might not need all that space

        float x_offset = 0;
        float y_offset = 0;

        float space_offset    = text_size;
        float char_offset     = text_size;
        float new_line_offset = text_size;

        char prev_letter = 0;
        for (u32 i = 0; i < text_len; i++) {
                char letter = text[i];

                if (letter >= 'a' and letter <= 'z') letter += 'A' - 'a';

                if ((letter >= 'A' and letter <= 'Z') or letter == ' ' or (letter >= '0' and letter <= '9')) {
                        FT_Load_Char(font, letter, FT_LOAD_DEFAULT);
                        FT_Glyph_Metrics glyph_metrics = font->glyph->metrics;

                        Transform2D letter_transform{};

                        // width = x_offset;

                        if (i > 0) {
                                FT_Vector kerning;
                                FT_Get_Kerning(font, letter, prev_letter, FT_KERNING_DEFAULT, &kerning);

                                x_offset += (kerning.x >> 6);
                        }


                        letter_transform.position = { x_offset, y_offset + (glyph_metrics.horiBearingY >> 6) };
                        x_offset += (glyph_metrics.horiAdvance >> 6);

                        width = std::max(width, x_offset);

                        letter_transform.scale = {
                                text_size,
                                text_size,
                        };

                        if (letter == ' ') continue;

                        letter_data.emplace_back(letter, letter_transform, default_colour);
                } else {
                        switch (letter) {
                                case ' ':
                                        x_offset += space_offset;
                                        break;
                                case '\n':
                                        x_offset = 0;
                                        y_offset += new_line_offset;
                                        height += text_size;
                                        break;
                                case '\t':
                                        x_offset += 4 * space_offset;
                                        break;

                                default:
                                        std::println("[WARN] Character is not supported, skipping: {}", letter);
                        }
                }

                prev_letter = letter;
        }
}

void RenderedText::SetColour(UISystem* ui_system, Vec4 colour) {
        default_colour = colour;

        for (u32 i = 0; i < letter_data.size(); i++) {
                letter_data[i].colour = colour;
        }
}

void RenderedText::Draw(UISystem* ui_system, bool transparent) {
        std::vector<ModelInstance> letter_draw_data;
        letter_draw_data.resize(letter_data.size());

        // NOTE: No rotation yet
        Mat4 text_mat = glm::translate(Mat4(1.0f), transform.position);
        text_mat      = glm::scale(text_mat, transform.scale);

        for (u32 i = 0; i < letter_data.size(); i++) {

                float window_width  = (float)ui_system->game_context->window.width;
                float window_height = (float)ui_system->game_context->window.height;

                Vec3 position = {
                        transform.position.x + letter_data[i].transform.position.x / window_width,
                        transform.position.y + letter_data[i].transform.position.y / window_height,
                        transform.position.z,
                };

                Vec3 scale = {
                        transform.scale.x * letter_data[i].transform.scale.x / window_width,
                        transform.scale.y * letter_data[i].transform.scale.y / window_height,
                        transform.scale.z,
                };

                Mat4 letter_mat = glm::translate(Mat4(1.0f), position);
                letter_mat      = glm::scale(letter_mat, scale);

                if (transparent) letter_draw_data[i].model_id = ui_system->transparent_letter_model_id;
                else letter_draw_data[i].model_id = ui_system->letter_model_id;

                letter_draw_data[i].transform.position = position;
                letter_draw_data[i].transform.rotation = Quat{};
                letter_draw_data[i].transform.scale    = scale;

                letter_draw_data[i].colour0 = letter_data[i].colour;

                if (letter_data[i].letter >= 'A' and letter_data[i].letter <= 'Z') letter_draw_data[i].user_value = (letter_data[i].letter - 'A') + 10;
                else letter_draw_data[i].user_value = letter_data[i].letter - '0';

                switch (font_id) {
                        case FontID::Title:
                                letter_draw_data[i].user_value1 = 99;
                                break;
                        case FontID::Default:
                                letter_draw_data[i].user_value1 = 98;
                                break;
                }
        }

        ui_system->AddText(letter_draw_data.data(), letter_draw_data.size());
}