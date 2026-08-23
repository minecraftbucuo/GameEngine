//
// SDL_net 迁移 N1：eng::Packet 非模板实现（整型字节序 / string 编解码）
//
#include "Packet.h"

namespace eng {

// ── bool：1 字节（复刻 sf::Packet）────────────────────────────
Packet& Packet::operator<<(const bool value) {
    const std::uint8_t byte = value ? 1 : 0;
    return pod(byte);
}

Packet& Packet::operator>>(bool& value) {
    std::uint8_t byte = 0;
    if (!pod(byte).isValid()) return *this;
    value = byte != 0;
    return *this;
}

// ── 32 位整型：网络字节序（大端）──────────────────────────────
Packet& Packet::writeNet32(const std::uint32_t value) {
    const std::uint8_t bytes[4] = {
        static_cast<std::uint8_t>(value >> 24),
        static_cast<std::uint8_t>(value >> 16),
        static_cast<std::uint8_t>(value >> 8),
        static_cast<std::uint8_t>(value),
    };
    append(bytes, sizeof(bytes));
    return *this;
}

Packet& Packet::readNet32(std::uint32_t& value) {
    if (!canRead(4)) return *this;
    const auto* p = m_data.data() + m_readPos;
    value = (static_cast<std::uint32_t>(p[0]) << 24)
          | (static_cast<std::uint32_t>(p[1]) << 16)
          | (static_cast<std::uint32_t>(p[2]) << 8)
          | static_cast<std::uint32_t>(p[3]);
    m_readPos += 4;
    return *this;
}

Packet& Packet::operator<<(const std::int32_t value) {
    return writeNet32(static_cast<std::uint32_t>(value));
}

Packet& Packet::operator>>(std::int32_t& value) {
    std::uint32_t raw = 0;
    if (!readNet32(raw).isValid()) return *this;
    value = static_cast<std::int32_t>(raw);
    return *this;
}

Packet& Packet::operator<<(const std::uint32_t value) { return writeNet32(value); }

Packet& Packet::operator>>(std::uint32_t& value) { return readNet32(value); }

// ── float：原始内存直拷（sf 不对浮点换序，复刻之）─────────────
Packet& Packet::operator<<(const float value) { return pod(value); }

Packet& Packet::operator>>(float& value) { return pod(value); }

// ── string：uint32 大端长度 + 字节流（复刻 sf::Packet）─────────
Packet& Packet::operator<<(const std::string& value) {
    writeNet32(static_cast<std::uint32_t>(value.size()));
    append(value.data(), value.size());
    return *this;
}

Packet& Packet::operator>>(std::string& value) {
    std::uint32_t length = 0;
    if (!readNet32(length).isValid()) return *this;
    if (!canRead(length)) return *this;

    value.assign(reinterpret_cast<const char*>(m_data.data() + m_readPos), length);
    m_readPos += length;
    return *this;
}

} // namespace eng
