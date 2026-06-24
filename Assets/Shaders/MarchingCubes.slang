/*
Marching cubes using modified version of Flying Edges
*/

import Base;

// ===== Input Structs ===== //

struct EdgeSumInfo {
        uint vertex_offset;
        uint vertex_count;

        uint index_offset;
        uint index_count;

        void Print() {
                printf("SHADER - Edge Sums:\n");
                printf("    Vertex Offset: %u\n", vertex_offset);
                printf("    Vertex Count: %u\n", vertex_count);
                printf("    Triangle Offset: %u\n", index_offset);
                printf("    Triangle Count: %u\n", index_count);
        }
};

struct TriangleLookup {
        int triangle_index[16]; // Groups of 3 -> upto 5 triangles

        void Print() {
                printf("SHADER - Triangle Lookup:\n    %i, %i, %i, %i, %i, %i, %i, %i, %i, "
                       "%i, %i, %i, %i, %i, %i, %i\n",
                       triangle_index[0], triangle_index[1], triangle_index[2], triangle_index[3], triangle_index[4], triangle_index[5], triangle_index[6],
                       triangle_index[7], triangle_index[8], triangle_index[9], triangle_index[10], triangle_index[11], triangle_index[12], triangle_index[13],
                       triangle_index[14], triangle_index[15], );
        }
};

struct Vertex {
        float3 position;
        float3 normal;
        float2 uv;

        void Print() {
                printf("SHADER - Vertex:\n");
                printf("    Position:   %f, %f, %f\n", position.x, position.y, position.z);
                printf("    Normal:     %f, %f, %f\n", normal.x, normal.y, normal.z);
                printf("    uv:         %f, %f\n", uv.x, uv.y);
        }
};

struct PlanetInfo {
        // Input Arrays - For MC
        uint*           edge_case_lookup;
        TriangleLookup* triangle_lookup;

        // Intermediate Arrays
        uint*        edges;
        EdgeSumInfo* cell_sums;
        float*       triangle_normals;

        // Outputs
        uint*   indices;
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
                printf("    Edge Sums:          %p\n", cell_sums);
                printf("    Triangle Normals:   %p\n", triangle_normals);
                printf("    Indices:            %p\n", indices);
                printf("    Vertices:           %p\n", vertices);
                printf("    Chunk Cells:        %u\n", chunk_cells);
                printf("    Chunk Dims:         %u\n", chunk_dims);
                printf("    Position:           %f, %f, %f\n", position.x, position.y, position.z);
                printf("    Seed:               %u\n", seed);
        }
};

// ===== Input Variables ===== //

[vk::push_constant] PlanetInfo info;

// ===== Helper Functions ===== //

float GetScale() {
        return (float)info.chunk_dims / (float)info.chunk_cells;
}

float3 GetWorldPosition(float3 coords, float scale) {
        float3 local_position = coords * scale;
        return info.position + local_position;
}

// ===== Bit Helper Funcitons ===== //

uint FlipFirstTwoBits(uint v) {
        // uint x = ((v) ^ (v >> 1)) & 1;
        // return v ^ ((x) | (x << 1));

        uint res = 0;
        res |= (v & 1) << 1;
        res |= (v & 2) >> 1;
        return res;
}

uint CopyBit(uint v, int from, int to) {
        int shift_amount = to - from;

        if (shift_amount == 0) return v;

        v &= ~(1 << to);

        if (shift_amount < 0) {
                v |= (v & (1 << from)) >> abs(shift_amount);
        } else {
                v |= (v & (1 << from)) << shift_amount;
        }

        return v;
}

// ===== Density ===== //

// TODO: Implement
float GetDensity(float3 position) {
        return length(position) - 10000;
}

// ================== //
// ===== Passes ===== //
// ================== //

