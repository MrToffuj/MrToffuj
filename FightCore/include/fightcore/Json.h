#pragma once
// Minimal JSON reader for mod files. Numbers are kept as text and converted to fixed point
// exactly, so a mod gives every peer bit-identical values.
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fightcore/Fixed.h"

namespace fc::json {

struct Value {
    enum class Type : uint8_t { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    std::string text; // string contents, or the number as written
    std::vector<Value> items;
    std::vector<std::pair<std::string, Value>> members;

    const Value* get(std::string_view key) const;
    bool has(std::string_view key) const { return get(key) != nullptr; }

    int32_t asInt(int32_t fallback = 0) const;
    Fx asFx(Fx fallback = {}) const;
    bool asBool(bool fallback = false) const;
    std::string_view asString(std::string_view fallback = "") const;

    // shorthands for object members
    int32_t intOr(std::string_view key, int32_t fallback) const;
    Fx fxOr(std::string_view key, Fx fallback) const;
    bool boolOr(std::string_view key, bool fallback) const;
    std::string_view stringOr(std::string_view key, std::string_view fallback) const;
};

bool parse(std::string_view text, Value& out, std::string* error = nullptr);

} // namespace fc::json
