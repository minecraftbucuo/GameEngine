//
// ConfigManager 单元测试
//
// 这些用例依赖「路径可指定 + reset()」这两项改造（见 17a0e36）：
// 全部用系统临时目录里的独立文件，既不改进程工作目录，
// 也不碰仓库里的 Asset/config.json。
//
#include <doctest/doctest.h>

#include "ConfigManager.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

// 临时 JSON 文件：构造时写入内容，析构时删除。
class TempJsonFile {
public:
    TempJsonFile(const std::string& content, const std::string& tag) {
        static int seq = 0;
        const auto name = "ge_test_" + tag + "_" + std::to_string(++seq) + ".json";
        path_ = (std::filesystem::temp_directory_path() / name).string();

        std::ofstream out(path_, std::ios::binary | std::ios::trunc);
        out << content;
    }

    ~TempJsonFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    TempJsonFile(const TempJsonFile&) = delete;
    TempJsonFile& operator=(const TempJsonFile&) = delete;

    [[nodiscard]] const std::string& path() const { return path_; }

private:
    std::string path_;
};

// 保证用例结束（含中途 REQUIRE 失败提前返回）时恢复默认单例，
// 不让全局替换泄漏到其它用例。
struct InstanceGuard {
    ~InstanceGuard() { ConfigManager::setInstance(nullptr); }
};

} // namespace

// ── 默认值 / reset ────────────────────────────────────────────────

TEST_CASE("ConfigManager: 默认值") {
    ConfigManager cm;

    CHECK(cm.window.width == 1200);
    CHECK(cm.window.height == 960);
    CHECK(cm.window.title == "GameEngine");
    CHECK(cm.window.fps == 165);

    CHECK(cm.network.serverIp == "127.0.0.1");
    CHECK(cm.network.port == 6666);
    CHECK(cm.network.webBridgePort == 8081);
    CHECK(cm.network.tickRate == 128);
    CHECK(cm.network.timeout == doctest::Approx(5.0f));

    CHECK(cm.game.gravity == doctest::Approx(3200.0f));
    CHECK(cm.game.playerSpeed == doctest::Approx(500.0f));
    CHECK(cm.game.debug == true);

    CHECK(cm.getPath() == ConfigManager::DEFAULT_PATH);
}

TEST_CASE("ConfigManager: reset() 恢复默认值") {
    ConfigManager cm;
    cm.window.width = 1;
    cm.network.port = 1;
    cm.game.gravity = 1.0f;
    cm.assets.font = "dirty";
    cm.assets.textures["x"] = "y";

    cm.reset();

    CHECK(cm.window.width == 1200);
    CHECK(cm.network.port == 6666);
    CHECK(cm.game.gravity == doctest::Approx(3200.0f));
    CHECK(cm.assets.font == "./Asset/fonts/Minecraft_AE.ttf");
    CHECK(cm.assets.textures.empty());
}

// ── 加载失败路径 ──────────────────────────────────────────────────

TEST_CASE("ConfigManager: 文件不存在 —— 返回 false 并记住该路径") {
    ConfigManager cm;
    CHECK_FALSE(cm.load("/definitely/not/here/config.json"));
    CHECK(cm.getPath() == "/definitely/not/here/config.json");
}

TEST_CASE("ConfigManager: 非法 JSON —— 返回 false 且不崩") {
    TempJsonFile f("{ this is not json at all", "badjson");

    ConfigManager cm;
    CHECK_FALSE(cm.load(f.path()));
}

TEST_CASE("ConfigManager: 根节点不是对象 —— 返回 false") {
    TempJsonFile f("[1, 2, 3]", "arrayroot");

    ConfigManager cm;
    CHECK_FALSE(cm.load(f.path()));
}

// ── 正常加载 ──────────────────────────────────────────────────────

