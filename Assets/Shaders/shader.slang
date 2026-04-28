/* Copyright (c) 2025-2026, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

struct VSInput {
        float3 Pos;
        float3 Normal;
        float2 UV;
};

Sampler2D textures[];

struct ShaderData {
        float4x4 projection;
        float4x4 view;
        float4x4 model;
};

struct VSOutput {
        float4   Pos : SV_POSITION;
        float3   Normal;
        float2   UV;
        float3   Factor;
        float3   LightVec;
        float3   ViewVec;
        uint32_t InstanceIndex;
};

// clang-format off
[shader("vertex")]
VSOutput main(VSInput input, uniform ShaderData* shaderData, uint instanceIndex : SV_VulkanInstanceID) {
        // clang-format on
        VSOutput output;
        float4x4 modelMat    = shaderData.model;
        output.Normal        = mul((float3x3)mul(shaderData.view, modelMat), input.Normal);
        output.UV            = input.UV;
        output.Pos           = mul(shaderData.projection, mul(shaderData.view, mul(modelMat, float4(input.Pos.xyz, 1.0))));
        output.Factor        = 1.0f;
        output.InstanceIndex = instanceIndex;
        // Calculate view vectors required for lighting
        float4 fragPos       = mul(mul(shaderData.view, modelMat), float4(input.Pos.xyz, 1.0));
        output.LightVec      = fragPos.xyz;
        output.ViewVec       = -fragPos.xyz;
        return output;
}

        // clang-format off
[shader("fragment")] float4 main(VSOutput input) {
        // clang-format on
        // Phong lighting
        float3 N        = normalize(input.Normal);
        float3 L        = normalize(input.LightVec);
        float3 V        = normalize(input.ViewVec);
        float3 R        = reflect(-L, N);
        float3 diffuse  = max(dot(N, L), 0.0025);
        float3 specular = pow(max(dot(R, V), 0.0), 16.0) * 0.75;
        // Sample from texture
        // float3 color = textures[NonUniformResourceIndex(input.InstanceIndex)].Sample(input.UV).rgb * input.Factor;
        float3 color    = { 0.0f, 1.0f, 0.0f };
        return float4(diffuse * color.rgb + specular, 1.0);
}