/*
Traverse all x edges and get case
*/
[[shader("compute")]] [[numthreads(64, 1, 1)]] [[require(SPV_KHR_non_semantic_info)]]
void Pass1(uint3 dispatch_id : SV_DispatchThreadID) {
        // ===== Make sure were in range ===== //

        if (dispatch_id.x > info.chunk_cells) return;

        // ============================== //
        // ===== Calc Common Values ===== //
        // ============================== //

        const float scale = GetScale();

        const int cell_count = info.chunk_cells;
        const int edge_count = cell_count + 1;

        const u32 edge_x_count = cell_count;
        const u32 edge_y_count = edge_count;
        const u32 edge_z_count = edge_count;

        const u32 edge_x_step = 1;
        const u32 edge_y_step = edge_x_count;
        const u32 edge_z_step = edge_x_count * edge_y_count;

        const u32 sum_x_count = 1;
        const u32 sum_y_count = cell_count;
        const u32 sum_z_count = cell_count;

        const u32 sum_x_step = 0;
        const u32 sum_y_step = sum_x_count;
        const u32 sum_z_step = sum_x_count * sum_y_count;

        uint3  coords         = uint3(0, dispatch_id.x, dispatch_id.y);
        float3 world_position = GetWorldPosition(float3(coords), scale);

        // ===== Calc Input Index ===== //

        const u32 first_edge_index = (coords.z * edge_z_step) + (coords.y * edge_y_step);
        const u32 first_sum_index  = (coords.z * sum_z_step) + (coords.y * sum_y_step);

        // ====================== //
        // ===== Start Pass ===== //
        // ====================== //

        // ===== Do First Check Out of Loop ===== //

        float density   = GetDensity(world_position);
        uint  is_inside = (density <= 0.0f);

        info.edges[first_edge_index] = is_inside << 1;

        // ===== Loop All X Edges ===== //

        float last_density = density;

        for (uint x = 1; x < edge_x_count; x++) {
                world_position.x += scale;
                density   = GetDensity(world_position);
                is_inside = (density <= 0.0f);

                info.edges[first_edge_index + x] = is_inside << 1;
                info.edges[first_edge_index + (x - 1)] |= is_inside << 0;

                // ===== Write Lerp Alpha straight away ===== //

                u32 alpha_bits = 0;
                if (abs(density) + abs(last_density) != 0) {
                        float alpha = abs(last_density) / (abs(density) + abs(last_density));
                        alpha_bits  = u32(1023 * alpha);
                }

                info.edges[first_edge_index + (x - 1)] |= (alpha_bits << 22);

                last_density = density;
        }

        // ===== Do Last Check Out of Loop ===== //

        world_position.x += scale;
        density   = GetDensity(world_position);
        is_inside = (density <= 0.0f);

        u32 last_edge_index = first_edge_index + (edge_x_count - 1);
        info.edges[last_edge_index] |= is_inside << 0;

        // ===== Write Lerp Alpha straight away ===== //

        u32 alpha_bits = 0;
        if ((abs(density) + abs(last_density)) != 0) {
                float alpha = abs(last_density) / (abs(density) + abs(last_density));
                alpha_bits  = u32(1023 * alpha);
        }

        info.edges[last_edge_index] |= (alpha_bits << 22);
}

