/*
NOTE:
- Implement decent noise functions in a seperate file to include.
- Use includes well, this will make my life VERY easy.
*/


struct FieldData {
        float density;
};

struct TriangleData {
        uint traingles[15];
};

// Push constant - Max 256 Bytes
struct PlanetCreationData {
        // Creation Info
        uint seed; // 0 + 4 = 4
        float3 position; // 4 + 12 = 16 -----
        uint chunk_cells; // 16 + 4 = 20
        uint chunk_dims; // 20 + 4 = 24

        // Input Arrays
        uint* edge_table; // 24 + 8 = 32 -----
        TriangleData* triangle_table; // 32 + 8 = 40

        // Output Arrays
        float* density_array; // 40 + 8 = 48 -----
        uint* triangle_offset_array; // 48 + 8 = 56
        FieldData* triangle_array; // 56 + 8 = 64 -----

        Edge* edges; // 64 + 8 = 72

};

// NOTE:
/*
Noise functions from:
https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-37-efficient-random-number-generation-and-application
*/

uint TausStep(uint z, uint s1, uint s2, int s3, uint M) {
        uint b = (((z << s1) ^ z) >> s2);
        return (((z & M) << s3) ^ b);
}

uint LCGStep(uint z, uint a, uint c) {
        return (a * z + c);
}

float Noise(float3 position, uint seed) {
        uint v1 = uint(position.x * seed);
        uint v2 = uint(position.y * seed * 0.14382f);
        uint v3 = uint(position.z * seed * 0.73930f);

        uint v4 = v1 * 23508427 + v2 * 84311879 * v3 * 52282817;

        return 2.3283064365387e-10 * float(TausStep(v1, 13, 19, 12, 4294967294u) ^ TausStep(v2, 2, 25, 4, 4294967288u) ^
                                           TausStep(v3, 3, 11, 17, 4294967280u) ^ LCGStep(v4, 1664525, 1013904223));
}

float GenerateDensity(float3 position, uint seed) {
        float radius  = 1000;
        float density = radius - length(position - float3(0, -radius, 0));

        density += Noise(position * 1.01, seed) * 1.00f;
        density += Noise(position * 1.96, seed) * 0.5f;
        density += Noise(position * 3.91, seed + 1) * 0.25f;
        density += Noise(position * 8.03, seed + 1) * 0.125f;
        density += Noise(position * 16.23, seed + 2) * 0.0625f;
        density += Noise(position * 31.99, seed + 2) * 0.03125;

        return density;
}

float3 GetEdgeVertex(float3 a, float3 b, float a_density, float b_density) {

}

/*
NOTE:
- Implement Marching Cubes to start
- Look into - Flying Edges: A High-Performance Scalable Isocontouring Algorithm
        - Supposidly faster method than MC but seems like they only ran on CPU.
        - Need to compare with MC
        - They mention that the computational trimming done might not be worth for a GPU implementation.
*/

// Can we do 8x8x1 and do rows first, can do density calc and iso check in one.
// Can do in one shader stage -> Density, Iso, Tri Count
// Just need to note, can do the entire row(x) as we need to calc density for first time, Then when doing y and z need to do all in thread group
// Plus one to connect to the next thread group, but as no guarantee that other thread group has finished we can just calc the density (bit of wasted compute,
// but means were not having to have 3 seperate dispatches to do the same work).
// This is only actually useful if I plan on generating bounding box for actual needed cells.
// Is this worth it on GPU?
[shader("compute")]
[numthreads(4, 4, 4)]
void main(uniform PlanetCreationData planet_creation_data, uint3 dispatch_id : SV_DispatchThreadID) {
        uint  thread_count = 4;
        uint output_dims  = planet_creation_data.chunk_cells + 1;

        // As we have CellCount + 1 values, we cant nicely divide the last row and column.
        if (dispatch_id.x >= output_dims || dispatch_id.y >= output_dims || dispatch_id.z >= output_dims) {
                return;
        }

        // ===== Calculate the Ouput Index ===== //
        uint index = dispatch_id.x + dispatch_id.y * output_dims + dispatch_id.z * output_dims * output_dims;

        // ===== Get chunk positions ===== //

        float3 relative_position = (float3(dispatch_id) / planet_creation_data.chunk_cells) * planet_creation_data.chunk_dims;
        float3 position          = planet_creation_data.position + relative_position;

        // ===== Output ===== //

        planet_creation_data.density_array[index] = GenerateDensity(position, planet_creation_data.seed);
}


