#include "audio_library/track.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include "csv_parser.h"
#include "json_parser.h"
#include <cmath>
#include <stdexcept>

namespace audio_library {

Track::Track(std::string title, std::string artist, std::chrono::seconds duration)
    : title_(std::move(title))
    , artist_(std::move(artist))
    , duration_(duration) {
}

Track::Track(std::string title, std::string artist, int duration_seconds)
    : Track(std::move(title), std::move(artist), std::chrono::seconds(duration_seconds)) {
}

void Track::set_rating(double rating) {
    rating_ = std::clamp(rating, 0.0, 5.0);
}

std::string Track::format_duration() const {
    auto total_seconds = duration_.count();
    auto hours = total_seconds / 3600;
    auto minutes = (total_seconds % 3600) / 60;
    auto seconds = total_seconds % 60;
    
    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << ":" << std::setfill('0') << std::setw(2) << minutes 
            << ":" << std::setw(2) << seconds;
    } else {
        oss << minutes << ":" << std::setfill('0') << std::setw(2) << seconds;
    }
    return oss.str();
}

bool Track::matches_query(const std::string& query) const {
    // Convert query to lowercase for case-insensitive search
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    // Check each field
    auto contains_query = [&lower_query](const std::string& field) {
        std::string lower_field = field;
        std::transform(lower_field.begin(), lower_field.end(), lower_field.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower_field.find(lower_query) != std::string::npos;
    };
    
    return contains_query(title_) || contains_query(artist_) || 
           contains_query(album_) || contains_query(genre_);
}

std::string Track::to_string() const {
    std::ostringstream oss;
    oss << "\"" << title_ << "\" by " << artist_ << " (" << format_duration() << ")";
    if (!album_.empty()) {
        oss << " from \"" << album_ << "\"";
    }
    if (year_ > 0) {
        oss << " [" << year_ << "]";
    }
    if (rating_ > 0) {
        oss << " Rating: " << std::fixed << std::setprecision(1) << rating_ << "/5.0";
    }
    return oss.str();
}

std::string Track::to_csv() const {
    auto escape_csv = [](const std::string& field) {
        if (field.find_first_of(",\"\n\r") != std::string::npos) {
            std::string escaped = "\"";
            for (char c : field) {
                if (c == '"') escaped += "\"\"";
                else escaped += c;
            }
            escaped += "\"";
            return escaped;
        }
        return field;
    };
    
    std::ostringstream oss;
    oss << escape_csv(title_) << ","
        << escape_csv(artist_) << ","
        << duration_.count() << ","
        << escape_csv(album_) << ","
        << escape_csv(genre_) << ","
        << year_ << ","
        << play_count_ << ","
        << std::fixed << std::setprecision(1) << rating_;
    return oss.str();
}

std::string Track::to_json() const {
    auto escape_json = [](const std::string& str) {
        if (!detail::valid_utf8(str)) throw std::runtime_error("Invalid UTF-8 in track metadata");
        std::string escaped;
        for (unsigned char c : str) {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\b': escaped += "\\b"; break;
                case '\f': escaped += "\\f"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default:
                    if (c >= 0x20) {
                        escaped += static_cast<char>(c);
                    } else {
                        // JSON requires escaping control characters.
                        std::ostringstream oss;
                        oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                            << static_cast<int>(static_cast<unsigned char>(c));
                        escaped += oss.str();
                    }
            }
        }
        return escaped;
    };
    
    std::ostringstream oss;
    oss << "{"
        << "\"title\":\"" << escape_json(title_) << "\","
        << "\"artist\":\"" << escape_json(artist_) << "\","
        << "\"duration\":" << duration_.count() << ",";
    
    if (!album_.empty()) {
        oss << "\"album\":\"" << escape_json(album_) << "\",";
    }
    if (!genre_.empty()) {
        oss << "\"genre\":\"" << escape_json(genre_) << "\",";
    }
    if (year_ > 0) {
        oss << "\"year\":" << year_ << ",";
    }
    if (play_count_ > 0) {
        oss << "\"play_count\":" << play_count_ << ",";
    }
    if (rating_ > 0) {
        oss << "\"rating\":" << std::fixed << std::setprecision(1) << rating_ << ",";
    }
    
    // Remove trailing comma
    std::string result = oss.str();
    if (result.back() == ',') {
        result.pop_back();
    }
    result += "}";
    
    return result;
}