/*
Loop all cells and get cell MC case
*/
[[shader("compute")]] [[require(SPV_KHR_non_semantic_info, spvGroupNonUniformArithmetic, spvGroupNonUniformBallot, )]] [[numthreads(64, 1, 1)]]
void Pass2(uint3 dispatch_id : SV_DispatchThreadID) {
        // ===== Make sure were in range ===== //

        if (dispatch_id.x >= info.chunk_cells) return;

        // ============================== //
        // ===== Calc Common Values ===== //
        // ============================== //

        const float scale = GetScale();

        const int cell_count = info.chunk_cells;
        const int edge_count = cell_count + 1;

        const u32 edge_x_count = cell_count;
        const u32 edge_y_count = edge_count;
        const u32 edge_z_count = edge_count;

        const u32 edge_x_step = 1;
        const u32 edge_y_step = edge_x_count;
        const u32 edge_z_step = edge_x_count * edge_y_count;

        const u32 sum_x_count = 1;
        const u32 sum_y_count = cell_count;
        const u32 sum_z_count = cell_count;

        const u32 sum_x_step = 0;
        const u32 sum_y_step = sum_x_count;
        const u32 sum_z_step = sum_x_count * sum_y_count;

        uint3  coords         = uint3(0, dispatch_id.x, dispatch_id.y);
        float3 world_position = GetWorldPosition(float3(coords), scale);

        // ===== Calc Input Index ===== //

        const u32 first_edge_index = (coords.z * edge_z_step) + (coords.y * edge_y_step);
        const u32 first_sum_index  = (coords.z * sum_z_step) + (coords.y * sum_y_step);

        // ====================== //
        // ===== Start Pass ===== //
        // ====================== //

        // ===== Work out which edge intersections we care about ===== //

        u32 bits_to_count = (1 << 0) + (1 << 3) + (1 << 8);

        // == If last in dimension need to check next as well == //

        if (coords.y == cell_count - 1) {
                bits_to_count |= (1 << 4) + (1 << 7);
        }

        if (coords.z == cell_count - 1) {
                bits_to_count |= (1 << 2) + (1 << 11);

                if (coords.y == cell_count - 1) {
                        bits_to_count |= (1 << 6);
                }
        }

        // ===== Count Indices + Vertices ===== //

        u32 index_sum  = 0;
        u32 vertex_sum = 0;

        for (u32 x = 0; x < cell_count; x++) {
                coords.x = x;

                u32 bits = bits_to_count;

                u32 index_count  = 0;
                u32 vertex_count = 0;

                // ===== Get Marching Cubes Lookup Index ===== //

                u32 lookup_index = 0;

                u32 edge_0_index = first_edge_index + x;
                lookup_index |= (FlipFirstTwoBits(info.edges[edge_0_index]) & 3) << 0;

                u32 edge_2_index = edge_0_index + edge_z_step;
                lookup_index |= (info.edges[edge_2_index] & 3) << 2;

                u32 edge_4_index = edge_0_index + edge_y_step;
                lookup_index |= (FlipFirstTwoBits(info.edges[edge_4_index]) & 3) << 4;

                u32 edge_6_index = edge_2_index + edge_y_step;
                lookup_index |= (info.edges[edge_6_index] & 3) << 6;

                // == Store For Future == //

                info.edges[edge_0_index] |= lookup_index << 2;

                // ===== Lookup Triangle Count ===== //

                uint triangle_count = info.triangle_lookup[lookup_index].triangle_index[15];
                index_count += triangle_count * 3;

                if (triangle_count > 0 && length(world_position) < 100)
                        printf("HERE AS WELL? %u, %u, %u, -- %uowned_vertex_bits\n", coords.x, coords.y, coords.z, lookup_index);

                // ===== Lookup Intersection Count ===== //

                u32 edge_case = info.edge_case_lookup[lookup_index];

                // ===== If last x, need to check extra ===== //

                if (coords.x == cell_count - 1) {
                        bits |= (1 << 1) + (1 << 9);

                        if (coords.y == cell_count - 1) {
                                bits |= (1 << 5);
                        }

                        if (coords.z == cell_count - 1) {
                                bits |= (1 << 10);
                        }
                }

                // == Count Intersections == //

                u32 counted_bits = edge_case & bits;
                vertex_count     = countbits(counted_bits);

                // ===== Store which bits we counted ===== //

                info.edges[edge_0_index] |= (counted_bits << 10);

                // ===== Add to Sum ===== //

                index_sum += index_count;
                vertex_sum += vertex_count;

                // ===== DEBUG ===== //

                if ((index_count > 15) || (vertex_count > 12)) {
                        printf("Invalid Index or Vertex Counts - Index Count: %u - Vertex Count: %u\n", index_count, vertex_count);
                }

                world_position.x += scale;
        }

        // ===== Write to Output Buffers ===== //

        info.cell_sums[first_sum_index].vertex_count  = vertex_sum;
        info.cell_sums[first_sum_index].vertex_offset = vertex_sum;
        info.cell_sums[first_sum_index].index_count   = index_sum;
        info.cell_sums[first_sum_index].index_offset  = index_sum;
}

