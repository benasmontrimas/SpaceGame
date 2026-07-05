import Lights;

import Base;

// ===== STRUCTS ===== //

enum MaterialType {
        None = 0,

        SkyMap  = 1,
        Basic   = 2,
        Planet  = 3,
        Text    = 4,
        UI      = 5,
        UIImage = 6,

        Count,
};

struct MaterialData {
        u32 texture_id_1;
        u32 texture_id_2;
        u32 texture_id_3;
        u32 texture_id_4;
};

struct ModelData {
        MaterialType material_id;
        MaterialData material_data;
};

struct CameraMatrices {
        float4x4 projection;
        float4x4 view;
};

// ===== INPUTS + OUTPUTS ===== //

struct VertexData {
        [[vk::location(0)]] float3 position;
        [[vk::location(1)]] float3 normal;
        [[vk::location(2)]] float2 uv;

        [[vk::location(3)]] float4x4 model_matrix;
        [[vk::location(7)]] float4   colour0;
        [[vk::location(8)]] float4   colour1;
        [[vk::location(9)]] u32      user_value;
        [[vk::location(10)]] u32     user_value1;
        [[vk::location(11)]] u32     user_value2;
        [[vk::location(12)]] u32     user_value3;
};

// ParameterBlock<VertexData> vertex_data;
// ParameterBlock<InstanceData> instance_data;

struct VSOutput {
        float4 position : SV_POSITION;
        float3 normal;
        float3 tangent;
        float2 uv;

        MaterialData material_data;
        MaterialType material_id;

        float4 colour0;
        float4 colour1;

        float3 world_position;
};

[[vk::binding(0)]]
ConstantBuffer<CameraMatrices> camera_matrices;

[[vk::binding(1)]]
Sampler2D textures[];

[[vk::push_constant]]
ModelData model_data;

// ================== //
// ===== VERTEX ===== //
// ================== //


[[shader("vertex")]] [[require(SPV_KHR_non_semantic_info)]] VSOutput main(VertexData input, int instance_index : SV_VulkanInstanceID) {
        VSOutput output;
        float4   tangent = { cross(input.normal, { 0.0, 1.0, 0.0 }), 0.0 };

        if (length(tangent) == 0.0) {
                tangent = { cross(input.normal, { 0.0, 0.0, 1.0 }), 0.0 };
        }

        tangent = normalize(tangent);

        output.tangent = normalize(mul(input.model_matrix, tangent)).xyz;
        output.normal  = normalize(mul((float3x3)(input.model_matrix), input.normal));
        output.uv      = input.uv;

        float3 position = input.position.xyz;

        // ===== To Model Space ===== //

        output.world_position = position;
        output.position       = mul(input.model_matrix, float4(position, 1.0));
        output.position       = mul(camera_matrices.view, output.position);
        output.position       = mul(camera_matrices.projection, output.position);

        output.material_data = model_data.material_data;
        output.material_id   = model_data.material_id;

        switch (model_data.material_id) {
                case MaterialType::Text:
                        float2 texture_dims   = float2(612, 510);
                        float2 cell_dimension = float2(102.0, 85.0) / texture_dims;
                        float2 cell_origin    = float2(0.10625, 0.28125) / texture_dims;
                        if (input.user_value1 == 98) cell_origin = float2(0.31875, 0.18125) / texture_dims;
                        u32 columns = 6;

                        // printf("Value: %u\n", input.user_value);
                        u32 col = input.user_value % columns;
                        u32 row = input.user_value / columns;

                        float2 uv_start = cell_dimension * float2(col, row) + cell_origin + float2(0.001, 0.001);
                        output.uv       = uv_start + (input.uv * (cell_dimension - cell_origin)) * 0.99;

                        output.position                   = mul(input.model_matrix, float4(position, 1.0));
                        output.material_data.texture_id_1 = input.user_value1;
                        break;
                case MaterialType::UI:
                        output.colour0 = input.colour0;
                        output.colour1 = input.colour1;

                        output.position = mul(input.model_matrix, float4(position, 1.0));

                        break;
                case MaterialType::UIImage:
                        output.colour0 = input.colour0;
                        output.colour1 = input.colour1;

                        output.position = mul(input.model_matrix, float4(position, 1.0));
                        break;
        }

        return output;
}

// ==================== //
// ===== FRAGMENT ===== //
// ==================== //


float4 SkyMapMaterial(VSOutput input) {
        float3 normal = normalize(input.world_position.xyz);

        float theta = asin(normal.z);
        float phi   = atan2(normal.y, normal.x);

        float pi = float.getPi();
        float u  = (phi / (2 * pi)) + 0.5;
        float v  = (theta / (pi)) + 0.5;

        float4 colour = textures[0].Sample(float2(u, v));
        return colour;
}

float4 BasicMaterial(VSOutput input) {
        float4 albedo = textures[input.material_data.texture_id_1].Sample(input.uv);

        PointLight light = {
                { 0, 20000, 100 },
                100000,
                { 1, 0.84, 0.66, 1.0 },
        };

        light.colour.xyz = light.colour.xyz / 4.0f;

        float4 colour = albedo * light.Evaluate(input.world_position, input.normal);
        colour.a      = 1.0f;
        return colour;
}

