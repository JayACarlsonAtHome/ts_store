#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <stdexcept>

class TsStoreFlags {
public:
    static constexpr size_t Bytes = 1;

    // Severity bit positions (one-hot for exclusive use, or allow multi if needed)
    static constexpr uint8_t Severity_Info   = 0;
    static constexpr uint8_t Severity_Warn   = 1;
    static constexpr uint8_t Severity_Error  = 2;
    static constexpr uint8_t Severity_Trace  = 3;
    static constexpr uint8_t Severity_Debug  = 4;
    // Bits 5-7 reserved for future (e.g., Note, Fatal, etc.)

    TsStoreFlags() = default;

    void set(uint8_t pos, bool value = true) {
        if (pos >= Bytes * 8) throw std::out_of_range("Flag position out of range");
        if (value) data_[0] |=  (static_cast<uint8_t>(1) << pos);
        else       data_[0] &= ~(static_cast<uint8_t>(1) << pos);
    }

    void clear_all() { data_.fill(0); }

    bool test(uint8_t pos) const {
        if (pos >= Bytes * 8) throw std::out_of_range("Flag position out of range");
        return (data_[0] & (static_cast<uint8_t>(1) << pos)) != 0;
    }

    void toggle(uint8_t pos) {
        if (pos >= Bytes * 8) throw std::out_of_range("Flag position out of range");
        data_[0] ^= (static_cast<uint8_t>(1) << pos);
    }

    std::vector<uint8_t> to_bytes() const {
        return {data_[0]};  // Single byte, already in host order but we'll force big-endian logic later if Bytes > 1
    }

    void from_bytes(const std::vector<uint8_t>& bytes) {
        if (bytes.size() != Bytes) throw std::invalid_argument("Byte vector size must be 1");
        data_[0] = bytes[0];
        // For multi-byte future: add explicit big-endian unpacking
    }

private:
    std::array<uint8_t, Bytes> data_{};
};