[shader("compute")]
[numthreads(4, 4, 4)]
void TriangleCount(uniform PlanetCreationData info, uint3 dispatch_id : SV_DispatchThreadID) {
        // TODO: Count triangles needed for each cell and return
        // CPU Needs to then sum and create buffer for triangles, plus provide offsets for next stage.

        uint  thread_count = 4;
        uint output_dims  = info.chunk_cells;

        // Do we assume we area always in bounds?
        if (dispatch_id.x >= output_dims || dispatch_id.y >= output_dims || dispatch_id.z >= output_dims) {
                return;
        }

        // ===== Calculate the Ouput Index ===== //
        uint index = dispatch_id.x + dispatch_id.y * output_dims + dispatch_id.z * output_dims * output_dims;

        // ===== Get Indices for density ===== //

        uint output_dims_squared = output_dims * output_dims;

        uint cell_vertex_indices[8];

        cell_vertex_indices[0] = index;
        cell_vertex_indices[1] = index + 1;
        cell_vertex_indices[2] = index + 1 + output_dims_squared;
        cell_vertex_indices[3] = index + output_dims_squared;
        cell_vertex_indices[4] = index + output_dims;
        cell_vertex_indices[5] = index + output_dims + 1;
        cell_vertex_indices[6] = index + output_dims + 1 + output_dims_squared;
        cell_vertex_indices[7] = index + output_dims + output_dims_squared;

        // ===== Get Cell Vertices Density ===== //
        uint cube_index = 0;

        for (uint i = 0; i < 8; i++) {
                // Why do we use i as index? This is defo wrong
                if (info.density_array[i] < 0.0f) cube_index |= (1 << i);
        }

        if (info.edge_table[cube_index] == 0) {
                return;
        }

        // ===== v0 Position ===== //
        float3 relative_position = (float3(dispatch_id) / info.chunk_cells) * info.chunk_dims;
        float3 position          = info.position + relative_position;
        float cell_width = info.chunk_dims / info.chunk_cells;

        float3 positions[8] = {
                position,
                position + float3(cell_width, 0, 0),
                position + float3(cell_width, 0, cell_width),
                position + float3(0, 0, cell_width),
                position + float3(0, cell_width, 0),
                position + float3(cell_width, cell_width, 0),
                position + float3(cell_width, cell_width, cell_width),
                position + float3(0, cell_width, cell_width),
        };

        float3 vertex_list[12];
        float3 reference_list[12];

        float half_cell_width = cell_width / 2;

        if (edge_table[cube_index] & (1 << 0)) {
                vertex_list[0] = GetEdgeVertex(positions[0], positions[1], info.density_array[0], info.density_array[1]);
                reference_list[0] = position + float3(0, half_cell_width, half_cell_width);
                reference_list[0].x = vertex_list[0].x;
        }

        if (edge_table[cube_index] & (1 << 1)) {
                vertex_list[1] = GetEdgeVertex(positions[1], positions[2], info.density_array[1], info.density_array[2]);
                reference_list[1] = position + float3(half_cell_width, half_cell_width, 0);
                reference_list[1].z = vertex_list[1].z;
        }

        if (edge_table[cube_index] & (1 << 2)) {
                vertex_list[2] = GetEdgeVertex(positions[2], positions[3], info.density_array[2], info.density_array[3]);
                reference_list[2] = position + float3(0, half_cell_width, half_cell_width);
                reference_list[2].x = vertex_list[2].x;
        }

        if (edge_table[cube_index] & (1 << 3)) {
                vertex_list[3] = GetEdgeVertex(positions[3], positions[0], info.density_array[3], info.density_array[0]);
                reference_list[3] = position + float3(half_cell_width, half_cell_width, 0);
                reference_list[3].z = vertex_list[3].z;
        }

        if (edge_table[cube_index] & (1 << 4)) {
                vertex_list[4] = GetEdgeVertex(positions[4], positions[5], info.density_array[4], info.density_array[5]);
                reference_list[4] = position + float3(0, half_cell_width, half_cell_width);
                reference_list[4].x = vertex_list[4].x;
        }

        if (edge_table[cube_index] & (1 << 5)) {
                vertex_list[5] = GetEdgeVertex(positions[5], positions[6], info.density_array[5], info.density_array[6]);
                reference_list[5] = position + float3(half_cell_width, half_cell_width, 0);
                reference_list[5].z = vertex_list[5].z;
        }

        if (edge_table[cube_index] & (1 << 6)) {
                vertex_list[6] = GetEdgeVertex(positions[6], positions[7], info.density_array[6], info.density_array[7]);
                reference_list[6] = position + float3(0, half_cell_width, half_cell_width);
                reference_list[6].x = vertex_list[6].x;
        }

        if (edge_table[cube_index] & (1 << 7)) {
                vertex_list[7] = GetEdgeVertex(positions[7], positions[4], info.density_array[7], info.density_array[4]);
                reference_list[7] = position + float3(half_cell_width, half_cell_width, 0);
                reference_list[7].z = vertex_list[7].z;
        }

        if (edge_table[cube_index] & (1 << 8)) {
                vertex_list[8] = GetEdgeVertex(positions[0], positions[4], info.density_array[0], info.density_array[4]);
                reference_list[8] = position + float3(half_cell_width, 0, half_cell_width);
                reference_list[8].y = vertex_list[8].y;
        }

        if (edge_table[cube_index] & (1 << 9)) {
                vertex_list[9] = GetEdgeVertex(positions[1], positions[5], info.density_array[1], info.density_array[5]);
                reference_list[9] = position + float3(half_cell_width, 0, half_cell_width);
                reference_list[9].y = vertex_list[9].y;
        }

        if (edge_table[cube_index] & (1 << 10)) {
                vertex_list[10] = GetEdgeVertex(positions[2], positions[6], info.density_array[2], info.density_array[6]);
                reference_list[10] = position + float3(half_cell_width, 0, half_cell_width);
                reference_list[10].y = vertex_list[10].y;
        }

        if (edge_table[cube_index] & (1 << 11)) {
                vertex_list[11] = GetEdgeVertex(positions[3], positions[7], info.density_array[3], info.density_array[7]);
                reference_list[11] = position + float3(half_cell_width, 0, half_cell_width);
                reference_list[11].y = vertex_list[11].y;
        }

        float3 summed_normals[12] = {};
        uint normal_count[12] = {};

        uint triangle_count = 0;
        for (int i = 0; i < 15; i += 3) {
                if (triangle_table[cube_index][i] == -1) break;

                /*
                NOTE: Hows this for a plan
                - Loop all edges along x
                        - Calc density and vertex position
                        - Store in edge array. x_edges count = ((cell_count + 1) ^ 2) * (cell_count)
                */

                uint triangle_offset = triangle_offset_array[index];

                // NOTE: How can we just output just index of vertex and have vertex in another array... Loop edges not cells
                vertices[triangle_offset + triangle_count + 0].position = vertex_list[triangle_table[cube_index][i + 0]];
                vertices[triangle_offset + triangle_count + 1].position = vertex_list[triangle_table[cube_index][i + 1]];
                vertices[triangle_offset + triangle_count + 2].position = vertex_list[triangle_table[cube_index][i + 2]];

                // ===== Calc Triangle Normal ===== //

                // ===== Add to Vertex Normal for this cell : Each vertex can neighbour 4 cells ===== //
                // NOTE: This will lead to different normal calcs on the edges.
                // Can do extra calc to get accurate normal? Either need to do marching cubes for cells + border, or accept non averaged normals on the borders of chunks.
                // Need to set offset of unused values to end so we can just ignore. Dont care if we have race conditions as the values are not used
                // But this means we dont need to do if checks.

                // Each cell has 12 edges a vertex can be on.
                uint normals_array_index = (index * 12) + triangle_table[cube_index][i + 0];

                float3 e0 = vertex_list[triangle_table[cube_index][i + 1]] - vertex_list[triangle_table[cube_index][i + 0]]
                float3 e1 = vertex_list[triangle_table[cube_index][i + 2]] - vertex_list[triangle_table[cube_index][i + 0]]

                // Assuming CCW winding
                summed_normals[triangle_table[cube_index][i + 0]] += cross(e0, e1);
                normal_count[triangle_table[cube_index][i + 0]]++;

                triangle_count++;
        }

        // Output to actual array.
        for (int i = 0; i < 12; i++) {
                info.normals[(index * 12) + i] = summed_normals[i];
                info.normal_count[(index * 12) + i] = normal_count[i];
        }
}