bool Track::operator==(const Track& other) const noexcept {
    return title_ == other.title_ && artist_ == other.artist_;
}

bool Track::operator<(const Track& other) const noexcept {
    if (artist_ != other.artist_) {
        return artist_ < other.artist_;
    }
    if (title_ != other.title_) {
        return title_ < other.title_;
    }
    return album_ < other.album_;
}

std::optional<Track> Track::from_csv_line(const std::string& line) {
    const auto fields = detail::csv_fields(line);

    // Parse fields (minimum required: title, artist, duration)
    if (fields.size() < 3 || fields.size() > 8) {
        return std::nullopt;
    }
    
    try {
        auto integer = [](const std::string& value) {
            size_t used = 0;
            const auto result = std::stoi(value, &used);
            if (used != value.size()) throw std::runtime_error("Invalid integer");
            return result;
        };
        Track track(fields[0], fields[1], integer(fields[2]));
        
        // Optional fields
        if (fields.size() > 3 && !fields[3].empty()) {
            track.set_album(fields[3]);
        }
        if (fields.size() > 4 && !fields[4].empty()) {
            track.set_genre(fields[4]);
        }
        if (fields.size() > 5 && !fields[5].empty()) {
            track.set_year(integer(fields[5]));
        }
        if (fields.size() > 6 && !fields[6].empty()) {
            size_t used = 0;
            if (fields[6].find('-') != std::string::npos) return std::nullopt;
            track.play_count_ = std::stoull(fields[6], &used);
            if (used != fields[6].size()) return std::nullopt;
        }
        if (fields.size() > 7 && !fields[7].empty()) {
            size_t used = 0;
            const auto rating = std::stod(fields[7], &used);
            if (used != fields[7].size() || !std::isfinite(rating) || rating < 0 || rating > 5) return std::nullopt;
            track.set_rating(rating);
        }
        
        return track;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<Track> Track::from_json(const std::string& json) {
    try {
        detail::JsonReader reader(json);
        const auto fields = reader.object();
        reader.finish();
        using Kind = detail::JsonReader::Field::Kind;
        auto get = [&fields](const std::string& key, Kind kind) -> const std::string& {
            const auto& field = fields.at(key);
            if (field.kind != kind) throw std::runtime_error("Wrong JSON field type");
            return field.text;
        };
        auto integer = [&get](const std::string& key) {
            const auto& text = get(key, Kind::Number);
            size_t used = 0;
            const auto result = std::stoi(text, &used);
            if (used != text.size()) throw std::runtime_error("Expected an integer");
            return result;
        };
        Track track(get("title", Kind::String), get("artist", Kind::String), integer("duration"));
        if (fields.count("album")) track.set_album(get("album", Kind::String));
        if (fields.count("genre")) track.set_genre(get("genre", Kind::String));
        if (fields.count("year")) track.set_year(integer("year"));
        if (fields.count("play_count")) {
            const auto& text = get("play_count", Kind::Number);
            size_t used = 0;
            if (text.front() == '-') return std::nullopt;
            track.play_count_ = std::stoull(text, &used);
            if (used != text.size()) return std::nullopt;
        }
        if (fields.count("rating")) {
            const auto& text = get("rating", Kind::Number);
            size_t used = 0;
            const auto rating = std::stod(text, &used);
            if (used != text.size() || !std::isfinite(rating) || rating < 0 || rating > 5) return std::nullopt;
            track.set_rating(rating);
        }
        return track;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::size_t TrackHash::operator()(const Track& track) const noexcept {
    // Combine hashes of title and artist
    std::size_t h1 = std::hash<std::string>{}(track.title());
    std::size_t h2 = std::hash<std::string>{}(track.artist());
    
    // Use a better hash combination technique
    return h1 ^ (h2 << 1);
}

} // namespace audio_library
