/*
Marching cubes using modified version of Flying Edges
*/

struct EdgeSumInfo {
        uint vertex_offset;
        uint vertex_count;

        uint triangle_offset;
        uint triangle_count;

        void Print() {
                printf("SHADER - Edge Sums:\n");
                printf("    Vertex Offset: %u\n", vertex_offset);
                printf("    Vertex Count: %u\n", vertex_count);
                printf("    Triangle Offset: %u\n", triangle_offset);
                printf("    Triangle Count: %u\n", triangle_count);
        }
};

struct TriangleLookup {
        uint triangle_index[16]; // Groups of 3 -> upto 5 triangles

        void Print() {
                printf("SHADER - Triangle Lookup:\n");
                printf("    %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u\n",
                        triangle_index[0] , triangle_index[1] , triangle_index[2] , triangle_index[3],
                        triangle_index[4] , triangle_index[5] , triangle_index[6] , triangle_index[7],
                        triangle_index[8] , triangle_index[9] , triangle_index[10], triangle_index[11],
                        triangle_index[12], triangle_index[13], triangle_index[14], triangle_index[15],);
        }
};

// NOTE: This could be 32 bytes, but it gets padded to 48...
struct Vertex {
        float4 position;
        float4 normal;
        float4 uv;

        void Print() {
                printf("SHADER - Vertex:\n");
                printf("    Position:   %f, %f, %f\n", position.x, position.y, position.z);
                printf("    Normal:     %f, %f, %f\n", normal.x, normal.y, normal.z);
                printf("    uv:         %f, %f\n", uv.x, uv.y);
        }
};

// NOTE: Pass Chunk Cells/Dims as float?
struct PlanetInfo {
        // Input Arrays - For MC
        uint* edge_case_lookup;
        TriangleLookup* triangle_lookup;

        // Intermediate Arrays
        uint* edges;
        EdgeSumInfo* edge_sums;
        float* triangle_normals;

        // Outputs
        uint* indices;
        Vertex* vertices;

        uint chunk_cells;
        uint chunk_dims;

        float3 position;

        uint seed;

        void Print() {
                printf("SHADER - PlanetInfo:\n");
                printf("    Edge Lookup:        %p\n", edge_case_lookup);
                printf("    Triangle Lookup:    %p\n", triangle_lookup);
                printf("    Edges:              %p\n", edges);
                printf("    Edge Sums:          %p\n", edge_sums);
                printf("    Triangle Normals:   %p\n", triangle_normals);
                printf("    Indices:            %p\n", indices);
                printf("    Vertices:           %p\n", vertices);
                printf("    Chunk Cells:        %u\n", chunk_cells);
                printf("    Chunk Dims:         %u\n", chunk_dims);
                printf("    Position:           %f, %f, %f\n", position.x, position.y, position.z);
                printf("    Seed:               %u\n", seed);
        }
};

[vk::push_constant] PlanetInfo info;

uint GetLookupIndex(uint edge_index) {
        int edge_vertex_count = info.chunk_cells + 1;

        uint lookup_index = 0;

        uint e0_index = edge_index;
        lookup_index |= FlipFirstTwoBits(info.edges[e0_index]);

        uint e2_index = e0_index + 1;
        lookup_index |= info.edges[e2_index] << 2;

        uint e4_index = e2_index + edge_vertex_count;
        lookup_index |= FlipFirstTwoBits(info.edges[e4_index]) << 4;

        uint e6_index = e0_index + edge_vertex_count;
        lookup_index |= info.edges[e6_index] << 6;

        return lookup_index;
}

uint IsIntersection(uint index) {
        uint val = info.edges[index];
        if (((val >> 1) ^ (val & 1)) > 1) printf("Edge: %u %u\n", val >> 1, val & 1);
        return (val == 1) | (val == 2);
}

uint IsIntersection(uint start_index, uint end_index) {
        // uint val = info.edges[start_index] & 2;
        // val |= info.edges[end_index] & 1;

        // return (val == 1) | (val == 2);
        if (((info.edges[start_index] >> 1) ^ (info.edges[end_index] >> 1)) > 0) printf("Edge: %u %u", info.edges[start_index] >> 1, info.edges[end_index] >> 1);

        if (((info.edges[start_index] >> 1) ^ (info.edges[end_index] >> 1)) > 0) return 1;
        return 0;
}

uint GetIndex(uint3 coords) {
        uint edge_count = info.chunk_cells + 1;
        uint edge_vertex_count_squared = edge_count * edge_count;
        if (((coords.z * edge_vertex_count_squared) + (coords.y * edge_count) + coords.x) > 0) return 1;
        return 0;
}