float4 PlanetMaterial(VSOutput input) {
        // ===== Triplanar Mapping ===== //

        float  scale           = 1.0f / 100.0f;
        float3 scaled_position = input.world_position * scale;

        float2 uv0 = scaled_position.zy;
        float2 uv1 = scaled_position.xz;
        float2 uv2 = scaled_position.xy;

        uv0.y += 0.5;
        uv2.x += 0.5;

        float3 abs_normal = abs(input.normal);
        float3 weights    = pow(abs_normal / (abs_normal.x + abs_normal.y + abs_normal.z), 2);

        // ===== Albedo ===== //

        float4 sample_0 = textures[1].Sample(uv0) * weights.x;
        float4 sample_1 = textures[1].Sample(uv1) * weights.y;
        float4 sample_2 = textures[1].Sample(uv2) * weights.z;

        float4 albedo = sample_0 + sample_1 + sample_2;

        // ===== Normal Mapping ===== //

        float3 bitangent = cross(input.normal, input.tangent);

        float3x3 tbn = float3x3(input.tangent, normalize(bitangent), normalize(input.normal));

        float3 texture_normal_0 = textures[2].Sample(uv0).xyz;
        float3 texture_normal_1 = textures[2].Sample(uv1).xyz;
        float3 texture_normal_2 = textures[2].Sample(uv2).xyz;

        float3 normal_x = float3(texture_normal_0.xy + input.normal.zy, input.normal.x);
        float3 normal_y = float3(texture_normal_1.xy + input.normal.xz, input.normal.y);
        float3 normal_z = float3(texture_normal_2.xy + input.normal.xy, input.normal.z);

        float3 normal = normalize(normal_x.zyx * weights.x + normal_y.xzy * weights.y + normal_z.xyz * weights.z);

        // return float4(input.normal, 1.0f);

        // ===== Light ===== //

        PointLight light = {
                { 0, 20000, 100 },
                100000,
                { 1, 0.84, 0.66, 1.0 },
        };

        light.colour.xyz = light.colour.xyz / 4.0f;

        float4 colour = albedo * light.Evaluate(input.world_position, normal);
        colour.a      = 1.0f;
        return colour;
}

float4 Text(VSOutput input) {
        float glyph_size = 80.0;

        float3 signed_distance_rgb = textures[input.material_data.texture_id_1].Sample(input.uv).rgb;
        float  signed_distance =
                max(min(signed_distance_rgb.r, signed_distance_rgb.g), min(max(signed_distance_rgb.r, signed_distance_rgb.g), signed_distance_rgb.b));

        // float screen_distance = ScreenPixelRange(input) * (signed_distance - 0.5);
        float pixel_range     = 128.0 / glyph_size * 2;
        float screen_distance = pixel_range * (signed_distance - 0.5);

        float opacity = clamp(screen_distance + 0.5, 0.0, 1.0);

        if (opacity < 0.1) discard;

        float hor_d = ddx(input.uv.x) * 100;

        float border = step(signed_distance, 1 - hor_d);
        border       = 1.0;

        float4 colour = input.colour0;
        colour.xyz *= opacity;

        return colour;
        // return lerp(float4(0, 0, 0, 1), float4(1, 1, 1, 1), signed_distance);
}

float RoundedBox(float v) {
        return pow(abs(v) * 2, 20);
}

float4 UI(VSOutput input) {
        float a = RoundedBox(input.uv.x - 0.5) + RoundedBox(input.uv.y - 0.5);

        float hor_d = ddx(input.uv.x) * 3;
        float ver_d = ddy(input.uv.y) * 3;
        float r     = 0.1;

        // NOTE: DO THIS FOR TEXT BORDER TOO
        float  aspect_ratio = ver_d / hor_d;
        float2 ar           = float2(aspect_ratio, 1.0);
        // printf("ar: %f\n", aspect_ratio);

        float2 closest_point_to_rect    = clamp(input.uv, float2(r), float2(1) - float2(r));
        float  distance_to_rounded_rect = distance(closest_point_to_rect * ar, input.uv * ar) - 0.1;

        float inside = step(distance_to_rounded_rect, 0.0);
        if (inside == 0) discard;

        float border_x = step(distance_to_rounded_rect, -hor_d);
        float border_y = step(distance_to_rounded_rect, -ver_d);

        float border = min(border_x, border_y);

        // a = input.uv.x

        // if (a > 1.0) discard;
        // float border = step(a, 0.5);

        return border * input.colour0 + (1.0 - border) * input.colour1;
}

float4 UIImage(VSOutput input) {
        float4 image = textures[input.material_data.texture_id_1].Sample(input.uv);

        // if (image.a < 1.0) discard;

        image.a = input.colour0.a;

        return image;
}

[[shader("fragment")]] [[require(SPV_KHR_non_semantic_info)]] float4 main(VSOutput input) {
        input.normal = normalize(input.normal);

        switch (input.material_id) {
                case MaterialType::None:
                        return { 1, 0, 1, 1 };

                case MaterialType::SkyMap:
                        return SkyMapMaterial(input);
                case MaterialType::Basic:
                        return BasicMaterial(input);
                case MaterialType::Planet:
                        return PlanetMaterial(input);
                case MaterialType::Text:
                        return Text(input);
                case MaterialType::UI:
                        return UI(input);
                case MaterialType::UIImage:
                        return UIImage(input);

                default:
                        return { 1, 0, 1, 1 };
        }
}