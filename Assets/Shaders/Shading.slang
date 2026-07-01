import Lights;

import Base;

// ===== STRUCTS ===== //

enum MaterialType {
        None = 0,

        SkyMap = 1,
        Basic  = 2,
        Planet = 3,
        Text   = 4,

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
        [[vk::location(7)]] u32      user_value;
};

// ParameterBlock<VertexData> vertex_data;
// ParameterBlock<InstanceData> instance_data;

struct VSOutput {
        float4 position : SV_POSITION;
        float3 normal;
        float3 tangent;
        float2 uv;
        float2 uv1;
        float2 uv2;
        float3 weights;

        MaterialData material_data;
        MaterialType material_id;

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
                        float2 texture_dims   = float2(512, 512);
                        float2 cell_dimension = float2(102.0, 85.0) / texture_dims;
                        float  glyph_size     = 86.8125;
                        float2 cell_origin    = float2(0.0552134629, 0.247660187) / texture_dims;
                        u32    columns        = 5;

                        u32 col = input.user_value % columns;
                        u32 row = input.user_value / columns;

                        float2 uv_start = cell_dimension * float2(col, row) + cell_origin + float2(0.003, 0.003);
                        output.uv       = uv_start + (input.uv * cell_dimension) - float2(0.008, 0.002);

                        output.position                   = mul(input.model_matrix, float4(position, 1.0));
                        output.material_data.texture_id_1 = model_data.material_data.texture_id_1;
                        break;
        }

        return output;
}

// ==================== //
// ===== FRAGMENT ===== //
// ==================== //


float4 SkyMapMaterial(VSOutput input) {
        float theta = asin(input.normal.z);
        float phi   = atan2(input.normal.y, input.normal.x);

        float pi = float.getPi();
        float u  = (phi / (2 * pi)) + 0.5;
        float v  = (theta / (pi)) + 0.5;

        return textures[0].Sample(float2(u, v));
}

float4 BasicMaterial(VSOutput input) {
        return float4(0.3f, 1.0f, 0.8f, 1.0f);
}

float4 PlanetMaterial(VSOutput input) {
        // ===== Triplanar Mapping ===== //

        float  scale           = 1.0f / 50.0f;
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

        // normal = input.normal;

        // ===== Light ===== //

        PointLight light = {
                { 0, 20000, 100 },
                100000,
                { 1, 0.84, 0.66, 1.0 },
        };

        return albedo * light.Evaluate(input.world_position, normal);
        // return float4(1.0f, 0.0f, 0.0f, 1.0f);
}

float4 Text(VSOutput input) {
        float2 cell_dimension = { 102.0, 85.0 };
        float  glyph_size     = 86.8125;
        float2 cell_origin    = { 0.0552134629, 0.247660187 };
        u32    columns        = 5;

        float3 signed_distance_rgb = textures[input.material_data.texture_id_1].Sample(input.uv).rgb;
        float  signed_distance =
                max(min(signed_distance_rgb.r, signed_distance_rgb.g), min(max(signed_distance_rgb.r, signed_distance_rgb.g), signed_distance_rgb.b));

        // float screen_distance = ScreenPixelRange(input) * (signed_distance - 0.5);
        float pixel_range     = 512.0 / glyph_size * 2;
        float screen_distance = pixel_range * (signed_distance - 0.5);

        float opacity = clamp(screen_distance + 0.5, 0.0, 1.0);
        if (opacity < 0.1) discard;

        if (signed_distance < 0.8) return float4(1, 1, 1, 1);
        return float4(0, 0, 0, 1);
        // return lerp(float4(0, 0, 0, 1), float4(1, 1, 1, 1), signed_distance);
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

                default:
                        return { 1, 0, 1, 1 };
        }
}