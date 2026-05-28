/*
Marching cubes using modified version of Flying Edges
*/

struct EdgeInfo {
        uint case;
};

struct EdgeSumInfo {
        uint vertex_offset;
        uint vertex_count;

        uint triangle_offset;
        uint triangle_count;
};

// NOTE: Does this have padding issues between array values?
// Want uint4 * 5 for better packing -> 80 bytes vs 228 bytes OR uint4 * uint4 * 4 and tight pack, but need to work out index.
// Or cast to float array?
struct TriangleLookup {
        uint triangle_index[15]; // Groups of 3 -> upto 5 triangles
};

// NOTE: This could be 32 bytes, but it gets padded to 48...
struct Vertex {
        float3 position;
        float3 normal;
        float2 uv;
};

struct PlanetInfo {
        uint seed;      // 4
        float3 position; // 16 -
        uint chunk_cells; // 20
        uint chunk_dims; // 24

        // Input Arrays - For MC
        uint* edge_case_lookup; // 32 -
        TriangleLookup* triangle_lookup; // 40

        // Intermediate Arrays
        EdgeInfo* edges; // 48 -
        EdgeSumInfo* edge_sums; // 56
        float* triangle_normals; // 64 -

        // Outputs
        uint indices;

};

[shader("vertex")]
float main() {
        float x = 1.0f;

        return x;
}