// This takes 25MB for 64 size chunk...
// 200MB for 128...
// If we only store index, and then calc the position and normal later its 25MB for 128 and 200MB for 256, in which case just densities take 67MB
// Can also reduce memory using u16, but can we guarantee less that 65k verts? Difficult to guarantee at 128, a single row where each cell has 4 verts... Would need to test.
struct Edge {
        uint edge_case; // 0, 1, 2, or 3

        // uint start_index; // I thought order would matter but it doesnt... Maybe it does? y, z, x means we only need to check edges (x, y, z), (x, y + 1, z), (x, y, z + 1), (x, y + 1, z + 1)
        // Extra edges needed to check - (y + 1).y, (y + 1, z + 1).y and .x, (z + 1).z
        // Can store this in edge_case? when we check if it has an intersect just set some bit in edge_case to 1 for each x,y,z -> want but order of xzy? doesnt matter. edge_case |= ((edge_case ^ (edge_case >> 1)) & 1) << x_bit;
        // OR if (density ^ last_density) edge_index << x_bit; Much simpler...
};

struct EdgeSum {
        uint vertex_offset;
        uint vertex_count;

        uint triangle_offset;
        uint triangle_count;
};

// If we can guarantee less than 1024 triangles in a row, we can use an edge and store 10 bits per count in edge_case, and start_index as triangle count.
struct Row {
        uint x_count;
        uint y_count;
        uint z_count;
        uint triangle_count;
};

