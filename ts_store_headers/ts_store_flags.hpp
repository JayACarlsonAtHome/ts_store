#include <bitset>
#include <array>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <initializer_list>

template<size_t Bytes>
class TsStoreFlags {
public:

    static constexpr size_t Bits = Bytes * 8;
    static constexpr size_t Bit_LogConsole     =  0;
    static constexpr size_t Bit_KeeperRecord   =  1;
    static constexpr size_t Bit_DatabaseEntry  =  2;
    static constexpr size_t Bit_SendNetwork    =  3;
    static constexpr size_t Bit_HotCacheHint   =  5;
    static constexpr size_t Bit_Severity_LSB   =  6;  // 3-bit field (6-8)
    static constexpr size_t Bit_IsResultData   = 59;
    static constexpr size_t Bit_HasData        = 60;
    static constexpr size_t Bit_IsExplicitNull = 61;
    static constexpr size_t Bit_IsInvalid      = 62;

    enum class Severity : uint8_t
    {
        No_Value_Selected = 0,
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
        Fatal
    };

    std::string get_severity_string() const {
        uint8_t sev = static_cast<uint8_t>((flags.to_ullong() >> Bit_Severity_LSB) & 0b111);
        switch (sev) {
        case 0: return  "No Value Selected";
        case 1: return  "Trace";
        case 2: return  "Debug";
        case 3: return  "Info";
        case 4: return  "Warn";
        case 5: return  "Error";
        case 6: return  "Critical";
        case 7: return  "Fatal";
        default: return  "No Value Selected";
        }
    }

    static size_t get_severity_string_width()  { return 10

        ; };

    using EventFlags = TsStoreFlags<8>;

    // Set a description for a specific bit position (0 to Bits-1)
    void set_description(size_t pos, std::string desc) {
        if (pos >= Bits) {
            throw std::out_of_range("Flag position out of range");
        }
        descriptions[pos] = std::move(desc);
    }

    // Individual bit operations
    void set_flag(size_t pos, bool value = true) {
        if (pos >= Bits) throw std::out_of_range("Flag position out of range");
        flags.set(pos, value);
    }

    void clear_flag(size_t pos) {
        set_flag(pos, false);
    }

    void toggle_flag(size_t pos) {
        if (pos >= Bits) throw std::out_of_range("Flag position out of range");
        flags.flip(pos);
    }

    // Set multiple flags at once from a list of positions
    void set_flags_from_positions(std::initializer_list<size_t> positions) {
        for (size_t pos : positions) {
            set_flag(pos);
        }
    }

    // Get descriptions of all currently set flags, lowest → highest bit
    std::vector<std::string> get_set_flags() const {
        std::vector<std::string> set_descs;
        set_descs.reserve(flags.count());  // Pre-reserve for efficiency

        for (size_t i = 0; i < Bits; ++i) {
            if (flags.test(i)) {
                const auto& desc = descriptions[i];
                // Only include if a description was explicitly set (skip empty)
                if (!desc.empty()) {
                    set_descs.push_back(desc);
                }
            }
        }
        return set_descs;
    }

    // Convenience: pretty-print set flags as a comma-separated string
    std::string to_string() const {
        auto set_flags = get_set_flags();
        if (set_flags.empty()) return "<none>";

        std::ostringstream oss;
        for (size_t i = 0; i < set_flags.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << set_flags[i];
        }
        return oss.str();
    }

    // Raw access (if needed)
    const std::bitset<Bits>& raw_flags() const { return flags; }
    void set_raw_flags(const std::bitset<Bits>& new_flags) { flags = new_flags; }

    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> bytes(Bytes, 0);
        for (size_t global_bit = 0; global_bit < Bits; ++global_bit) {
            if (flags.test(global_bit)) {
                size_t rev_bit = Bits - 1 - global_bit;
                size_t byte_idx = rev_bit / 8;
                size_t bit_pos = 7 - (rev_bit % 8);
                bytes[byte_idx] |= uint8_t(1) << bit_pos;
            }
        }
        return bytes;
    }

    void from_bytes(const std::vector<uint8_t>& bytes) {
        if (bytes.size() != Bytes) {
            throw std::invalid_argument("Byte vector size must equal Bytes template parameter");
        }
        flags.reset();
        for (size_t byte_idx = 0; byte_idx < Bytes; ++byte_idx) {
            uint8_t b = bytes[byte_idx];
            for (size_t bit_pos = 0; bit_pos < 8; ++bit_pos) {
                if (b & (uint8_t(1) << bit_pos)) {
                    size_t rev_bit = byte_idx * 8 + (7 - bit_pos);
                    if (rev_bit >= Bits) continue;  // safety (should not happen)
                    size_t global_bit = Bits - 1 - rev_bit;
                    flags.set(global_bit);
                }
            }
        }
    }

    void from_host_uint64(uint64_t host_value) {
        flags.reset();
        for (size_t i = 0; i < 64 && i < Bits; ++i) {
            if (host_value & (1ULL << i)) flags.set(i);
        }
    }

    void load_from_host(uint64_t host_value) {
        flags.reset();
        for (size_t i = 0; i < 64 && i < Bits; ++i) {
            if (host_value & (1ULL << i)) {
                flags.set(i);
            }
        }
    }

    static constexpr size_t get_severity_mask_from_enum(Severity sev) noexcept {
        return static_cast<uint64_t>(sev) << Bit_Severity_LSB;
    }

    static constexpr size_t get_severity_mask_from_index(size_t index) noexcept {
        return (index & 0b111) << Bit_Severity_LSB;  // clamps to 3 bits
    }

private:
    std::bitset<Bits> flags{};
    std::array<std::string, Bits> descriptions{};
};

template<size_t Bytes>
void init_event_flag_descriptions(TsStoreFlags<Bytes>& f) {
    f.set_description(TsStoreFlags<Bytes>::Bit_LogConsole,    "LogConsole");
    f.set_description(TsStoreFlags<Bytes>::Bit_KeeperRecord,  "KeeperRecord");
    f.set_description(TsStoreFlags<Bytes>::Bit_DatabaseEntry, "DatabaseEntry");
    f.set_description(TsStoreFlags<Bytes>::Bit_SendNetwork,   "SendNetwork");
    f.set_description(TsStoreFlags<Bytes>::Bit_HotCacheHint,  "HotCacheHint");
    f.set_description(TsStoreFlags<Bytes>::Bit_HasData,       "HasData");
    f.set_description(TsStoreFlags<Bytes>::Bit_IsExplicitNull,"IsExplicitNull");
    f.set_description(TsStoreFlags<Bytes>::Bit_IsInvalid,     "IsInvalid");
    f.set_description(TsStoreFlags<Bytes>::Bit_IsResultData,  "IsResultData");
}