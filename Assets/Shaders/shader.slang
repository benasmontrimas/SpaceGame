#include "Base.slang"

Sampler2D textures[];

struct VSInput {
        float3 position;
        float3 normal;
        float2 uv;
};

struct VSOutput {
        float4 position : SV_POSITION;
        float3 normal;
        float2 uv;

        MaterialID material_id;
};

enum MaterialID {
        None = 0,

        SkyMap = 1,
        Basic = 2,
        Planet = 3,

        Count,
};

struct ShaderData {
        float4x4 projection;
        float4x4 view;
        float4x4 model;

        MaterialID material_id;

        float pad[15];
};

[[shader("vertex")]] VSOutput main(VSInput input, uniform ShaderData* shading_data, uint instance_index : SV_VulkanInstanceID) {
        VSOutput output;

        if (shading_data.material_id == MaterialID::Planet) {
                output.normal = normalize(input.position);
        } else {
                output.normal   = normalize(mul((float3x3)transpose(shading_data.model), input.normal.xyz));
        }

        output.position = mul(shading_data.projection, mul(shading_data.view, mul(shading_data.model, float4(input.position.xyz, 1.0))));
        output.uv       = input.uv.xy;

        output.material_id = shading_data.material_id;

        return output;
}

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
        float3 light_dir = normalize(float3(0.2f, 1.0f, 0.0f));

        float l = max(dot(normalize(input.normal.xyz), light_dir), 0.0);

        float3 diffuse_colour = float3(0.4f, 0.6f, 0.1f) * l;

        return float4(diffuse_colour, 1.0f) ;
}

[[shader("fragment")]] [[require(SPV_KHR_non_semantic_info)]] float4 main(VSOutput input) {
        switch (input.material_id) {
                case MaterialID::None:
                        LogWarn("Unassigned Material!");
                        return {};

                case MaterialID::SkyMap:
                        return SkyMapMaterial(input);
                case MaterialID::Basic:
                        return BasicMaterial(input);
                case MaterialID::Planet:
                        return PlanetMaterial(input);

                default:
                        LogWarn("Unknown Material");
                        return {};
        }
}