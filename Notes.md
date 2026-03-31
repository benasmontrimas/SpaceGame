# NOTES

- Compute shader, uses 2D information, is it better to store in a Buffer resource, or an image resource, or is there not difference.
        - Can measure to see if it affects performance.


- When rendering planets, store vertices in a single planet buffer, with however many chunks we want to load then just alter the index buffer
less memory needs to be copied over