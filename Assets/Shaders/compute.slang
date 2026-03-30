struct FieldData {
        float density;
};

// Push constant
struct PlanetCreationData {
        uint seed;

        uint density_offset;
        FieldData* density_array;
};

[shader("compute")]
[numthreads(1,1,1)]
void main(uniform PlanetCreationData planet_creation_data, uint3 threadId : SV_DispatchThreadID) {
        uint index = threadId.x;
        planet_creation_data.density_array[planet_creation_data.density_offset + index].density = float(index);
}