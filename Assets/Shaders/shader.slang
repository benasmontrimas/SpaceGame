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
};

struct ShaderData {
        float4x4 projection;
        float4x4 view;
        float4x4 model;
};

[shader("vertex")]
VSOutput main(VSInput input, uniform ShaderData* shader_data, uint instance_index : SV_VulkanInstanceID) {
        VSOutput output;

        output.position    = mul(shader_data.projection, mul(shader_data.view, mul(shader_data.model, float4(input.position.xyz, 1.0))));
        output.normal      = normalize(mul((float3x3)transpose(shader_data.model), input.normal.xyz));
        output.uv          = input.uv.xy;

        return output;
}

[shader("fragment")]
float4 main(VSOutput input) {
        float theta = asin(input.normal.z);
        float phi   = atan2(input.normal.y, input.normal.x);

        float pi = float.getPi();
        float u  = (phi / (2 * pi)) + 0.5;
        float v  = (theta / (pi)) + 0.5;

        return textures[0].Sample(float2(u, v));
}