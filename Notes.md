# NOTES

- Game does not use a very good method for selecting queue families. On some devices we might have queues being shared when they
dont need to be (well only if non transfer but still compute/graphics queues exist).
        - Apparently all graphics/compute families implicity support transfer operations. But this means we might not select a transfer
        queue even if one is implicitly available?
        - Check if any queues only support one action and use that.
        - Also currently prioritise a compute queue over a transfer queue, however I guess most likely that if there isnt a seperate
        compute/transfer queue they will share the same one, rather than the graphics. So might want to do checks to make it so that
        if there isnt a queue family for each we prioritise transfer having a seperate and lump compute and graphics together.
                - Well actually this depends. Would we rather better transfer or chunk loading?

- Compute shader, uses 2D information, is it better to store in a Buffer resource, or an image resource, or is there not difference.
        - Can measure to see if it affects performance.


- When rendering planets, store vertices in a single planet buffer, with however many chunks we want to load then just alter the index buffer
less memory needs to be copied over