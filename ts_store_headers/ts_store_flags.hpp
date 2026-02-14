#pragma once

#include <bitset>
#include <array>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>
#include <sstream>

class TsStoreFlags {
public:

    static constexpr size_t FlagBytes = sizeof(uint64_t);
    static constexpr size_t Bits = FlagBytes * 8;
    static_assert(FlagBytes == 8, "TsStoreFlags requires 64-bit uint64_t");

    explicit TsStoreFlags(uint64_t raw = 0) {
        flags = std::bitset<Bits>(raw);
    }

    size_t raw() const { return flags.to_ullong(); }

    // User-controlled flags (bits 0-6)
    enum class UserFlag : size_t {
        LogConsole     = 0,
        KeeperRecord   = 1,
        DatabaseEntry  = 2,
        SendNetwork    = 3,
        HotCacheHint   = 4,
        IsResult       = 5,
        IsExplicitNull = 6
    };

    // ts_store internal flags (bits 16-17)
    enum class InternalFlag : size_t {
        HasData   = 16,
        IsInvalid = 17
    };

    // Severity (bits 32-34)
    enum class Severity : uint8_t {
        NotSet = 0,
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
        Fatal
    };

    static constexpr size_t Severity_LSB = 32;  // Only constant needed for masking

    // Generic accessors for UserFlag and InternalFlag
    template<typename FlagT>
    requires (std::is_same_v<FlagT, UserFlag> || std::is_same_v<FlagT, InternalFlag>)
    void set(FlagT f, bool value = true) noexcept {
        flags.set(static_cast<size_t>(f), value);
    }

    template<typename FlagT>
    requires (std::is_same_v<FlagT, UserFlag> || std::is_same_v<FlagT, InternalFlag>)
    void clear(FlagT f) noexcept {
        flags.reset(static_cast<size_t>(f));
    }

    template<typename FlagT>
    requires (std::is_same_v<FlagT, UserFlag> || std::is_same_v<FlagT, InternalFlag>)
    [[nodiscard]] bool is_set(FlagT f) const noexcept {
        return flags.test(static_cast<size_t>(f));
    }

    // All describable flags (user + internal)
    static constexpr size_t DescribableCount = 9;
    static constexpr std::array<std::pair<size_t, std::string_view>, DescribableCount> describable_flags = {{
        {static_cast<size_t>(UserFlag::LogConsole),     "LogConsole"},
        {static_cast<size_t>(UserFlag::KeeperRecord),   "KeeperRecord"},
        {static_cast<size_t>(UserFlag::DatabaseEntry),  "DatabaseEntry"},
        {static_cast<size_t>(UserFlag::SendNetwork),    "SendNetwork"},
        {static_cast<size_t>(UserFlag::HotCacheHint),   "HotCacheHint"},
        {static_cast<size_t>(UserFlag::IsResult),       "IsResult"},
        {static_cast<size_t>(UserFlag::IsExplicitNull), "IsExplicitNull"},
        {static_cast<size_t>(InternalFlag::HasData),    "HasData"},
        {static_cast<size_t>(InternalFlag::IsInvalid),  "IsInvalid"}
    }};

        // Severity methods
    void set_severity(Severity sev) noexcept {
        uint64_t raw = flags.to_ullong();
        raw &= ~(0b111ULL << Severity_LSB);
        raw |= (static_cast<uint64_t>(sev) << Severity_LSB);
        flags = std::bitset<Bits>(raw);
    }

    [[nodiscard]] Severity get_severity() const noexcept {
        return static_cast<Severity>((flags.to_ullong() >> Severity_LSB) & 0b111);
    }

    static constexpr std::array<std::string_view, 8> severity_strings = {
        "Not Set", "Trace", "Debug", "Info", "Warn", "Error", "Critical", "Fatal"
    };

    std::string get_severity_string() const {
        return std::string(severity_strings[static_cast<size_t>(get_severity())]);
    }

    static constexpr uint64_t get_severity_mask_from_index(size_t index) noexcept {
        return (index & 0b111) << Severity_LSB;
    }

    void set_severity_from_index(size_t index) noexcept {
        set_severity(static_cast<Severity>(index & 0b111));
    }

    // Printing helpers
    std::vector<std::string> get_set_flags() const {
        std::vector<std::string> set_descs;
        set_descs.reserve(DescribableCount);

        for (const auto& pair : describable_flags) {
            if (flags.test(pair.first)) {
                set_descs.emplace_back(pair.second);
            }
        }
        return set_descs;
    }

    std::string to_string() const {
        auto set_flags = get_set_flags();
        if (set_flags.empty()) return "<none>";

        std::ostringstream oss;
        for (size_t i = 0; i < set_flags.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << set_flags[i];
        }
        oss << " | Severity: " << get_severity_string();
        return oss.str();
    }

    // Serialization
    std::array<uint8_t, FlagBytes> to_bytes() const {
        uint64_t val = flags.to_ullong();
        std::array<uint8_t, FlagBytes> bytes{};

        for (size_t i = 0; i < FlagBytes; ++i) {
            bytes[i] = static_cast<uint8_t>(val >> ((FlagBytes - 1 - i) * 8));
        }
        return bytes;
    }

    void from_bytes(const std::array<uint8_t, FlagBytes>& bytes) {
        size_t val = 0;
        for (size_t i = 0; i < FlagBytes; ++i) {
            val = (val << 8) | bytes[i];
        }
        flags = std::bitset<Bits>(val);
    }

    static constexpr size_t get_severity_string_width() noexcept {
        size_t max = 0;
        for (const auto& str : severity_strings) max = std::max(max, str.size());
        return max;
    }

private:
    std::bitset<Bits> flags{};
};

//Free standing function...do not put in class...
inline uint64_t flags_set_has_data(uint64_t raw_flags, bool const enable = true) noexcept {
    constexpr uint64_t mask = 1ULL << static_cast<size_t>(TsStoreFlags::InternalFlag::HasData);
    return enable ? (raw_flags | mask) : (raw_flags & ~mask);
}

inline uint64_t flags_clear_has_data(uint64_t const raw_flags) noexcept {
    return flags_set_has_data(raw_flags, false);
}

inline uint64_t set_user_flag(uint64_t raw, TsStoreFlags::UserFlag f, bool const value = true) noexcept {
    uint64_t mask = 1ULL << static_cast<size_t>(f);
    return value ? (raw | mask) : (raw & ~mask);
}

inline uint64_t set_internal_flag(uint64_t raw, TsStoreFlags::InternalFlag f, bool const value = true) noexcept {
    uint64_t mask = 1ULL << static_cast<size_t>(f);
    return value ? (raw | mask) : (raw & ~mask);
}

inline uint64_t set_severity(uint64_t raw, TsStoreFlags::Severity sev) noexcept {
    raw &= ~(0b111ULL << TsStoreFlags::Severity_LSB);
    raw |= (static_cast<uint64_t>(sev) << TsStoreFlags::Severity_LSB);
    return raw;
}