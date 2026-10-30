#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <utility>

namespace audio_library::detail {

inline bool valid_utf8(std::string_view text) {
    for (size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i++]);
        if (first < 0x80) continue;
        unsigned count;
        uint32_t code;
        uint32_t minimum;
        if (first >= 0xc2 && first <= 0xdf) {
            count = 1; code = first & 0x1f; minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            count = 2; code = first & 0x0f; minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            count = 3; code = first & 0x07; minimum = 0x10000;
        } else return false;
        if (text.size() - i < count) return false;
        while (count--) {
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80) return false;
            code = (code << 6) | (next & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
    }
    return true;
}

// A bounded recursive-descent reader. Unknown metadata is validated and skipped.
class JsonReader {
public:
    struct Field {
        enum class Kind { String, Number, Other } kind = Kind::Other;
        std::string text;
    };
    using Fields = std::unordered_map<std::string, Field>;

    explicit JsonReader(std::string_view input) : input_(input) {}

    Fields object() {
        Fields fields;
        expect('{');
        if (take('}')) return fields;
        do {
            const auto key = string();
            expect(':');
            whitespace();
            Field field;
            if (peek() == '"') {
                field.kind = Field::Kind::String;
                field.text = string();
            } else if (peek() == '-' || digit(peek())) {
                field.kind = Field::Kind::Number;
                field.text = number();
            } else {
                value(1);
            }
            if (!fields.emplace(key, std::move(field)).second) fail();
        } while (take(','));
        expect('}');
        return fields;
    }

    std::vector<std::string_view> array_objects(size_t maximum) {
        std::vector<std::string_view> objects;
        expect('[');
        if (!take(']')) {
            do {
                whitespace();
                if (objects.size() >= maximum || peek() != '{') fail();
                const auto start = pos_;
                value(0);
                objects.push_back(input_.substr(start, pos_ - start));
            } while (take(','));
            expect(']');
        }
        finish();
        return objects;
    }

    void finish() {
        whitespace();
        if (pos_ != input_.size()) fail();
    }

private:
    std::string_view input_;
    size_t pos_ = 0;

    [[noreturn]] static void fail() { throw std::runtime_error("Invalid JSON or JSON limit exceeded"); }
    static bool digit(char c) { return c >= '0' && c <= '9'; }
    char peek() const { return pos_ < input_.size() ? input_[pos_] : '\0'; }
    void whitespace() {
        while (pos_ < input_.size() && (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n')) ++pos_;
    }
    bool take(char c) {
        whitespace();
        if (pos_ < input_.size() && peek() == c) { ++pos_; return true; }
        return false;
    }
    void expect(char c) { if (!take(c)) fail(); }

    uint32_t hex4() {
        uint32_t code = 0;
        for (int i = 0; i < 4; ++i) {
            if (pos_ == input_.size()) fail();
            const char c = input_[pos_++];
            code <<= 4;
            if (c >= '0' && c <= '9') code |= c - '0';
            else if (c >= 'a' && c <= 'f') code |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') code |= c - 'A' + 10;
            else fail();
        }
        return code;
    }
    static void append_utf8(std::string& out, uint32_t code) {
        if (code <= 0x7f) out += static_cast<char>(code);
        else if (code <= 0x7ff) {
            out += static_cast<char>(0xc0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3f));
        } else if (code <= 0xffff) {
            out += static_cast<char>(0xe0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (code & 0x3f));
        } else {
            out += static_cast<char>(0xf0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (code & 0x3f));
        }
    }
    std::string string() {
        expect('"');
        std::string out;
        while (pos_ < input_.size()) {
            const auto c = static_cast<unsigned char>(input_[pos_++]);
            if (c == '"') {
                if (!valid_utf8(out)) fail();
                return out;
            }
            if (c < 0x20) fail();
            if (c != '\\') { out += static_cast<char>(c); continue; }
            if (pos_ == input_.size()) fail();
            switch (input_[pos_++]) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    auto code = hex4();
                    if (code >= 0xd800 && code <= 0xdbff) {
                        if (pos_ + 2 > input_.size() || input_[pos_] != '\\' || input_[pos_ + 1] != 'u') fail();
                        pos_ += 2;
                        const auto low = hex4();
                        if (low < 0xdc00 || low > 0xdfff) fail();
                        code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
                    } else if (code >= 0xdc00 && code <= 0xdfff) fail();
                    append_utf8(out, code);
                    break;
                }
                default: fail();
            }
        }
        fail();
    }
    std::string number() {
        const auto start = pos_;
        if (peek() == '-') ++pos_;
        if (peek() == '0') ++pos_;
        else {
            if (!digit(peek())) fail();
            while (digit(peek())) ++pos_;
        }
        if (peek() == '.') {
            ++pos_;
            if (!digit(peek())) fail();
            while (digit(peek())) ++pos_;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') ++pos_;
            if (!digit(peek())) fail();
            while (digit(peek())) ++pos_;
        }
        return std::string(input_.substr(start, pos_ - start));
    }
    void value(size_t depth) {
        if (depth > 64) fail();
        whitespace();
        if (peek() == '"') { string(); return; }
        if (take('{')) {
            if (take('}')) return;
            do { string(); expect(':'); value(depth + 1); } while (take(','));
            expect('}');
        } else if (take('[')) {
            if (take(']')) return;
            do { value(depth + 1); } while (take(','));
            expect(']');
        } else if (peek() == '-' || digit(peek())) {
            number();
        } else {
            bool matched = false;
            for (const auto literal : {"true", "false", "null"}) {
                const std::string_view token(literal);
                if (input_.substr(pos_, token.size()) == token) {
                    pos_ += token.size(); matched = true; break;
                }
            }
            if (!matched) fail();
        }
    }
};

} // namespace audio_library::detail
