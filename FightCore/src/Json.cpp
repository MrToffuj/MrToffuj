#include "fightcore/Json.h"

namespace fc::json {
namespace {

struct Parser {
    std::string_view s;
    size_t p = 0;
    std::string error;

    bool fail(const char* what) {
        if (error.empty()) error = std::string(what) + " at offset " + std::to_string(p);
        return false;
    }
    void skip() {
        while (p < s.size()) {
            const char c = s[p];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++p; continue; }
            if (c == '/' && p + 1 < s.size() && s[p + 1] == '/') { // allow // comments, handy in hand-written mods
                while (p < s.size() && s[p] != '\n') ++p;
                continue;
            }
            break;
        }
    }
    bool literal(std::string_view word) {
        if (s.substr(p, word.size()) != word) return false;
        p += word.size();
        return true;
    }
    bool string(std::string& out) {
        if (p >= s.size() || s[p] != '"') return fail("expected string");
        ++p;
        while (p < s.size() && s[p] != '"') {
            char c = s[p++];
            if (c == '\\' && p < s.size()) {
                const char e = s[p++];
                switch (e) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'u': // keep it simple: only ASCII escapes
                        if (p + 4 > s.size()) return fail("bad escape");
                        c = static_cast<char>(std::stoi(std::string(s.substr(p, 4)), nullptr, 16) & 0x7f);
                        p += 4;
                        break;
                    default: c = e; break;
                }
            }
            out.push_back(c);
        }
        if (p >= s.size()) return fail("unterminated string");
        ++p;
        return true;
    }
    bool value(Value& v, int depth) {
        if (depth > 32) return fail("nested too deep");
        skip();
        if (p >= s.size()) return fail("unexpected end");
        const char c = s[p];
        if (c == '{') {
            v.type = Value::Type::Object;
            ++p;
            skip();
            if (p < s.size() && s[p] == '}') { ++p; return true; }
            while (true) {
                skip();
                std::string key;
                if (!string(key)) return false;
                skip();
                if (p >= s.size() || s[p] != ':') return fail("expected ':'");
                ++p;
                Value child;
                if (!value(child, depth + 1)) return false;
                v.members.emplace_back(std::move(key), std::move(child));
                skip();
                if (p < s.size() && s[p] == ',') { ++p; continue; }
                if (p < s.size() && s[p] == '}') { ++p; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            v.type = Value::Type::Array;
            ++p;
            skip();
            if (p < s.size() && s[p] == ']') { ++p; return true; }
            while (true) {
                Value child;
                if (!value(child, depth + 1)) return false;
                v.items.push_back(std::move(child));
                skip();
                if (p < s.size() && s[p] == ',') { ++p; continue; }
                if (p < s.size() && s[p] == ']') { ++p; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { v.type = Value::Type::String; return string(v.text); }
        if (literal("true")) { v.type = Value::Type::Bool; v.boolean = true; return true; }
        if (literal("false")) { v.type = Value::Type::Bool; return true; }
        if (literal("null")) return true;
        if (c == '-' || (c >= '0' && c <= '9')) {
            const size_t start = p;
            if (s[p] == '-') ++p;
            while (p < s.size() && ((s[p] >= '0' && s[p] <= '9') || s[p] == '.')) ++p;
            if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) return fail("exponents are not supported");
            v.type = Value::Type::Number;
            v.text = std::string(s.substr(start, p - start));
            return true;
        }
        return fail("unexpected character");
    }
};

} // namespace

const Value* Value::get(std::string_view key) const {
    for (const auto& [k, v] : members)
        if (k == key) return &v;
    return nullptr;
}

int32_t Value::asInt(int32_t fallback) const {
    if (type != Type::Number) return fallback;
    int64_t v = 0;
    size_t i = text[0] == '-' ? 1 : 0;
    for (; i < text.size() && text[i] != '.'; ++i) {
        v = v * 10 + (text[i] - '0');
        if (v > 1000000000) return fallback;
    }
    return static_cast<int32_t>(text[0] == '-' ? -v : v); // decimals are dropped
}

Fx Value::asFx(Fx fallback) const {
    if (type != Type::Number) return fallback;
    size_t i = 0;
    const bool negative = !text.empty() && text[0] == '-';
    if (negative) ++i;
    int64_t whole = 0;
    while (i < text.size() && text[i] != '.') {
        whole = whole * 10 + (text[i] - '0');
        if (whole > 30000) return fallback; // out of fixed point range
        ++i;
    }
    int64_t frac = 0, scale = 1;
    if (i < text.size() && text[i] == '.') {
        ++i;
        for (int digits = 0; i < text.size() && digits < 6; ++i, ++digits) {
            frac = frac * 10 + (text[i] - '0');
            scale *= 10;
        }
    }
    const int64_t raw = whole * Fx::kOne + frac * Fx::kOne / scale;
    return Fx::fromRaw(static_cast<int32_t>(negative ? -raw : raw));
}

bool Value::asBool(bool fallback) const { return type == Type::Bool ? boolean : fallback; }
std::string_view Value::asString(std::string_view fallback) const { return type == Type::String ? std::string_view(text) : fallback; }

int32_t Value::intOr(std::string_view key, int32_t fallback) const {
    const Value* v = get(key);
    return v ? v->asInt(fallback) : fallback;
}
Fx Value::fxOr(std::string_view key, Fx fallback) const {
    const Value* v = get(key);
    return v ? v->asFx(fallback) : fallback;
}
bool Value::boolOr(std::string_view key, bool fallback) const {
    const Value* v = get(key);
    return v ? v->asBool(fallback) : fallback;
}
std::string_view Value::stringOr(std::string_view key, std::string_view fallback) const {
    const Value* v = get(key);
    return v ? v->asString(fallback) : fallback;
}

bool parse(std::string_view text, Value& out, std::string* error) {
    Parser parser{text, 0, {}};
    out = Value{};
    bool ok = parser.value(out, 0);
    if (ok) {
        parser.skip();
        if (parser.p != text.size()) ok = parser.fail("trailing characters");
    }
    if (!ok && error) *error = parser.error;
    return ok;
}

} // namespace fc::json
