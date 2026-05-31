/*
Marching cubes using modified version of Flying Edges
*/

/*
NOTE: I can calculate Density function here
- But if need to return for editing purposes will need to write to buffer
- If want to pass in density values will need to do some extra work to check if values are modified, not sure how to do with no branching
- Otherwise just pass in a density and do in seperate pass, which also allows more control over passed values, or even just overwrite
*/

// NOTE: Lots of wasted memory using 2 bits out of 32, can pack.
// NOTE: How does the GPU pack these values in a pointer array? If same as HLSL array packing, then this uses 16 bytes per element instead of 4.
// Can use uint4 to improve memory packing.
struct EdgeInfo {
        uint edge_case;
}; // at Chunk Size 256 : ~67MB

// NOTE: Is *_offset not just previous edge *_count?
struct EdgeSumInfo {
        uint vertex_offset;
        uint vertex_count;

        uint triangle_offset;
        uint triangle_count;
}; // at Chunk Size 256 : ~1MB

// NOTE: Does this have padding issues between array values?
// Want uint4 * 5 for better packing -> 80 bytes vs 228 bytes OR uint4 * uint4 * 4 and tight pack, but need to work out index.
// Or cast to uint array?
struct TriangleLookup {
        uint triangle_index[16]; // Groups of 3 -> upto 5 triangles
}; // Constant : 0.01MB

// NOTE: This could be 32 bytes, but it gets padded to 48...
struct Vertex {
        float3 position;
        float3 normal;
        float2 uv;
};

// NOTE: Pass Chunk Cells/Dims as float?
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
        uint* indices;
        Vertex* vertices;
};

uint GetLookupIndex(PlanetInfo info, uint edge_index) {
        int edge_vertex_count = info.chunk_cells + 1;

        uint lookup_index = 0;

        uint e0_index = edge_index;
        lookup_index |= FlipFirstTwoBits(info.edges[e0_index].edge_case);

        uint e2_index = e0_index + 1;
        lookup_index |= info.edges[e2_index].edge_case << 2;

        uint e4_index = e2_index + edge_vertex_count;
        lookup_index |= FlipFirstTwoBits(info.edges[e4_index].edge_case) << 4;

        uint e6_index = e0_index + edge_vertex_count;
        lookup_index |= info.edges[e6_index].edge_case << 6;

        return lookup_index;
}

uint IsIntersection(PlanetInfo info, uint index) {
        uint val = info.edges[index].edge_case;
        return (val == 1) | (val == 2);
}

uint IsIntersection(PlanetInfo info, uint start_index, uint end_index) {
        uint val = info.edges[start_index].edge_case & 2;
        val |= info.edges[end_index].edge_case & 1;

        return (val == 1) | (val == 2);
}

uint GetIndex(uint3 coords, uint edge_vertex_count) {
        uint edge_vertex_count_squared = edge_vertex_count * edge_vertex_count;
        return (cell_coords.z * edge_vertex_count_squared) + (cell_coords.y * edge_vertex_count) + cell_coords.x;
}

uint CountIntersections(PlanetInfo info, uint3 cell_coords, uint edge_vertex_count) {
        uint edge_vertex_count_squared = edge_vertex_count * edge_vertex_count;

        uint start_index = GetIndex(cell_coords, edge_vertex_count);
        uint y_index = start_index + edge_vertex_count;
        uint z_index = start_index + (edge_vertex_count_squared);

        uint res = 0;
        res += IsIntersection(info, start_index);
        res += IsIntersection(info, start_index, y_index);
        res += IsIntersection(info, start_index, z_index);

        return res;
}

float3 Interpolate(float3 p1, float3 p2, float alpha) {
        return float3(1);
}

// TODO: Implement
float GetDensity(float3 position) {
        return 0.5f;
}

/*
Traverse all x edges and get case
*/
[shader("compute")]
[numthreads(8, 8, 1)]
void Pass1(uniform PlanetInfo info, uint3 dispatch_id : SV_DispatchThreadID) {
        // Get Index Values
        uint3 coords = uint3(0, dispatch_id.x, dispatch_id.y);

        // Get Density + World Position For cell
        float scale = (float)info.chunk_cells / (float)info.chunk_dims;
        float3 local_position = float3(coords) * scale;
        float3 world_position = info.position + local_position;

        float density = GetDensity(world_position);

        int edge_vertex_count = info.chunk_cells + 1;

        // Do first out of loop as prev doesnt exist
        uint start_index = (coords.z * edge_vertex_count * edge_vertex_count) + (coords.y * edge_vertex_count);
        uint is_inside = (density <= 0.0f);
        info.edges[start_index].edge_case |= is_inside << 1; // NOTE: Less than or Greater than? Doesnt matter really though.

        for (uint x = 1; x < edge_vertex_count - 1; x++) {
                world_position.x += scale;
                float density = GetDensity(world_position);

                is_inside = (density <= 0.0f);

                info.edges[start_index + x].edge_case       |= is_inside << 1;
                info.edges[start_index + (x - 1)].edge_case |= is_inside;
        }

        // Do last out of loop as next doesnt exist
        world_position.x += scale;
        density = GetDensity(world_position);

        is_inside = (density <= 0.0f);
        info.edges[start_index + (edge_vertex_count - 1)].edge_case |= is_inside;
}

uint FlipFirstTwoBits(uint v) {
        uint x = ((v) ^ (v >> 1)) & 1;
        return v ^ ((x) | (x << 1));
}