// Assuming Max 256 chunk_cells and 32 lanes in a wave
// We already reduces by one dimension
groupshared uint group_vertex_offsets[256 / 32];
groupshared uint group_index_offsets[256 / 32];

[[shader("compute")]] [[require(SPV_KHR_non_semantic_info, spvGroupNonUniformBallot, spvGroupNonUniformArithmetic,
                                spvGroupNonUniformVote)]] [[numthreads(127, 1, 1)]]
void Pass3(uint3 dispatch_id : SV_DispatchThreadID, int3 group_thread_coord : SV_GroupThreadID, uint3 group_coord : SV_GroupID) {
        // ============================== //
        // ===== Calc Common Values ===== //
        // ============================== //

        const float scale = GetScale();

        const int cell_count = info.chunk_cells;
        const int edge_count = cell_count + 1;

        const u32 edge_x_count = cell_count;
        const u32 edge_y_count = edge_count;
        const u32 edge_z_count = edge_count;

        const u32 edge_x_step = 1;
        const u32 edge_y_step = edge_x_count;
        const u32 edge_z_step = edge_x_count * edge_y_count;

        const u32 sum_x_count = 1;
        const u32 sum_y_count = cell_count;
        const u32 sum_z_count = cell_count;

        const u32 sum_x_step = 0;
        const u32 sum_y_step = sum_x_count;
        const u32 sum_z_step = sum_x_count * sum_y_count;

        uint3  coords         = uint3(0, 0, dispatch_id.x);
        float3 world_position = GetWorldPosition(float3(coords), scale);

        // ===== Calc Input Index ===== //

        const u32 first_edge_index = (coords.z * edge_z_step) + (coords.y * edge_y_step);

        const u32 first_sum_index = (coords.z * sum_z_step) + (coords.y * sum_y_step);

        // ====================== //
        // ===== Start Pass ===== //
        // ====================== //

        // Not sure in specifics of group sync. So just to be safe we still run the sync on all threads.
        bool is_valid_index = dispatch_id.x < cell_count;

        // == Get Wave Values == //

        u32 wave_count = WaveGetNumWaves();

        u32 lane_count = WaveGetLaneCount();
        u32 lane_index = WaveGetLaneIndex();
        u32 wave_index = group_thread_coord.x / lane_count;


        // == Initialise groupshared memory == //

        if (WaveIsFirstLane()) {
                group_vertex_offsets[wave_index] = 0;
                group_index_offsets[wave_index]  = 0;
        }

        GroupMemoryBarrierWithGroupSync();

        const u32 last_sum_index = first_sum_index + (sum_y_count - 1);

        if (is_valid_index) {
                // ===== Sum Lane Offsets ===== //

                for (u32 y = 1; y < cell_count; y++) {
                        u32 index = first_sum_index + y;

                        // == Add Previous == //

                        info.cell_sums[index].vertex_offset += info.cell_sums[index - 1].vertex_offset;
                        info.cell_sums[index].index_offset += info.cell_sums[index - 1].index_offset;
                }

                // == Get Lane Count == //

                u32 lane_vertex_count = info.cell_sums[last_sum_index].vertex_offset;
                u32 lane_index_count  = info.cell_sums[last_sum_index].index_offset;

                // == Get Wave Offset == //

                u32 wave_vertex_offset = WavePrefixSum(lane_vertex_count);
                u32 wave_index_offset  = WavePrefixSum(lane_index_count);


                // ===== Add Wave Offset to Lane Count ===== //

                for (u32 y = 0; y < cell_count; y++) {
                        u32 index = first_sum_index + y;

                        // == Add Wave Offset == //

                        info.cell_sums[index].vertex_offset += wave_vertex_offset;
                        info.cell_sums[index].index_offset += wave_index_offset;
                }

                // == Make sure were only writting one value from each wave if we have a value to write == //

                // ===== Get Wave Count ===== //

                u32 wave_vertex_count = WaveActiveMax(info.cell_sums[last_sum_index].vertex_offset);
                u32 wave_index_count  = WaveActiveMax(info.cell_sums[last_sum_index].index_offset);

                // && wave_index_count > 0
                if (WaveIsFirstLane()) {

                        // ===== Add This Waves Sum to Each Wave Ahead ===== //

                        for (u32 next_wave_index = wave_index + 1; next_wave_index < wave_count; next_wave_index++) {
                                InterlockedAdd(group_vertex_offsets[next_wave_index], wave_vertex_count);
                                InterlockedAdd(group_index_offsets[next_wave_index], wave_index_count);
                        }
                }
        }

        GroupMemoryBarrierWithGroupSync();

        if (is_valid_index) {
                u32 group_vertex_offset = group_vertex_offsets[wave_index];
                u32 group_index_offset  = group_index_offsets[wave_index];

                // if (wave_index == 0) printf("Group Vertex: %u - Index: %u\n", group_vertex_offset, group_index_offset);

                // ===== Add Group Offsets To Wave + Thread Offsets ===== //

                for (u32 y = 0; y < cell_count; y++) {
                        u32 index = first_sum_index + y;

                        info.cell_sums[index].vertex_offset += group_vertex_offset;
                        info.cell_sums[index].index_offset += group_index_offset;
                }

                // // ===== DEBUG ===== //
                // u32 group_vertex_count = WaveActiveMax(info.cell_sums[last_sum_index].vertex_offset);
                // u32 group_index_count  = WaveActiveMax(info.cell_sums[last_sum_index].index_offset);

                // if (WaveIsFirstLane()) {
                //         printf("Vertex: %u, Index: %u, Wave Index: %u\n", group_vertex_count, group_index_count, wave_index);
                // }

                // const u32 max_possible_index_count = 31457280;
                // if (group_index_count > max_possible_index_count) {
                //         printf("[SHADER] Index Count is higher than possible - Index Count: %u - Vertex Count: %u\n", group_index_count, group_vertex_count);
                // }
                // // === END DUBUG === //
        }
}

