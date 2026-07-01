/*
Marching cubes using modified version of Flying Edges
*/

import Base;
import Noise;

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

struct Normals {
        float3[4] normals;
}

struct PlanetInfo {

        // Input Arrays - For MC
        uint*           edge_case_lookup;
        TriangleLookup* triangle_lookup;

        // Intermediate Arrays
        uint*        edges;
        EdgeSumInfo* cell_sums;
        Normals*     normals;

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
                printf("    Triangle Normals:   %p\n", normals);
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
        float val = length(position) - 10000.0;

        float offset = 0;

        float mult = Perlin(position / 2000.0);

        offset += Perlin(position / 498.0) * 180.6 * mult;
        offset += Perlin(position / 326.0) * 112.3 * mult;
        offset += Perlin(position / 205.0) * 100.5 * mult;
        offset += Perlin(position / 303.0) * 78.4;
        offset += Perlin(position / 99.4) * 20.3;
        offset += Perlin(position / 42.0) * 5.8;
        offset += Perlin(position / 11.0) * 1.1;
        offset += Perlin(position * 1.8) * 0.08;
        offset += Perlin(position * 8.0) * 0.05;
        offset += Perlin(position * 104.0) * 0.01;

        float a = smoothstep(0.0, 1.0, (100.0 - offset) / 30.0);

        offset = offset * (1.0 - a) + a * (70 + VoronoiNoise(position.xz / 50.0) * 4.0);

        return val - offset;
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

        for (uint x = 1; x < edge_x_count; x++) {
                world_position.x += scale;
                density   = GetDensity(world_position);
                is_inside = (density <= 0.0f);

                info.edges[first_edge_index + x] = is_inside << 1;
                info.edges[first_edge_index + (x - 1)] |= is_inside << 0;
        }

        // ===== Do Last Check Out of Loop ===== //

        world_position.x += scale;
        density   = GetDensity(world_position);
        is_inside = (density <= 0.0f);

