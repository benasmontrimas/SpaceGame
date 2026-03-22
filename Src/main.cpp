#include <print>

#include "Game.h"

int main() {
        Game game{};
        game.Init();

        game.Run();

        game.Shutdown();
}