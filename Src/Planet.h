#pragma once

// NOTE: ChunkLoading should generate these.
struct PlanetChunk {
        u32 vertex_count;
        u32 index_count;

        // Buffer for Vertices
        // Buffer for Indices

        // NOTE: Do we want to store the LOD for all 4 corners? If a chunk near this one changes LOD we
        // want to be able to modify just the edge connecting it.
        uVec4 lod;

        float* index_offsets; // Store the offsets for edges, so if we change a bordering lod we need to do less work to find the vertex.
};