struct Vertex {
        float3 position;
        float3 normal; // We can just normalize? or better to store normal counts and use that?
};

static const uint x_bit = 4;
static const uint y_bit = 3;
static const uint z_bit = 2;

[shader("compute")]
[numthreads(1, 8, 8)]
void LoopXEdges(uniform PlanetCreationData info, uint3 dispatch_id : SV_DispatchThreadID) {
        uint edge_count = info.cell_count + 1;

        uint edge_index = dispatch_id.y * info.cell_count + dispatch_id.z * edge_count * info.cell_count;

        uint density_index = dispatch_id.x + dispatch_id.y * info.chunk_cells + dispatch.z * info.chunk_cells * info.chunk_cells;
        float last_density = planet_creation_data.density_array[density_index];
        density_index++;

        uint x_sum = 0;

        for (int i = 0; i < info.cell_count; i++) {
                float density = planet_creation_data.density_array[density_index + i];
                edges[edge_index + i].edge_case |= (last_density < 0) << 0;
                edges[edge_index + i].edge_case |= (density < 0) << 1;

                if (last_density ^ density < 0.0f) {
                        edges[edge_index + i].edge_case |= 1 << x_bit;
                        x_sum++;
                }

                edges[edge_index + i].start_index = x_sum;

                last_density = density;
        }

        rows[dispatch_id.y + dispatch_id.z * info.cell_count].z_count = x_sum;
}