uint CountIntersections(uint3 cell_coords) {
        uint edge_count = info.chunk_cells + 1;
        uint edge_vertex_count_squared = edge_count * edge_count;

        uint start_index = GetIndex(cell_coords);
        uint y_index = start_index + edge_count;
        uint z_index = start_index + (edge_vertex_count_squared);

        uint res = 0;
        res += IsIntersection(start_index);
        res += IsIntersection(start_index, y_index);
        res += IsIntersection(start_index, z_index);

        return res;
}

// TODO: Implement
float GetDensity(float3 position) {
        return length(position) - 600;
}

/*
Traverse all x edges and get case
*/
[shader("compute")]
[require(spirv, SPV_KHR_non_semantic_info)]
[numthreads(64, 1, 1)]
void Pass1(uint3 dispatch_id : SV_DispatchThreadID) {
        // Get Index Values
        uint3 coords = uint3(0, dispatch_id.x, dispatch_id.y);
        // if (coords.y > info.chunk_cells || coords.z > info.chunk_cells) return;

        // Get Density + World Position For cell
        float scale = (float)info.chunk_cells / (float)info.chunk_dims;
        float3 local_position = float3(coords) * scale;
        float3 world_position = info.position + local_position;

        float density = GetDensity(world_position);

        int cell_count = info.chunk_cells;
        int edge_count = cell_count + 1;

        int y_step = cell_count;
        int z_step = y_step * edge_count;

        // ===== Do First Check Out of Loop ===== //

        uint start_index = (coords.z * z_step) + (coords.y * y_step);
        uint is_inside = (density <= 0.0f);

        // if (start_index == 0) {
        //         info.Print();
        // }


        info.edges[start_index] = is_inside << 1;
        // ===== Loop All X Edges ===== //

        for (uint x = 1; x < edge_count; x++) {
                world_position.x += scale;
                float density = GetDensity(world_position);

                is_inside = (density <= 0.0f);

                info.edges[start_index + x]       = is_inside << 1;
                info.edges[start_index + (x - 1)] |= is_inside;
        }

        // ===== Do Last Check Out of Loop ===== //

        world_position.x += scale;
        density = GetDensity(world_position);

        is_inside = (density <= 0.0f);

        info.edges[start_index + (edge_count - 1)] |= is_inside;
}

uint FlipFirstTwoBits(uint v) {
        // uint x = ((v) ^ (v >> 1)) & 1;
        // return v ^ ((x) | (x << 1));

        uint res = 0;
        res |= (v & 1) << 1;
        res |= (v & 2) >> 1;
        return res;
}

// TODO: Need to have cells--, as we currently will need an entire extra dispatch for 1 extra row. better be one under!
// Count intersections and traingles for each row.
[shader("compute")]
[require(spirv, SPV_KHR_non_semantic_info)]
[numthreads(64, 1, 1)]
void Pass2(uint3 dispatch_id : SV_DispatchThreadID) {
        if (dispatch_id.x >= info.chunk_cells) return;

        uint3 coords = uint3(0, dispatch_id.x, dispatch_id.y);

        int cell_count = info.chunk_cells;
        int edge_count = cell_count + 1;

        int y_step = cell_count;
        int z_step = y_step * edge_count;

        // edge_sums is a 2D array of size [chunk_cells+1][chunk_cells+1] size.
        uint edge_sum_index = (coords.z * y_step) + (coords.y);
        uint first_edge_index = (coords.z * z_step) + (coords.y * y_step);

        uint intersection_sum = 0;
        uint triangle_sum = 0;

        for (uint x = 0; x < cell_count; x++) {
                coords.x = x;

                // ===== Get Marching Cubes Lookup Index ===== //

                uint lookup_index = 0;

                uint e0_index = first_edge_index + x;
                lookup_index |= FlipFirstTwoBits(info.edges[e0_index]);

                uint e2_index = e0_index + z_step;
                lookup_index |= info.edges[e2_index] << 2;

                uint e4_index = e0_index + y_step;
                lookup_index |= FlipFirstTwoBits(info.edges[e4_index]) << 4;

                uint e6_index = e2_index + y_step;
                lookup_index |= info.edges[e6_index] << 6;

                // ===== Lookup Triangle Count ===== //

                uint triangle_count = info.triangle_lookup[lookup_index].triangle_index[15];
                triangle_sum += triangle_count;

                // ===== Lookup Intersection Count ===== //

                uint mc_edge_case = info.edge_case_lookup[lookup_index];
                uint intersection_count = (mc_edge_case & 1) + ((mc_edge_case >> 3) & 1) + ((mc_edge_case >> 8) & 1);
                intersection_sum += intersection_count;

                // == DEBUG == //

                if (lookup_index != 255) {
                        // printf("SHADER: Triangle Count: %u\n", triangle_count);
                        printf("SHADER: Lookup Index: %u\n", lookup_index);
                        // printf("SHADER: E0: %u E6: %u\n", e0_index, e6_index);
                }
        }

        // ===== Write to Output Buffers ===== //

        info.edge_sums[edge_sum_index].vertex_count = intersection_sum;
        info.edge_sums[edge_sum_index].vertex_offset = intersection_sum;
        info.edge_sums[edge_sum_index].triangle_count = triangle_sum * 3;
        info.edge_sums[edge_sum_index].triangle_offset = triangle_sum * 3;
}

