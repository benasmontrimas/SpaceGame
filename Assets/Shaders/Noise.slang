float Hash(float n) {
        return frac(sin(n) * 43758.5453);
}

float Hash(float2 pos) {
        return fract(sin(dot(pos, float2(12.9898, 78.233))) * 43758.5453);
}

float2 Hash2(float2 pos) {
        float2 a = sin(float2(dot(pos, float2(31.455, 192.323)), dot(pos, float2(331.002, 59.329)))) * 43758.5453;
        return frac(a);
}

float Perlin(float3 pos) {
        // The noise function returns a value in the range -1.0f -> 1.0f

        float3 p = floor(pos);
        float3 f = frac(pos);

        f       = f * f * (3.0 - 2.0 * f);
        float n = p.x + p.y * 57.0 + 113.0 * p.z;

        return lerp(lerp(lerp(Hash(n + 0.0), Hash(n + 1.0), f.x), lerp(Hash(n + 57.0), Hash(n + 58.0), f.x), f.y),
                    lerp(lerp(Hash(n + 113.0), Hash(n + 114.0), f.x), lerp(Hash(n + 170.0), Hash(n + 171.0), f.x), f.y), f.z);
}

float VoronoiNoise(float2 pos) {
        float2 cell_coord = floor(pos);
        float2 cell_loc   = frac(pos);

        float min_distance = 1.0f;

        for (int y = -1; y <= 1; y++) {
                for (int x = -1; x <= 1; x++) {

                        float2 cell_offset = float2(x, y);

                        float2 cell_node = Hash2(cell_coord + cell_offset);

                        float distance = length(cell_offset + cell_node - cell_loc);

                        min_distance = min(min_distance, distance);
                }
        }

        return min_distance;
}