u32 Bit(u32 b) {
        return 1 << b;
}

// Write Indices and Vertices
[[shader("compute")]] [[require(spirv, SPV_KHR_non_semantic_info, spvGroupNonUniformBallot)]] [[numthreads(64, 1, 1)]]
void Pass4(uint3 dispatch_id : SV_DispatchThreadID, int3 group_thread_coord : SV_GroupThreadID, uint3 group_coord : SV_GroupID) {
        if (dispatch_id.x >= info.chunk_cells) return;

        // ============================== //
        // ===== Calc Common Values ===== //
        // ============================== //

        const float scale = GetScale();

        const int cell_count = info.chunk_cells;
        const int edge_count = cell_count + 1;

        const u32 edge_x_count = cell_count;
        const u32 edge_y_count = edge_count;
        const u32 edge_z_count = edge_count;

        const u32 edge_x_step = 1;
        const u32 edge_y_step = edge_x_count;
        const u32 edge_z_step = edge_x_count * edge_y_count;

        const u32 sum_x_count = 1;
        const u32 sum_y_count = cell_count;
        const u32 sum_z_count = cell_count;

        const u32 sum_x_step = 0;
        const u32 sum_y_step = sum_x_count;
        const u32 sum_z_step = sum_x_count * sum_y_count;

        uint3  coords         = uint3(0, dispatch_id.x, dispatch_id.y);
        float3 world_position = GetWorldPosition(float3(coords), scale);

        // ===== Calc Input Index ===== //

        const u32 first_edge_index = (coords.z * edge_z_step) + (coords.y * edge_y_step);
        const u32 first_sum_index  = (coords.z * sum_z_step) + (coords.y * sum_y_step);

        // ====================== //
        // ===== Start Pass ===== //
        // ====================== //

        // == Get Wave Values == //

        u32 wave_count = WaveGetNumWaves();

        u32 lane_index = WaveGetLaneIndex();
        u32 lane_count = WaveGetLaneCount();
        u32 wave_index = group_thread_coord.x / lane_count;

        // printf("Groups: %u, Waves: %u, Lanes : %u, GroupID: %u, WaveID: %u\n", group_count, wave_count, lane_count, group_index, wave_index);

        // ===== Get Previous Rows Counts as Offsets ===== //

        u32 first_vertex_offset = 0;
        u32 first_index_offset  = 0;

        // == Check there is a previous to get == //

        if (first_sum_index != 0) {
                first_vertex_offset = info.cell_sums[first_sum_index - 1].vertex_offset;
                first_index_offset  = info.cell_sums[first_sum_index - 1].index_offset;
        }

        // ===== Helper Arrays ===== //

        const u32 vertex_owning_edge[12] = {
                0, // 0
                1, // 1
                3, // 2
                0, // 3
                4, // 4
                5, // 5
                7, // 6
                4, // 7
                0, // 8
                1, // 9
                2, // 10
                3, // 11
        };

        u32 vertex_edge_index[8] = {
                first_edge_index,
                first_edge_index + edge_x_step,
                first_edge_index + edge_x_step + edge_z_step,
                first_edge_index + edge_z_step,
                first_edge_index + edge_y_step,
                first_edge_index + edge_x_step + edge_y_step,
                first_edge_index + edge_x_step + edge_y_step + edge_z_step,
                first_edge_index + edge_y_step + edge_z_step,
        };

        u32 vertex_sum_index[8] = {
                first_sum_index,
                first_sum_index + sum_x_step,
                first_sum_index + sum_x_step + sum_z_step,
                first_sum_index + sum_z_step,
                first_sum_index + sum_y_step,
                first_sum_index + sum_x_step + sum_y_step,
                first_sum_index + sum_x_step + sum_y_step + sum_z_step,
                first_sum_index + sum_y_step + sum_z_step,
        };

        u32 vertex_cell_vertex_offset[8] = {
                first_vertex_offset,
                first_vertex_offset,

                info.cell_sums[vertex_sum_index[2] - 1].vertex_offset,
                info.cell_sums[vertex_sum_index[3] - 1].vertex_offset,

                info.cell_sums[vertex_sum_index[4] - 1].vertex_offset,
                info.cell_sums[vertex_sum_index[5] - 1].vertex_offset,

                info.cell_sums[vertex_sum_index[6] - 1].vertex_offset,
                info.cell_sums[vertex_sum_index[7] - 1].vertex_offset,
        };

        uint2 edge_connecting_vertices[12] = {
                { 0, 1 }, // 0
                { 1, 2 }, // 1
                { 3, 2 }, // 2
                { 0, 3 }, // 3
                { 4, 5 }, // 4
                { 5, 6 }, // 5
                { 7, 6 }, // 6
                { 4, 7 }, // 7
                { 0, 4 }, // 8
                { 1, 5 }, // 9
                { 2, 6 }, // 10
                { 3, 7 }, // 11
        };

        float3 vertex_relative_offset[8] = {
                { 0, 0, 0 },     { scale, 0, 0 },     { scale, 0, scale },     { 0, 0, scale },
                { 0, scale, 0 }, { scale, scale, 0 }, { scale, scale, scale }, { 0, scale, scale },
        };

        u32 vertex_relative_edge_lookup[8][12] = {
                {}, // 0 - Wont need to ask
                {
                        0,  // 0
                        3,  // 1 -
                        0,  // 2
                        0,  // 3
                        0,  // 4
                        7,  // 5 -
                        0,  // 6
                        0,  // 7
                        0,  // 8
                        8,  // 9 -
                        11, // 10 -
                        0,  // 11
                },          // 1
                {
                        0, // 0
                        0, // 1
                        0, // 2
                        0, // 3
                        0, // 4
                        0, // 5
                        0, // 6
                        0, // 7
                        0, // 8
                        0, // 9
                        8, // 10 -
                        0, // 11
                },         // 2
                {
                        0, // 0
                        0, // 1
                        0, // 2 -
                        0, // 3
                        0, // 4
                        0, // 5
                        4, // 6 -
                        0, // 7
                        0, // 8
                        0, // 9
                        9, // 10 -
                        8, // 11 -
                },         // 3
                {
                        0, // 0
                        0, // 1
                        0, // 2
                        0, // 3
                        0, // 4 -
                        1, // 5 -
                        2, // 6 -
                        3, // 7 -
                        0, // 8
                        0, // 9
                        0, // 10
                        0, // 11
                },         // 4
                {
                        0, // 0
                        0, // 1
                        0, // 2
                        0, // 3
                        0, // 4
                        3, // 5 -
                        0, // 6
                        0, // 7
                        0, // 8
                        0, // 9
                        0, // 10
                        0, // 11
                },         // 5
                {},        // 6 - Never owner
                {
                        0, // 0
                        0, // 1
                        0, // 2
                        0, // 3
                        0, // 4
                        0, // 5
                        0, // 6 -
                        0, // 7
                        0, // 8
                        0, // 9
                        0, // 10
                        0, // 11
                },         // 7
        };

        u32 current_vertex_offset = first_vertex_offset;
        u32 current_index_offset  = first_index_offset;

        for (u32 x = 0; x < cell_count; x++) {
                coords.x = x;

                u32 owned_bits_mask = ((1 << 12) - 1);

                u32 edge_value = info.edges[vertex_edge_index[0]];

                u32 lookup_index      = (edge_value >> 2) & 255;
                u32 owned_vertex_bits = (edge_value >> 10) & owned_bits_mask;

                // ===== Lookup Triangles ===== //

                u32 triangle_bits = info.edge_case_lookup[lookup_index];

                // == Seperate which triangle vertices this cell doesnt own == //

                u32 unowned_vertex_bits = ((~owned_vertex_bits) & triangle_bits);

                // ===== DEBUG ===== //
                u32 expected_vertex_count = countbits(owned_vertex_bits);
                u32 actual_vertex_count   = 0;
                // == //

                i32 edge_vertex_index[12] = {};

                u32 first_vertex_ind = current_vertex_offset;

                for (u32 edge_id = 0; edge_id < 12; edge_id++) {
                        u32 edge_bit = 1 << edge_id;

                        // == Default to -1 == //

                        edge_vertex_index[edge_id] = -2;

                        // == Check if Edge is in triangle == //

                        if ((owned_vertex_bits & edge_bit) == edge_bit) {
                                // == This cell owns the vertex == //

                                uint2 edge_vertices = edge_connecting_vertices[edge_id];

                                u32 first_edge_vertex  = edge_vertices[0];
                                u32 second_edge_vertex = edge_vertices[1];

                                float3 first_vertex  = world_position + (vertex_relative_offset[first_edge_vertex]);
                                float3 second_vertex = world_position + (vertex_relative_offset[second_edge_vertex]);

                                float alpha = float(edge_value >> 22) / 1023.0f; // TODO

                                float3 vertex_position = lerp(first_vertex, second_vertex, alpha);

                                u32 vertex_ind = first_vertex_ind + countbits((owned_vertex_bits & (edge_bit - 1)));

                                info.vertices[vertex_ind].position = vertex_position;
                                // info.vertices[current_vertex_offset].position = float3(1);

                                // Store

                                // edge_vertex_index[edge_id] = 0;
                                edge_vertex_index[edge_id] = (i32)vertex_ind;
                                current_vertex_offset++;

                                // ===== DEBUG ===== //
                                actual_vertex_count++;
                                // == //

                        } else if ((unowned_vertex_bits & edge_bit) == edge_bit) {
                                // == Another cell owns this vertex == //

                                // == Find Which Cell Owns It == //

                                u32 owning_vertex = vertex_owning_edge[edge_id];

                                u32 x_mask = Bit(1) + Bit(2) + Bit(5) + Bit(6);
                                if (coords.x == cell_count - 1 && ((Bit(owning_vertex) & x_mask) > 0)) {
                                        if (edge_id == 10) owning_vertex = 3;
                                        else if (edge_id == 5) owning_vertex = 4;
                                }

                                u32 y_mask = Bit(4) + Bit(5) + Bit(6) + Bit(7);
                                if (coords.y == cell_count - 1 && ((Bit(owning_vertex) & y_mask) > 0)) {
                                        if (edge_id == 5) owning_vertex = 1;
                                        else if (edge_id == 6) owning_vertex = 3;
                                }

                                u32 z_mask = Bit(2) + Bit(3) + Bit(6) + Bit(7);
                                if (coords.z == cell_count - 1 && ((Bit(owning_vertex) & z_mask) > 0)) {
                                        if (edge_id == 6) owning_vertex = 4;
                                        else if (edge_id == 10) owning_vertex = 1;
                                }

                                // == Get Cells Offset == //

                                u32 owning_vertex_edge_index = vertex_edge_index[owning_vertex];
                                u32 owning_vertex_offset     = vertex_cell_vertex_offset[owning_vertex];

                                // == if +x need to add bits of cell before == //

                                if ((Bit(owning_vertex) & x_mask) > 0) {
                                        u32 previous_bits_counted = (info.edges[owning_vertex_edge_index - 1] >> 10) & owned_bits_mask;
                                        owning_vertex_offset += countbits(previous_bits_counted);
                                }

                                // == Add this edges offset == //


                                // ===== NOTE: EDGE IS NOT THE SAME HERE ===== //

                                u32 relative_edge     = vertex_relative_edge_lookup[owning_vertex][edge_id];
                                u32 relative_edge_bit = Bit(relative_edge);

                                u32 owning_bits_counted = (info.edges[owning_vertex_edge_index] >> 10) & owned_bits_mask;
                                owning_bits_counted     = owning_bits_counted & (relative_edge_bit - 1);
                                owning_vertex_offset += countbits(owning_bits_counted);

                                // == Add to Indices == //

                                edge_vertex_index[edge_id] = owning_vertex_offset;
                                // edge_vertex_index[edge_id] = 0;
                        }
                }

                // ===== DEBUG ===== //
                if (actual_vertex_count != expected_vertex_count)
                        printf("[SHADER] Got unexpected vertex count: %u, - Expected %u\n", actual_vertex_count, expected_vertex_count);

                u32 expected_index_count = info.triangle_lookup[lookup_index].triangle_index[15] * 3;
                u32 actual_index_count   = 0;
                // == //

                for (u32 i = 0; i < 15; i++) {
                        i32 edge_id = info.triangle_lookup[lookup_index].triangle_index[i];
                        if (edge_id == -1) break;

                        info.indices[current_index_offset] = edge_vertex_index[edge_id];
                        current_index_offset++;

                        // ===== DEBUG ===== //
                        if (edge_vertex_index[edge_id] == -1) printf("[SHADER] Got invalid vertex index: %u\n", edge_vertex_index[edge_id]);
                        actual_index_count++;
                        // == //
                }



                // ===== DEBUG ===== //
                if (actual_index_count != expected_index_count)
                        printf("[SHADER] Got unexpected index count: %u, - Expected %u\n", actual_index_count, expected_index_count);
                // == //


                for (u32 i = 0; i < 4; i++) {
                        u32 vertex_index = i * 2;

                        // == Get Cells Offset == //

                        u32 edge_index          = vertex_edge_index[vertex_index + (i % 2)];
                        u32 owning_bits_counted = (info.edges[edge_index] >> 10) & owned_bits_mask;

                        u32 vertex_count = countbits(owning_bits_counted);

                        vertex_cell_vertex_offset[vertex_index] += vertex_count;
                        vertex_cell_vertex_offset[vertex_index + 1] += vertex_count;

                        // == Update Edge Index == //

                        vertex_edge_index[vertex_index] += 1;
                        vertex_edge_index[vertex_index + 1] += 1;
                }

                if (current_vertex_offset != vertex_cell_vertex_offset[0]) {
                        printf("[SHADER] Unmatched - Current: %u - Expected: %u\n", current_vertex_offset, vertex_cell_vertex_offset[0]);
                }

                world_position.x += scale;
        }

        if (current_vertex_offset != info.cell_sums[first_sum_index].vertex_offset)
                printf("[SHADER] Not adding up : %u - %u\n", current_vertex_offset, info.cell_sums[first_sum_index].vertex_offset);
}

// Calculate Normals
[shader("compute")][numthreads(64, 1, 1)] void Pass5(uint3 dispatch_id : SV_DispatchThreadID) {
        // ...
}