enum EdgeOffset {
        _00,
        _01,
        _10,
        _11,
}

// Write Indices and Vertices
[shader("compute")]
[numthreads(64, 1, 1)]
void Pass3(uint3 dispatch_id : SV_DispatchThreadID) {
        uint3 coords = uint3(0, dispatch_id.x, dispatch_id.y);
        int edge_vertex_count = info.chunk_cells + 1;

        uint edge_index = (coords.z * edge_vertex_count) + (coords.y);

        // Used for writing to output buffer
        uint vertex_offset = info.edge_sums[edge_index].vertex_offset;
        uint index_offset = info.edge_sums[edge_index].triangle_offset;

        // Used for getting index for wanted vertex
        uint index_00_offset = info.edge_sums[edge_index].triangle_offset;
        uint index_10_offset = info.edge_sums[edge_index + 1].triangle_offset;
        uint index_01_offset = info.edge_sums[edge_index + edge_vertex_count].triangle_offset;
        uint index_11_offset = info.edge_sums[edge_index + edge_vertex_count + 1].triangle_offset;

        for (uint x = 0; x < info.chunk_cells; x++) {
                coords.x = x;

                // ===== Write Vertex Data ===== //

                // NOTE: Dont really like that im re doing this here
                uint mc_index = GetLookupIndex(edge_index);
                uint edge_bits = info.edge_case_lookup[mc_index];

                // ===== Generate Vertices for this points forward edges. (edges 0, 3, 8)
                // Only calculate those that are used

                float3 v0 = float3(coords);
                if ((edge_bits & (1 << 0)) > 0) {
                        float3 v1 = v0;
                        v1.x += 1.0;

                        info.vertices[vertex_offset].position.xyz = lerp(v0, v1, 0.5);
                        vertex_offset++;
                }

                if ((edge_bits & (1 << 3)) > 0) {
                        float3 v3 = v0;
                        v3.z += 1.0;

                        info.vertices[vertex_offset].position.xyz = lerp(v0, v3, 0.5);
                        vertex_offset++;
                }

                if ((edge_bits & (1 << 8)) > 0) {
                        float3 v4 = v0;
                        v4.y += 1.0;

                        info.vertices[vertex_offset].position.xyz = lerp(v0, v4, 0.5);
                        vertex_offset++;
                }

                // Vertex is theintepolated value between x edge, y edge and z edge
                // Triangles is lookup for current cell to get 3 verts
                //

                // ===== For index in triangle, get the actual index in indices, and set.

                TriangleLookup triangles = info.triangle_lookup[mc_index];

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


                EdgeOffset edge_offset[12] = {
                        EdgeOffset._00,
                        EdgeOffset._00,
                        EdgeOffset._01,
                        EdgeOffset._00,
                        EdgeOffset._10,
                        EdgeOffset._10,
                        EdgeOffset._11,
                        EdgeOffset._10,
                        EdgeOffset._00,
                        EdgeOffset._00,
                        EdgeOffset._01,
                        EdgeOffset._01,
                };

                // Do we need to count x and y
                uint2 do_count_lookup[12] = {
                        uint2(0, 0),
                        uint2(1, 1),
                        uint2(0, 0),
                        uint2(1, 1),
                        uint2(0, 0),
                        uint2(1, 1),
                        uint2(0, 0),
                        uint2(1, 1),
                        uint2(1, 0),
                        uint2(1, 0),
                        uint2(1, 0),
                        uint2(1, 0),
                };


                // NOTE: Need to keep track of current cell offset as well as +z +y +yz
                for (int i = 0; i < 15; i++) {
                        // Lookup MC table to get which edges make up the triangles.
                        uint local_index = triangles.triangle_index[i];
                        if (local_index == -1) break;

                        // Get the vertex which tracks this edge.
                        uint3 edge_coords = coords + edge_owner_lookup[local_index];
                        uint edge_start_index = (edge_coords.z * edge_vertex_count * edge_vertex_count) + (edge_coords.y * edge_vertex_count);

                        uint owning_edge_index = GetLookupIndex(edge_start_index + edge_coords.z);

                        uint owning_edge_case = info.edge_case_lookup[owning_edge_index];

                        // Check 0, 3, 8
                        // if ((owning_edge_case & (1 << 0)) > 0) {
                        //         if (local_index == 0) {
                        //                 info.indices[index_00_offset];
                        //         }
                        // }

                        // Just add one in each direction from this point, if any point more than 1 away from first vertex then
                        // we know we are out of cell

                        uint3 e0 = edge_coords;
                        e0.x += 1;

                        uint3 e1 = edge_coords;
                        e1.y += 1;

                        uint3 e2 = edge_coords;
                        e2.z += 1;

                        uint edge_offset_case = 0;

                        // Check offset - Flip as if offset is 1 we dont want to check it.
                        edge_offset_case |= ((edge_coords.x - coords.x) ^ 1) << 0; // x
                        edge_offset_case |= ((edge_coords.y - coords.y) ^ 1) << 1; // y
                        edge_offset_case |= ((edge_coords.z - coords.z) ^ 1) << 2; // z

                        // Check intersection
                        uint x_intersects = (owning_edge_case & (1 << 0));
                        uint y_intersects = (owning_edge_case & (1 << 8));
                        uint z_intersects = (owning_edge_case & (1 << 3));

                        edge_offset_case &= x_intersects << 0; // x
                        edge_offset_case &= y_intersects << 1; // y
                        edge_offset_case &= z_intersects << 2; // z

                        uint delta_x = edge_coords.x - coords.x;
                        uint delta_y = edge_coords.y - coords.y;
                        uint delta_z = edge_coords.z - coords.z;

                        uint offset = 0;
                        offset += (1 - delta_y) * (1 - delta_z) * index_00_offset;
                        offset += delta_y * (1 - delta_z) * index_10_offset;
                        offset += (1 - delta_y) * delta_z * index_01_offset;
                        offset += delta_y * delta_z * index_11_offset;

                        // These might be the same
                        uint edge_case_0 = GetLookupIndex(edge_start_index + coords.x);
                        uint edge_case_1 = GetLookupIndex(edge_start_index + edge_coords.x);

                        uint prev_inters = (edge_case_0 & (1 << 0)) + ((edge_case_0 & (1 << 8)) >> 8) + ((edge_case_0 & (1 << 3)) >> 3);

                        uint2 do_count = do_count_lookup[local_index];

                        uint x_inter = (edge_case_1 & (1 << 0)) >> 0;
                        uint y_inter = (edge_case_1 & (1 << 8)) >> 8;

                        uint final_index = (delta_x * (prev_inters)) + (x_inter * do_count.x) + (y_inter * do_count.y) + offset;

                        info.indices[index_offset] = final_index;
                        index_offset++;
                }

                uint edge_case_00 = GetLookupIndex(GetIndex(coords));
                uint index_00_count = (edge_case_00 & (1 << 0)) + ((edge_case_00 & (1 << 8)) >> 8) + ((edge_case_00 & (1 << 3)) >> 3);
                index_00_offset += index_00_count;

                uint edge_case_01 = GetLookupIndex(GetIndex(coords + uint3(0,0,1)));
                uint index_01_count = (edge_case_01 & (1 << 0)) + ((edge_case_01 & (1 << 8)) >> 8) + ((edge_case_01 & (1 << 3)) >> 3);
                index_01_offset += index_01_count;

                uint edge_case_10 = GetLookupIndex(GetIndex(coords + uint3(0,1,0)));
                uint index_10_count = (edge_case_10 & (1 << 0)) + ((edge_case_10 & (1 << 8)) >> 8) + ((edge_case_10 & (1 << 3)) >> 3);
                index_10_offset += index_10_count;

                uint edge_case_11 = GetLookupIndex(GetIndex(coords + uint3(0,1,1)));
                uint index_11_count = (edge_case_11 & (1 << 0)) + ((edge_case_11 & (1 << 8)) >> 8) + ((edge_case_11 & (1 << 3)) >> 3);
                index_11_offset += index_11_count;
        }

}


// Calculate Normals
[shader("compute")]
[numthreads(64, 1, 1)]
void Pass4(uint3 dispatch_id : SV_DispatchThreadID) {
        // ...
}