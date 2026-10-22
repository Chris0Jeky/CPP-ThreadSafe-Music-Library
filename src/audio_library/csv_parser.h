#pragma once

#include <string>
#include <vector>
#include <utility>

namespace audio_library::detail {

enum class CsvState { Start, Plain, Quoted, Closed };

inline std::vector<std::string> csv_fields(const std::string& record) {
    std::vector<std::string> fields;
    std::string field;
    auto state = CsvState::Start;
    for (const char c : record) {
        if (state == CsvState::Quoted) {
            if (c == '"') state = CsvState::Closed;
            else field += c;
        } else if (c == ',' ) {
            fields.push_back(std::move(field));
            field.clear();
            state = CsvState::Start;
        } else if (c == '"') {
            if (state == CsvState::Start) state = CsvState::Quoted;
            else if (state == CsvState::Closed) { field += '"'; state = CsvState::Quoted; }
            else return {};
        } else {
            if (state == CsvState::Closed || c == '\r' || c == '\n') return {};
            field += c;
            state = CsvState::Plain;
        }
    }
    if (state == CsvState::Quoted) return {};
    fields.push_back(std::move(field));
    return fields;
}

} // namespace audio_library::detail