// TODO: This is just copied from X, need to sum for each y_row, then after sum for each z_row, which should give us index and total index count.
[shader("compute")]
[numthreads(8, 1, 8)]
void LoopYEdges(uniform PlanetCreationData info, uint3 dispatch_id : SV_DispatchThreadID) {
        uint edge_count = info.cell_count + 1;

        uint edge_index = dispatch_id.x + dispatch_id.y * info.cell_count + dispatch_id.z * edge_count * info.cell_count;

        // NEED TO FIND OUT HOW TO GET CORRECT ROW (JUST THE Y,Z i guess?)
        uint x_sum = 0; // Add row.x_count for each row
        uint y_sum = 0;

        for (int i = 0; i < info.cell_count; i++) {
                float density = planet_creation_data.density_array[density_index + i];
                edges[edge_index + i].edge_case |= (last_density < 0) << 0;
                edges[edge_index + i].edge_case |= (density < 0) << 1;

                if (last_density ^ density < 0.0f) {
                        edges[edge_index + i].edge_case |= 1 << x_bit;
                        x_sum++;
                }

                edges[edge_index + i].start_index = x_sum;

                last_density = density;
        }

        rows[dispatch_id.y + dispatch_id.z * info.cell_count].z_count = x_sum;
}

[shader("compute")]
[numthreads(1, 8, 8)]
void GetTriangleCount(uniform PlanetCreationData info, uint3 dispatch_id : SV_DispatchThreadID) {
        uint y_offset = info.cell_count;
        uint z_offset = edge_count * info.cell_count;

        uint e1_index = dispatch_id.y * y_offset + dispatch_id.z * z_offset;
        uint e2_index = e1_index + z_offset; // Flip?
        uint e3_index = e1_index + y_offset;
        uint e4_index = e2_index + y_offset;

        for (int i = 0; i < info.cell_count; i++) {
                uint edge_case = 0;

                edge_case |= edges[e1_index + i].edge_case << 0;
                edge_case |= (edges[e2_index + i].edge_case ^ 3) << 2; // Flip? Edge table we use has e2 going opposite direction to e1
                edge_case |= edges[e3_index + i].edge_case << 4;
                edge_case |= (edges[e4_index + i].edge_case ^ 3) << 6; // Flip?

                edge_case &= 255; // Remove all excess bits. As we use edge_case to store other information.

                // edge_case is this cells MC index!

                // Count triangles

                // Check y and z intersects - Only for edge at cell x,y,z
                // How do we check for y and z intersects at the y and z boundry of the cell?
        }

        // Check last y, z and need to add to total - These are not stored anywhere but we can get from previous edge.
}

[shader("compute")]
[numthreads(8, 1, 8)]
void LoopZEdges(uniform PlanetCreationData info, uint3 dispatch_id : SV_DispatchThreadID) {
        // Store in order for optimal access not consistant order! This is zxy
        uint edge_count = (info.cell_count + 1);
        uint edge_index_start = dispatch_id.y * edge_count * edge_count + dispatch_id.x * edge_count;

        // ===== Get Density ===== //
        uint density_index = dispatch_id.x + dispatch_id.y * info.chunk_cells + dispatch_id.z * info.chunk_cells * info.chunk_cells;

        last_density = planet_creation_data.density_array[density_index];

        uint vertex_count = 0;

        uint y_edge_index = dispatch_id.z * edge_count * edge_count + dispatch_id.x * edge_count + dispatch_id.y;
        uint y_last_edge_index = dispatch_id.z * edge_count * edge_count + dispatch_id.x * edge_count + info.cell_count;
        uint y_sum = edges[1][y_last_edge_index];

        uint x_sum = edges[0][info.cell_count * edge_count * edge_count];

        for (uint z = 1; z < edge_count; z++) {
                // We look at edge before current point
                uint index = edge_index_start + (z - 1);

                density_index += info.chunk_cells * info.chunk_cells;
                float density = planet_creation_data.density_array[density_index];

                if (last_density ^ density < 0.0f) {
                        vertex_count++;
                }

                edges[2][index].index = vertex_count;

                last_density = density;

                // ===== Calc X Indices ===== //

                // I hate this, is there a way to avoid this?
                if (dispatch_id.x == info.cell_count) continue;

                x_edge_index += info.cell_count;

                edges[0][x_edge_index] = x_sum + edges[0][x_edge_index];

                x_last_edge_index += info.cell_count;
                x_sum += edges[0][x_last_edge_index];
        }

        // Set last value so that we can store sums without overwritting
        // Add x total so we know what the offset for y vertices is.
        edges[0][edge_index_start + info.cell_count] = vertex_count + x_sum + edges[0][x_edge_index];
}