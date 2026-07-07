# The Lonely Surveyor

A space game made in C++ and Vulkan, using Flying Edges/Marching Cubes for procedural planet generation.

## Building
- Build script provided for building with MSBuild
        - Takes a configuration: Release, Development, or Debug
        - Defaults to Release.
        - From the project folder run "Scripts/Build.ps1" to build the project
- Premake file is provided


## Known Bugs
- Minimizing crashes the game.
- On windows quiting to main menu and returning, results in incorrect movement.
- Linux performance is currently better than Windows.

## Controls
- WASD: movement
- ESC: Quit (in main menu) Pause (in game)
- SPACE: Select (in menu) Jump (in game)
- F1: Toggle mouse relative to screen

## Requirements
- Vulkan 1.3 compatable device

### Assets Used
- Ground Texture from : https://polyhaven.com/
- Rock Asset from : https://polyhaven.com/
- Sky texture from : https://www.spacespheremaps.com/