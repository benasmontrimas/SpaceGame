# NOTES

# Game Engine

- Game does not use a very good method for selecting queue families. On some devices we might have queues being shared when they
dont need to be (well only if non transfer but still compute/graphics queues exist).
        - Apparently all graphics/compute families implicity support transfer operations. But this means we might not select a transfer
        queue even if one is implicitly available?
        - Check if any queues only support one action and use that.
        - Also currently prioritise a compute queue over a transfer queue, however I guess most likely that if there isnt a seperate
        compute/transfer queue they will share the same one, rather than the graphics. So might want to do checks to make it so that
        if there isnt a queue family for each we prioritise transfer having a seperate and lump compute and graphics together.
                - Well actually this depends. Would we rather better transfer or chunk loading?


# Transfer Engine

- Transfer Engine currently only allows passing data either back to the queue that it came from, or to any queue if it was already in
the transfer queue.
- Might want to allow passing from a queue, to transfer, then to another queue for ownership. IE, if we want to read from a compute buffer, before
its passed to graphics, we would now need to do two transfers.
- Fairly rare case. As usually would only be copying buffers between queues, which would pass both buffers back to the queue they were originally
from, or if a buffer is directly used, no transfer is needed.


# Compute Engine

- Compute shader, uses 2D information, is it better to store in a Buffer resource, or an image resource, or is there not difference.
        - Can measure to see if it affects performance.

- Is it better to loop within a shader or have more lightweight threads? ie. for density, would it be better to generate 5 numbers in the shader
or just the 1 point and run it 5 times?


# Gameplay

- When rendering planets, store vertices in a single planet buffer, with however many chunks we want to load then just alter the index buffer
less memory needs to be copied over

- Generate noise in the same way a kernel filter is applied, to create smoother noise, have random points which act as anchor points for areas around them.
Points can be placed is a random grid structure and then points around are influenced by that point.
- Points can be generated between chunks, and means continuity between edges.
- Poisson-Disc sampling.