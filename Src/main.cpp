#include "Game.h"

int main() {
        Game game{};
        game.Init();

        game.Run();

        game.Shutdown();
}

#ifdef OS_WINDOWS

#include <windows.h>

int WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR     lpCmdLine,
  int       nShowCmd
) {
        main();
}

#endif