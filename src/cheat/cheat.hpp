#pragma once
#include "types.hpp"
#include <string>
#include <vector>

class Bus;

struct RawCode {
    u32 first;
    u32 second;
};

struct DecodedCode {
    u8 type;
    u32 addr;
    u32 value;
};

struct CheatEntry {
    std::string name;
    std::vector<RawCode> raw_codes;
    std::vector<DecodedCode> decoded;
    bool enabled;
};

class CheatEngine {
public:
    void set_bus(Bus* bus) { bus_ = bus; }

    bool load(const std::string& path);
    bool save(const std::string& path);
    void apply();

    void toggle(int index);
    void remove(int index);
    bool add(const std::string& name, const std::string& code_text);

    int count() const { return (int)cheats_.size(); }
    const CheatEntry& entry(int i) const { return cheats_[i]; }
    bool dirty() const { return dirty_; }

    static constexpr u8 TYPE_WRITE8 = 0;
    static constexpr u8 TYPE_WRITE16 = 1;
    static constexpr u8 TYPE_WRITE32 = 2;
    static constexpr u8 TYPE_ROM_PATCH = 6;
    static constexpr u8 TYPE_COND_EQ = 0xD;
    static constexpr u8 TYPE_SKIP = 0xFE;

private:
    Bus* bus_ = nullptr;
    std::vector<CheatEntry> cheats_;
    bool dirty_ = false;

    u32 seeds_v1_[4];
    u32 seeds_v3_[4];

    void reset_seeds();
    static void tea_decrypt(u32& v0, u32& v1, const u32 key[4]);
    static void reseed_v1(u32* seeds, u16 params);
    static void reseed_v3(u32* seeds, u16 params);
    void decode_entry_v1(CheatEntry& entry);
    void decode_entry_v3(CheatEntry& entry);
    void decode_entry(CheatEntry& entry);
    static bool parse_code_line(const std::string& line, u32& first, u32& second);
};
