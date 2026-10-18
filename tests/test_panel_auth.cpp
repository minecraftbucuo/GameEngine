//
// 调试面板鉴权工具单元测试
//
// PanelAuth.h 是纯字符串处理，不依赖 httplib / 线程，因此可以放进
// 「秒级、完全离线」的测试目标，不需要 SERVER_BUILD 宏。
//
#include <doctest/doctest.h>

#include "Debug/PanelAuth.h"

#include <set>
#include <string>

namespace {

bool isLowerHex(const std::string& s) {
    for (const char c : s) {
        const bool digit = c >= '0' && c <= '9';
        const bool alpha = c >= 'a' && c <= 'f';
        if (!digit && !alpha) return false;
    }
    return true;
}

} // namespace

TEST_CASE("panel_auth: generateToken 默认产出 16 位小写十六进制") {
    const std::string t = panel_auth::generateToken();

    CHECK(t.size() == 16);
    CHECK(isLowerHex(t));
}

TEST_CASE("panel_auth: generateToken 按字节数产出对应长度") {
    CHECK(panel_auth::generateToken(4).size() == 8);
    CHECK(panel_auth::generateToken(1).size() == 2);
    CHECK(panel_auth::generateToken(0).empty());
}

TEST_CASE("panel_auth: generateToken 连续调用不重复") {
    // 只用来发现「忘了播种 / 返回常量」这类实现错误，不是随机性检验
    std::set<std::string> seen;
    for (int i = 0; i < 8; ++i) {
        seen.insert(panel_auth::generateToken());
    }
    CHECK(seen.size() >= 7);
}

TEST_CASE("panel_auth: tokenEquals 相同返回 true") {
    CHECK(panel_auth::tokenEquals("abc123", "abc123"));
    CHECK(panel_auth::tokenEquals("", ""));
}

TEST_CASE("panel_auth: tokenEquals 内容不同返回 false") {
    CHECK_FALSE(panel_auth::tokenEquals("abc123", "abc124"));
    CHECK_FALSE(panel_auth::tokenEquals("abc123", "Abc123"));
}

TEST_CASE("panel_auth: tokenEquals 长度不同必须失败（含前缀相同的情况）") {
    CHECK_FALSE(panel_auth::tokenEquals("abcdef", "abc"));
    CHECK_FALSE(panel_auth::tokenEquals("abc", "abcdef"));
    CHECK_FALSE(panel_auth::tokenEquals("", "abc"));
    CHECK_FALSE(panel_auth::tokenEquals("abc", ""));
}