TEST_CASE("ConfigManager: 完整配置生效") {
    TempJsonFile f(R"({
        "window":  { "width": 800, "height": 600, "title": "T", "fps": 60 },
        "network": { "serverIp": "10.0.0.1", "port": 1234, "tickRate": 64,
                     "timeout": 2.5, "webBridgePort": 9 },
        "game":    { "gravity": 100.0, "playerSpeed": 200.0, "debug": false },
        "assets":  { "font": "./x.ttf", "textures": { "a": "b.png" } }
    })", "full");

    ConfigManager cm;
    REQUIRE(cm.load(f.path()));

    CHECK(cm.window.width == 800);
    CHECK(cm.window.height == 600);
    CHECK(cm.window.title == "T");
    CHECK(cm.window.fps == 60);

    CHECK(cm.network.serverIp == "10.0.0.1");
    CHECK(cm.network.port == 1234);
    CHECK(cm.network.tickRate == 64);
    CHECK(cm.network.timeout == doctest::Approx(2.5f));
    CHECK(cm.network.webBridgePort == 9);

    CHECK(cm.game.gravity == doctest::Approx(100.0f));
    CHECK(cm.game.playerSpeed == doctest::Approx(200.0f));
    CHECK(cm.game.debug == false);

    CHECK(cm.assets.font == "./x.ttf");
    CHECK(cm.getTexturePath("a") == "b.png");
    CHECK(cm.getSoundPath("nope").empty());
    CHECK(cm.getPath() == f.path());
}

TEST_CASE("ConfigManager: 缺键保留实例原值（load 是就地覆盖，不先清零）") {
    // 记录当前行为：load() 不先 reset()，缺失的键会保留实例上的现有值。
    // 这也是「测试里必须先 reset() 才能拿到干净默认值」的原因。
    TempJsonFile f(R"({ "window": { "width": 640 } })", "partial");

    ConfigManager cm;
    cm.game.gravity = 777.0f;                    // 先改脏
    REQUIRE(cm.load(f.path()));

    CHECK(cm.window.width == 640);                        // 提供了 → 覆盖
    CHECK(cm.window.height == 960);                       // 未提供 → 保留（此处恰好等于默认）
    CHECK(cm.game.gravity == doctest::Approx(777.0f));    // 未提供 → 保留脏值
}

TEST_CASE("ConfigManager: reset() 后再 load 部分配置 —— 缺键回到默认") {
    TempJsonFile f(R"({ "window": { "width": 640 } })", "partial_reset");

    ConfigManager cm;
    cm.game.gravity = 777.0f;
    cm.reset();                                  // 干净起点
    REQUIRE(cm.load(f.path()));

    CHECK(cm.window.width == 640);
    CHECK(cm.game.gravity == doctest::Approx(3200.0f));   // 缺键 → 默认值
}

// ── 保存 ──────────────────────────────────────────────────────────

TEST_CASE("ConfigManager: save() 写回并保留未修改段") {
    TempJsonFile f(R"({
        "window": { "width": 800, "height": 600, "title": "T", "fps": 60 },
        "assets": { "font": "./x.ttf" }
    })", "save");

    ConfigManager cm;
    REQUIRE(cm.load(f.path()));
    cm.window.width = 1024;
    REQUIRE(cm.save());

    ConfigManager reloaded;
    REQUIRE(reloaded.load(f.path()));
    CHECK(reloaded.window.width == 1024);        // 改过的写回了
    CHECK(reloaded.assets.font == "./x.ttf");    // 未改的段仍在
}

// ── 实例替换（去单例化的机制验证）─────────────────────────────────

TEST_CASE("ConfigManager: setInstance 替换生效且互不污染") {
    InstanceGuard guard;

    ConfigManager detached;
    detached.network.port = 9999;
    ConfigManager::setInstance(&detached);

    CHECK(CONFIG.network.port == 9999);                          // 宏入口走替换后的实例
    CHECK(ConfigManager::getInstance().network.port == 9999);

    ConfigManager::setInstance(nullptr);
    CHECK(CONFIG.network.port == 6666);                          // 回到默认单例
    CHECK(detached.network.port == 9999);                        // 被替换的实例未被改动
}