        u32 last_edge_index = first_edge_index + (edge_x_count - 1);
        info.edges[last_edge_index] |= is_inside << 0;
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
void Pass4(uint3 dispatch_id : SV_DispatchThreadID) {
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

                        edge_vertex_index[edge_id] = -1;

                        // == Check if Edge is in triangle == //

                        if ((owned_vertex_bits & edge_bit) == edge_bit) {
                                // == This cell owns the vertex == //

                                uint2 edge_vertices = edge_connecting_vertices[edge_id];

                                u32 first_edge_vertex  = edge_vertices[0];
                                u32 second_edge_vertex = edge_vertices[1];

                                float3 first_vertex  = world_position + (vertex_relative_offset[first_edge_vertex]);
                                float3 second_vertex = world_position + (vertex_relative_offset[second_edge_vertex]);

                                float a_den = GetDensity(first_vertex);
                                float b_den = GetDensity(second_vertex);

                                float  alpha           = (-a_den) / (b_den - a_den);
                                float3 vertex_position = lerp(first_vertex, second_vertex, alpha);

                                u32 vertex_ind = first_vertex_ind + countbits((owned_vertex_bits & (edge_bit - 1)));

                                info.vertices[vertex_ind].position = vertex_position;
                                info.vertices[vertex_ind].normal   = normalize(vertex_position);
                                info.vertices[vertex_ind].uv       = {
                                        (atan2(vertex_position.x, vertex_position.z) / (2 * float.getPi())) + 0.5,
                                        (asin(vertex_position.y) / float.getPi()) + 0.5,
                                };
                                // info.vertices[current_vertex_offset].position = float3(1);

                                // Store
                                info.normals[vertex_ind].normals[0] = {};
                                info.normals[vertex_ind].normals[1] = {};
                                info.normals[vertex_ind].normals[2] = {};
                                info.normals[vertex_ind].normals[3] = {};

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

                for (u32 i = 0; i < 15; i += 3) {
                        i32 edge_id0 = info.triangle_lookup[lookup_index].triangle_index[i + 0];
                        i32 edge_id1 = info.triangle_lookup[lookup_index].triangle_index[i + 1];
                        i32 edge_id2 = info.triangle_lookup[lookup_index].triangle_index[i + 2];

                        if (edge_id0 == -1) break;

                        info.indices[current_index_offset + 0] = edge_vertex_index[edge_id0];
                        info.indices[current_index_offset + 1] = edge_vertex_index[edge_id2]; // Flip
                        info.indices[current_index_offset + 2] = edge_vertex_index[edge_id1];

                        current_index_offset += 3;

                        actual_index_count += 3;
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
[[shader("compute")]] [[require(spvGroupNonUniformBallot, SPV_KHR_non_semantic_info)]] [[numthreads(64, 1, 1)]] void
Pass5(uint3 dispatch_id : SV_DispatchThreadID, uint group_index : SV_GroupIndex) {
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

        u32 start_offset = 0;
        u32 end_offset   = info.cell_sums[first_sum_index].index_offset;

        if (first_sum_index != 0) {
                start_offset = info.cell_sums[first_sum_index - 1].index_offset;
        }

        for (u32 x = 0; x < cell_count; x++) {
                u32 edge_index = first_edge_index + x;

                u32 edge_value   = info.edges[edge_index];
                u32 lookup_index = (edge_value >> 2) & 255;

                for (u32 i = 0; i < 15; i += 3) {
                        i32 edge_id0 = info.triangle_lookup[lookup_index].triangle_index[i + 0];
                        if (edge_id0 == -1) break;
                        i32 edge_id1 = info.triangle_lookup[lookup_index].triangle_index[i + 2]; // Flipped for winding order
                        i32 edge_id2 = info.triangle_lookup[lookup_index].triangle_index[i + 1];

                        u32 i0 = info.indices[start_offset + 0];
                        u32 i1 = info.indices[start_offset + 1];
                        u32 i2 = info.indices[start_offset + 2];

                        float3 v0 = info.vertices[i0].position;
                        float3 v1 = info.vertices[i1].position;
                        float3 v2 = info.vertices[i2].position;

                        float3 e0 = normalize(v2 - v0);
                        float3 e1 = normalize(v1 - v0);

                        if (abs(dot(e0, e1)) >= 0.9999f) continue;
                        float3 normal = cross(e0, e1);

                        u32 normal_index0 = edge_id0 >> 1;
                        if (normal_index0 > 3) normal_index0 = edge_id0 - 8;
                        info.normals[i0].normals[normal_index0] = normal;

                        u32 normal_index1 = edge_id1 >> 1;
                        if (normal_index1 > 3) normal_index1 = edge_id1 - 8;
                        info.normals[i1].normals[normal_index1] = normal;

                        u32 normal_index2 = edge_id2 >> 1;
                        if (normal_index2 > 3) normal_index2 = edge_id2 - 8;
                        info.normals[i2].normals[normal_index2] = normal;

                        start_offset += 3;

                        if (normal_index0 > 3 || normal_index1 > 3 || normal_index2 > 3) printf("[SHADER] ERROR");
                }
        }
}

[[shader("compute")]] [[require(spvGroupNonUniformBallot, SPV_KHR_non_semantic_info)]] [[numthreads(64, 1, 1)]] void
Pass6(uint3 dispatch_id : SV_DispatchThreadID, uint group_index : SV_GroupIndex) {
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

        // NOTE: Not actual waves and lanes, just combining all for a chunk

        u32 dispatch_count  = cell_count * (cell_count / 4);
        u32 vertex_count    = info.cell_sums[(cell_count * cell_count) - 1].vertex_offset;
        u32 lane_work_count = (vertex_count + (dispatch_count - 1)) / dispatch_count;

        u32 lane_index = coords.y + coords.z * cell_count;

        u32 start_index = lane_work_count * lane_index;
        // printf("Vertex: %u\n", vertex_count);

        for (u32 i = 0; i < lane_work_count; i++) {
                u32 index = start_index + i;
                if (index > vertex_count - 1) return;

                float3 normal = {};

                for (u32 n = 0; n < 4; n++) {
                        normal += info.normals[index].normals[n];
                }

                if (dot(normal, normal) < 0.00001f) continue;
                info.vertices[index].normal = normalize(normal);
        }
}

struct IndirectData {
        u32 indexCount;
        u32 instanceCount;
        u32 firstIndex;
        i32 vertexOffset;
        u32 firstInstance;
}

struct GrassInputData {

        uint*   indices;
        Vertex* vertices;

        u32 index_count;
        u32 vertex_count;

        float3* points;
        u32*    point_count;

        IndirectData* indirect_draws; // Write grass data here
};

[vk::push_constant] GrassInputData grass_info;

[[shader("compute")]] [[require(spvGroupNonUniformBallot, SPV_KHR_non_semantic_info)]] [[numthreads(256, 1, 1)]] void
Grass(uint3 dispatch_id : SV_DispatchThreadID) {
        u32 thread_count = 256;

        u32 per_thread_work_count = (grass_info.index_count + (thread_count - 1)) / thread_count;

        u32 start_index = dispatch_id.x * per_thread_work_count;

        for (u32 i = 0; i < per_thread_work_count; i += 3) {
                u32 index = start_index + i;
                if (index >= grass_info.index_count) return;

                float3 v0 = grass_info.vertices[grass_info.indices[index + 0]].position;
                float3 v1 = grass_info.vertices[grass_info.indices[index + 1]].position;
                float3 v2 = grass_info.vertices[grass_info.indices[index + 2]].position;

                float3 e0 = v1 - v0;
                float3 e1 = v2 - v0;

                float area = length(cross(e0, e1)) / 2.0f;

                printf("Area: %f\n", area);

                u32 points_to_gen = 4;
                u32 write_index;
                InterlockedAdd(*grass_info.point_count, points_to_gen, write_index);

                for (u32 n = 0; n < points_to_gen; n++) {
                        float u = 0.33f * float(n / 2);
                        float v = 0.33f * float(n % 2);

                        float alpha = 1 - u - v;
                        float beta  = u;
                        float gamma = v;

                        float3 pos = v0 * alpha + v1 * beta + v2 * gamma;

                        grass_info.points[write_index] = pos;
                        write_index++;
                }
        }
}