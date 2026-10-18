//
// 调试面板鉴权工具（纯逻辑，无第三方依赖）
//
// 刻意不做 #ifdef SERVER_BUILD 整文件守卫：这里只有字符串处理，
// 客户端编进去是死代码（inline 未被调用即不生成），但换来单元测试可直接引用，
// 不必为测试目标单独配置宏。
//
#pragma once

#include <cstddef>
#include <random>
#include <string>
#include <string_view>

namespace panel_auth {

// 生成 bytes 字节随机数的十六进制表示（2 * bytes 个字符，小写）。
// 默认 8 字节 → 16 字符，够局域网调试用；不用于任何密码学场景。
inline std::string generateToken(std::size_t bytes = 8) {
    static constexpr char HEX[] = "0123456789abcdef";

    std::random_device rd;
    std::mt19937_64 gen(rd());

    std::string out;
    out.reserve(bytes * 2);
    for (std::size_t i = 0; i < bytes; ++i) {
        const auto v = static_cast<unsigned>(gen() & 0xFFu);
        out.push_back(HEX[v >> 4]);
        out.push_back(HEX[v & 0x0Fu]);
    }
    return out;
}

// 常量时间比较：内容不同也要走完全程，避免按字节提前返回泄露前缀匹配长度。
// 长度不等直接返回 false —— 长度本身不是秘密，且提前返回不泄露内容信息。
inline bool tokenEquals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;

    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

} // namespace panel_auth
