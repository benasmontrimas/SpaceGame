import Base;

struct PointLight {
        float3 position;
        float  radius;
        float4 colour;

        float4 Evaluate(float3 point, float3 normal) {
                float3 direction = (position - point);

                float a = clamp(dot(normal, normalize(direction)), 0, 1);

                float r = length(direction);

                float distance_falloff = max(1 - pow(r / radius, 2), 0);
                distance_falloff       = distance_falloff * distance_falloff;

                return colour * a * distance_falloff;
        }
};

struct DirectionLight {
        float3 direction;
};

float WindowValue(float val, float val_max) {
        return pow(max(1 - pow(val / val_max, 4), 0), 2);
}