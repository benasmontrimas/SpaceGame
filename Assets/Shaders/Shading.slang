import Lights;

import Base;

struct CameraMatrices {
        float4x4 projection;
        float4x4 view;
};

struct VSInput {
        float3 position;
        float3 normal;
        float2 uv;

        float4x4 model_matrix;
        u32      letter_index;
};

struct VSOutput {
        float4 position : SV_POSITION;
        float3 normal;
        float3 tangent;
        float2 uv;
        float2 uv1;
        float2 uv2;
        float3 weights;

        MaterialData material_data;
        MaterialID   material_id;

        float3 world_position;
};

enum MaterialID {
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

struct ShaderData {
        MaterialData material_data;
        MaterialID   material_id;
};

// ===== Input ===== //

[[vk::binding(0)]]
ConstantBuffer<CameraMatrices> camera_matrices;

[[vk::binding(1)]]
Sampler2D textures[];

// ================== //
// ===== VERTEX ===== //
// ================== //


[[shader("vertex")]] [[require(SPV_KHR_non_semantic_info)]] VSOutput main(VSInput input, uniform ShaderData* shading_data,
                                                                          uint instance_index : SV_VulkanInstanceID) {
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

        output.material_data = shading_data.material_data;
        output.material_id   = shading_data.material_id;

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


        if (length(input.world_position) < 10101) {
                albedo = { 0.0f, 0.0f, 1.0f, 1.0f };
                normal = normalize(input.world_position);
        }

        return albedo * light.Evaluate(input.world_position, normal);
        // return float4(1.0f, 0.0f, 0.0f, 1.0f);
}

// For World Space Text - Can replace with a precomputed value otherwise.
float ScreenPixelRange(VSOutput input) {
        Sampler2D tex = textures[input.material_data.texture_id_1];
        float     width, height;
        tex.GetDimensions(width, height);

        float px_range = 4;

        float2 unit_range      = float2(px_range) / float2(width, height);
        float2 screen_tex_size = rsqrt(pow(ddx(input.uv), 2) + pow(ddy(input.uv), 2));

        return max(0.5 * dot(unit_range, screen_tex_size), 1.0f);
}

float4 Text(VSOutput input) {
        float3 signed_distance_rgb = textures[input.material_data.texture_id_1].Sample(input.uv).rgb;
        float  signed_distance =
                max(min(signed_distance_rgb.r, signed_distance_rgb.g), min(max(signed_distance_rgb.r, signed_distance_rgb.g), signed_distance_rgb.b));

        float screen_distance = ScreenPixelRange(input) * (signed_distance - 0.5);

        float opacity = clamp(screen_distance + 0.5, 0.0, 1.0);

        return lerp(float4(0, 0, 0, 1), float4(1, 1, 1, 1), opacity);
}

[[shader("fragment")]] [[require(SPV_KHR_non_semantic_info)]] float4 main(VSOutput input) {
        input.normal = normalize(input.normal);

        switch (input.material_id) {
                case MaterialID::None:
                        return { 1, 0, 1, 1 };

                case MaterialID::SkyMap:
                        return SkyMapMaterial(input);
                case MaterialID::Basic:
                        return BasicMaterial(input);
                case MaterialID::Planet:
                        return PlanetMaterial(input);
                case MaterialID::Text:
                        return Text(input);

                default:
                        return { 1, 0, 1, 1 };
        }
}