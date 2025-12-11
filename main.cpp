#include "GameEngine.h"

int main() {
    auto* game = new GameEngine();
    game->init();
    game->start();
    return 0;
}