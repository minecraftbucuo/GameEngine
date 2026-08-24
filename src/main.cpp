#include "GameEngine.h"

int main() {
#ifdef __EMSCRIPTEN__
    // WASM 移植 Step 3：emscripten 主循环 simulate_infinite_loop 会中断 main
    // 且不再返回，栈上对象生命周期不可靠；引擎改为堆分配常驻，页面关闭整体回收
    auto* engine = new GameEngine();
    engine->init();
    engine->start();
#else
    GameEngine engine;
    engine.init();
    engine.start();
#endif
    return 0;
}