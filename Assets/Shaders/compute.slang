// Push constant
struct PlanetCreationData {
        uint seed;
};

RWStructuredBuffer<float> density_field;

[shader("compute")]
[numthreads(1,1,1)]
void main(uniform PlanetCreationData planet_creation_data, uint3 threadId : SV_DispatchThreadID) {
        uint index = threadId.x;
        density_field[index] = float(index);
}