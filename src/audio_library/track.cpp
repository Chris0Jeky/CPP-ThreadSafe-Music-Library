#include "audio_library/track.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_map>

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
        std::string escaped;
        for (char c : str) {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\b': escaped += "\\b"; break;
                case '\f': escaped += "\\f"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default:
                    if (c >= 0x20 && c <= 0x7E) {
                        escaped += c;
                    } else {
                        // Unicode escape for non-ASCII characters
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
    // Simple CSV parser (handles quoted fields with embedded commas)
    std::vector<std::string> fields;
    std::string current_field;
    bool in_quotes = false;
    bool quote_escaped = false;
    
    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        
        if (quote_escaped) {
            if (c == '"') {
                current_field += '"';
            } else {
                // Not an escaped quote, end the quoted field
                in_quotes = false;
                --i; // Process this character again
            }
            quote_escaped = false;
        } else if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.length() && line[i + 1] == '"') {
                    quote_escaped = true;
                } else {
                    in_quotes = false;
                }
            } else {
                current_field += c;
            }
        } else {
            if (c == '"' && current_field.empty()) {
                in_quotes = true;
            } else if (c == ',') {
                fields.push_back(current_field);
                current_field.clear();
            } else {
                current_field += c;
            }
        }
    }
    fields.push_back(current_field);
    
    // Parse fields (minimum required: title, artist, duration)
    if (fields.size() < 3) {
        return std::nullopt;
    }
    
    try {
        Track track(fields[0], fields[1], std::stoi(fields[2]));
        
        // Optional fields
        if (fields.size() > 3 && !fields[3].empty()) {
            track.set_album(fields[3]);
        }
        if (fields.size() > 4 && !fields[4].empty()) {
            track.set_genre(fields[4]);
        }
        if (fields.size() > 5 && !fields[5].empty()) {
            track.set_year(std::stoi(fields[5]));
        }
        if (fields.size() > 6 && !fields[6].empty()) {
            track.play_count_ = std::stoull(fields[6]);
        }
        if (fields.size() > 7 && !fields[7].empty()) {
            track.set_rating(std::stod(fields[7]));
        }
        
        return track;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<Track> Track::from_json(const std::string& json) {
    // Simple JSON parser for track objects
    static const std::regex field_regex("\"(\\w+)\"\\s*:\\s*(\"([^\"\\\\]|\\\\.)*\"|\\d+(\\.\\d+)?)");
    
    std::unordered_map<std::string, std::string> fields;
    
    auto matches_begin = std::sregex_iterator(json.begin(), json.end(), field_regex);
    auto matches_end = std::sregex_iterator();
    
    for (auto it = matches_begin; it != matches_end; ++it) {
        std::smatch match = *it;
        std::string key = match[1];
        std::string value = match[2];
        
        // Remove quotes from string values
        if (value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.length() - 2);
            
            // Unescape JSON string
            std::string unescaped;
            for (size_t i = 0; i < value.length(); ++i) {
                if (value[i] == '\\' && i + 1 < value.length()) {
                    switch (value[i + 1]) {
                        case '"': unescaped += '"'; ++i; break;
                        case '\\': unescaped += '\\'; ++i; break;
                        case 'b': unescaped += '\b'; ++i; break;
                        case 'f': unescaped += '\f'; ++i; break;
                        case 'n': unescaped += '\n'; ++i; break;
                        case 'r': unescaped += '\r'; ++i; break;
                        case 't': unescaped += '\t'; ++i; break;
                        default: unescaped += value[i];
                    }
                } else {
                    unescaped += value[i];
                }
            }
            value = unescaped;
        }
        
        fields[key] = value;
    }
    
    // Check required fields
    if (fields.find("title") == fields.end() || 
        fields.find("artist") == fields.end() || 
        fields.find("duration") == fields.end()) {
        return std::nullopt;
    }
    
    try {
        Track track(fields["title"], fields["artist"], std::stoi(fields["duration"]));
        
        // Optional fields
        if (fields.find("album") != fields.end()) {
            track.set_album(fields["album"]);
        }
        if (fields.find("genre") != fields.end()) {
            track.set_genre(fields["genre"]);
        }
        if (fields.find("year") != fields.end()) {
            track.set_year(std::stoi(fields["year"]));
        }
        if (fields.find("play_count") != fields.end()) {
            track.play_count_ = std::stoull(fields["play_count"]);
        }
        if (fields.find("rating") != fields.end()) {
            track.set_rating(std::stod(fields["rating"]));
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