// Count intersections and traingles for each row.
[shader("compute")]
[numthreads(8, 8, 1)]
void Pass2(uniform PlanetInfo info, uint3 dispatch_id : SV_DispatchThreadID) {
        uint3 coords = uint3(0, dispatch_id.x, dispatch_id.y);
        int edge_vertex_count = info.chunk_cells + 1;
        // edge_sums is a 2D array of size [chunk_cells+1][chunk_cells+1] size.
        uint edge_sum_index = (coords.z * edge_vertex_count) + (coords.y);

        uint intersection_sum = 0;
        uint triangle_sum = 0;

        for (uint x = 0; x < info.chunk_cells; x++) {
                // ===== Get Intersection/Vertex Count ===== //

                coords.x = x;
                uint intersection_count = CountIntersections(info, coords, edge_vertex_count);

                intersection_sum += intersection_count;

                // ===== Get Marching Cubes Lookup Index ===== //

                uint lookup_index = 0;

                uint e0_index = edge_sum_index;
                lookup_index |= FlipFirstTwoBits(info.edges[e0_index].edge_case);

                uint e2_index = e0_index + 1;
                lookup_index |= info.edges[e2_index].edge_case << 2;

                uint e4_index = e2_index + edge_vertex_count;
                lookup_index |= FlipFirstTwoBits(info.edges[e4_index].edge_case) << 4;

                uint e6_index = e0_index + edge_vertex_count;
                lookup_index |= info.edges[e6_index].edge_case << 6;

                // ===== Lookup Triangle Count ===== //
                uint triangle_count = info.triangle_lookup[lookup_index].triangle_index[15];
                triangle_sum += triangle_count;
        }

        info.edge_sums[edge_sum_index].vertex_count = intersection_sum;
        info.edge_sums[edge_sum_index].triangle_count = triangle_sum;
}

// Write Indices and Vertices
[shader("compute")]
[numthreads(8, 8, 1)]
void Pass3(uniform PlanetInfo info, uint3 dispatch_id : SV_DispatchThreadID) {
        uint3 coords = uint3(0, dispatch_id.x, dispatch_id.y);
        int edge_vertex_count = info.chunk_cells + 1;

        uint edge_index = (coords.z * edge_vertex_count) + (coords.y);

        uint index_offset = info.edge_sums[edge_index].triangle_offset;
        uint vertex_offset = info.edge_sums[edge_index].vertex_offset;

        uint index_y_offset = info.edge_sums[edge_index + 1].triangle_offset;
        uint index_z_offset = info.edge_sums[edge_index + edge_vertex_count].triangle_offset;
        uint index_yz_offset = info.edge_sums[edge_index + edge_vertex_count + 1].triangle_offset;

        for (uint x = 0; x < info.chunk_cells; x++) {
                coords.x = x;

                // ===== Write Vertex Data ===== //

                // NOTE: Dont really like that im re doing this here
                uint lookup_index = GetLookupIndex(info, edge_index);
                uint edge_bits = info.edge_case_lookup[lookup_index];

                // ===== Generate Vertices for this points forward edges. (edges 0, 3, 8)
                // Only calculate those that are used

                float3 v0 = float3(coords);
                if ((edge_bits & (1 << 0)) > 0) {
                        float3 v1 = v0;
                        v1.x += 1.0;

                        info.vertices[index_offset] = Interpolate(v0, v1, 0.5);
                        index_offset++;
                }

                if ((edge_bits & (1 << 3)) > 0) {
                        float3 v3 = v0;
                        v1.z += 1.0;

                        info.vertices[index_offset] = Interpolate(v0, v3, 0.5);
                        index_offset++;
                }

                if ((edge_bits & (1 << 8)) > 0) {
                        float3 v4 = v0;
                        v1.y += 1.0;

                        info.vertices[index_offset] = Interpolate(v0, v4, 0.5);
                        index_offset++;
                }

                // Vertex is theintepolated value between x edge, y edge and z edge
                // Triangles is lookup for current cell to get 3 verts
                //

                // ===== For index in triangle, get the actual index in indices, and set.

                TriangleLookup triangles = info.triangle_lookup[lookup_index];

                uint3 edge_owner_lookup[12] = {
                        uint3(0),
                        uint3(1, 0, 0),
                        uint3(0, 0, 1),
                        uint3(0),
                        uint3(0, 1, 0),
                        uint3(1, 1, 0),
                        uint3(0, 1, 1),
                        uint3(0, 1, 0),
                        uint3(0),
                        uint3(1, 0, 0),
                        uint3(0, 0, 1),
                        uint3(1, 0, 1),
                };

                // NOTE: Need to keep track of current cell offset as well as +z +y +yz
                for (int i = 0; i < 15; i++) {
                        // Lookup MC table to get which edges make up the triangles.
                        uint local_index = triangles.triangle_index[i];
                        if (local_index == -1) break;

                        // Get the vertex which tracks this edge.
                        uint3 edge_coords = coords + edge_owner_lookup[local_index];

                        uint owning_edge_index = GetLookupIndex(info, (edge_coords.z * edge_vertex_count) + edge_coords.y);

                        uint owning_edge_case = info.edge_case_lookup[owning_edge_index];

                        // Check 0, 3, 8
                        if (owning_edge_case & (1 << 0)) {
                                if (local_index == 0) {
                                        info.indices[vertex_offset] =
                                }
                        }




                        // uint edge_offset = info.edge_sums[edge_index + 1].vertex_offset;

                        // uint3 other_coords = coords;
                        // other_coords.x += 1;

                        // uint other_index = GetLookupIndex(info, other_coords, edge_vertex_count);

                        // edge_offset += (edge_bits & 2) >> 1;

                        // info.indices[
                }
        }

}


