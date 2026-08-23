//
// SDL_net 迁移 N1：自研序列化容器（纯新增，零引用）
//
// 线格式逐字节复刻 sf::Packet（实测字节对齐验证，见 build/packet_fmt_test.cpp）：
//   - bool     = 1 字节
//   - int32/uint32 = 4 字节，网络字节序（大端，htonl 语义）
//   - float    = 4 字节，原始内存直拷（sf 原样，不换序！）
//   - string   = uint32 大端长度 + 字节流（不含 '\0'）
//   - 枚举     = 按底层类型的上述规则
//   - 流帧（TcpClient 组帧时使用）：uint32 长度前缀 + payload
// 迁移期保证：新旧 exe 线格式互通，可交叉验证。
//
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace eng {

class Packet {
public:
    Packet() = default;

    // ── 容器状态 ──────────────────────────────────────────────
    [[nodiscard]] const void* getData() const { return m_data.data(); }
    [[nodiscard]] std::size_t getDataSize() const { return m_data.size(); }
    void clear() {
        m_data.clear();
        m_readPos = 0;
        m_valid = true;
    }

    /// 读越界/长度不符时置无效（复刻 sf::Packet operator bool 惯用法：
    /// while (packet >> x) 在流读尽时自然终止）
    [[nodiscard]] bool isValid() const { return m_valid; }
    explicit operator bool() const { return m_valid; }

    // ── 追加原始字节（TcpClient 组帧用）──────────────────────
    void append(const void* data, const std::size_t size) {
        if (size == 0) return;
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        m_data.insert(m_data.end(), bytes, bytes + size);
    }

    // ── 运算符：覆盖本项目 Packet 实际流经的全部类型 ──────────
    Packet& operator<<(const bool value);
    Packet& operator>>(bool& value);

    Packet& operator<<(const std::int32_t value);
    Packet& operator>>(std::int32_t& value);

    Packet& operator<<(const std::uint32_t value);
    Packet& operator>>(std::uint32_t& value);

    Packet& operator<<(const float value);
    Packet& operator>>(float& value);

    Packet& operator<<(const std::string& value);
    Packet& operator>>(std::string& value);

    // 通用枚举支持（复刻 NetworkProtocol.h 原模板，底层类型编码）
    template <typename T>
    std::enable_if_t<std::is_enum_v<T>, Packet&>
    operator<<(const T& enumVal) {
        using Underlying = std::underlying_type_t<T>;
        return writeUnderlying<Underlying>(static_cast<Underlying>(enumVal));
    }

    template <typename T>
    std::enable_if_t<std::is_enum_v<T>, Packet&>
    operator>>(T& enumVal) {
        using Underlying = std::underlying_type_t<T>;
        Underlying val{};
        if (!readUnderlying<Underlying>(val).isValid()) return *this;
        enumVal = static_cast<T>(val);
        return *this;
    }

private:
    // 枚举底层类型分派（项目枚举底层均为 uint8/int32/uint32）
    template <typename T>
    Packet& writeUnderlying(const T value) {
        if constexpr (sizeof(T) == 1) { return pod(value); }
        else { return writeNet32(static_cast<std::uint32_t>(value)); }
    }

    template <typename T>
    Packet& readUnderlying(T& value) {
        if constexpr (sizeof(T) == 1) { return pod(value); }
        else {
            std::uint32_t raw = 0;
            readNet32(raw);
            value = static_cast<T>(raw);
            return *this;
        }
    }

    /// POD 原始内存读写（float 用：sf 对浮点不换序，直拷）
    template <typename T>
    Packet& pod(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "POD only");
        append(&value, sizeof(T));
        return *this;
    }

    template <typename T>
    Packet& pod(T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "POD only");
        if (!canRead(sizeof(T))) return *this;
        std::memcpy(&value, m_data.data() + m_readPos, sizeof(T));
        m_readPos += sizeof(T);
        return *this;
    }

    /// 32 位整数网络字节序读写（sf 对整型走 htonl 语义）
    Packet& writeNet32(const std::uint32_t value);
    Packet& readNet32(std::uint32_t& value);

    [[nodiscard]] bool canRead(const std::size_t size) const {
        if (!m_valid || m_readPos + size > m_data.size()) {
            m_valid = false;
            return false;
        }
        return true;
    }

    std::vector<std::uint8_t> m_data;   // payload（不含流帧长度前缀）
    std::size_t m_readPos = 0;          // 读取偏移
    mutable bool m_valid = true;        // mutable：operator bool 惯用法下允许 const 读检查置失效
};

} // namespace eng
