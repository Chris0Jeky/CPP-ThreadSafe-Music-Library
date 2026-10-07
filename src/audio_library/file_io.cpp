#include "audio_library/file_io.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <iomanip>

namespace audio_library {

std::vector<Track> FileIO::import_csv(const std::filesystem::path& path, 
                                     const ImportOptions& options) {
    std::vector<Track> tracks;
    std::ifstream file(path);
    
    if (!file.is_open()) {
        if (options.error_callback) {
            options.error_callback("Cannot open file: " + path.string(), 0);
        }
        return tracks;
    }
    
    std::string line;
    size_t line_number = 0;
    size_t error_count = 0;
    
    // Check if first line is a header
    if (std::getline(file, line)) {
        ++line_number;
        
        // Simple heuristic: if the third field is not a number, it's likely a header
        auto fields = split_csv_line(line);
        if (fields.size() >= 3) {
            try {
                static_cast<void>(std::stoi(fields[2]));
                // It's a data line, process it
                file.seekg(0);
                line_number = 0;
            } catch (...) {
                // It's a header, skip it
            }
        }
    }
    
    // Process data lines
    while (std::getline(file, line)) {
        ++line_number;
        
        if (options.progress_callback && line_number % 100 == 0) {
            options.progress_callback(line_number, 0); // Total unknown for streaming
        }
        
        auto track_opt = Track::from_csv_line(line);
        
        if (track_opt) {
            if (!options.validate_data || validate_track_data(*track_opt)) {
                tracks.push_back(std::move(*track_opt));
            } else {
                ++error_count;
                if (options.error_callback) {
                    options.error_callback("Invalid track data at line " + 
                                         std::to_string(line_number), line_number);
                }
            }
        } else {
            ++error_count;
            if (options.error_callback) {
                options.error_callback("Failed to parse line " + 
                                     std::to_string(line_number) + ": " + line, line_number);
            }
            if (!options.skip_invalid_lines) {
                break;
            }
        }
        
        if (error_count >= options.max_errors) {
            if (options.error_callback) {
                options.error_callback("Too many errors, stopping import", line_number);
            }
            break;
        }
    }
    
    return tracks;
}

bool FileIO::export_csv(const std::filesystem::path& path, 
                       const std::vector<Track>& tracks,
                       const ExportOptions& options) {
    std::ofstream file(path);
    
    if (!file.is_open()) {
        return false;
    }
    
    // Write header
    if (options.include_metadata) {
        file << "Title,Artist,Duration,Album,Genre,Year,PlayCount,Rating\n";
    } else {
        file << "Title,Artist,Duration\n";
    }
    
    // Write tracks
    size_t count = 0;
    for (const auto& track : tracks) {
        if (options.include_metadata) {
            file << track.to_csv() << "\n";
        } else {
            file << escape_csv_field(track.title()) << ","
                 << escape_csv_field(track.artist()) << ","
                 << track.duration_seconds() << "\n";
        }
        
        ++count;
        if (options.progress_callback && count % 100 == 0) {
            options.progress_callback(count, tracks.size());
        }
    }
    
    return file.good();
}

bool FileIO::export_csv(const std::filesystem::path& path, 
                       const std::vector<TrackPtr>& tracks,
                       const ExportOptions& options) {
    std::vector<Track> track_copies;
    track_copies.reserve(tracks.size());
    
    for (const auto& ptr : tracks) {
        if (ptr) {
            track_copies.push_back(*ptr);
        }
    }
    
    return export_csv(path, track_copies, options);
}

std::vector<Track> FileIO::import_json(const std::filesystem::path& path,
                                      const ImportOptions& options) {
    std::vector<Track> tracks;
    std::ifstream file(path);
    
    if (!file.is_open()) {
        if (options.error_callback) {
            options.error_callback("Cannot open file: " + path.string(), 0);
        }
        return tracks;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    // Simple JSON array parser
    size_t pos = content.find('[');
    if (pos == std::string::npos) {
        if (options.error_callback) {
            options.error_callback("Invalid JSON format: no array found", 0);
        }
        return tracks;
    }
    
    size_t end = content.rfind(']');
    if (end == std::string::npos || end <= pos) {
        if (options.error_callback) {
            options.error_callback("Invalid JSON format: array not closed", 0);
        }
        return tracks;
    }
    
    // Extract objects from array
    size_t obj_start = pos + 1;
    size_t error_count = 0;
    size_t track_count = 0;
    
    while (obj_start < end) {
        // Skip whitespace
        while (obj_start < end && std::isspace(content[obj_start])) {
            ++obj_start;
        }
        
        if (obj_start >= end) break;
        
        // Find object boundaries
        if (content[obj_start] == '{') {
            int brace_count = 1;
            size_t obj_end = obj_start + 1;
            
            while (obj_end < end && brace_count > 0) {
                if (content[obj_end] == '{') ++brace_count;
                else if (content[obj_end] == '}') --brace_count;
                ++obj_end;
            }
            
            if (brace_count == 0) {
                std::string json_obj = content.substr(obj_start, obj_end - obj_start);
                auto track_opt = Track::from_json(json_obj);
                
                if (track_opt) {
                    if (!options.validate_data || validate_track_data(*track_opt)) {
                        tracks.push_back(std::move(*track_opt));
                        ++track_count;
                        
                        if (options.progress_callback && track_count % 100 == 0) {
                            options.progress_callback(track_count, 0);
                        }
                    } else {
                        ++error_count;
                        if (options.error_callback) {
                            options.error_callback("Invalid track data in JSON", track_count);
                        }
                    }
                } else if (!options.skip_invalid_lines) {
                    ++error_count;
                    if (options.error_callback) {
                        options.error_callback("Failed to parse JSON object", track_count);
                    }
                } else {
                    ++error_count;
                }
                
                if (error_count >= options.max_errors) {
                    if (options.error_callback) {
                        options.error_callback("Too many errors, stopping import", track_count);
                    }
                    break;
                }
                
                // Move to next object
                obj_start = obj_end;
                while (obj_start < end && (std::isspace(content[obj_start]) || 
                                         content[obj_start] == ',')) {
                    ++obj_start;
                }
            } else {
                break; // Malformed JSON
            }
        } else {
            break; // Not an object
        }
    }
    
    return tracks;
}

bool FileIO::export_json(const std::filesystem::path& path,
                        const std::vector<Track>& tracks,
                        const ExportOptions& options) {
    std::ofstream file(path);
    
    if (!file.is_open()) {
        return false;
    }
    
    file << "[";
    if (options.pretty_print) file << "\n";
    
    size_t count = 0;
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (options.pretty_print) file << "  ";
        
        file << tracks[i].to_json();
        
        if (i < tracks.size() - 1) {
            file << ",";
        }
        
        if (options.pretty_print) file << "\n";
        
        ++count;
        if (options.progress_callback && count % 100 == 0) {
            options.progress_callback(count, tracks.size());
        }
    }
    
    file << "]";
    if (options.pretty_print) file << "\n";
    
    return file.good();
}

bool FileIO::export_json(const std::filesystem::path& path,
                        const std::vector<TrackPtr>& tracks,
                        const ExportOptions& options) {
    std::vector<Track> track_copies;
    track_copies.reserve(tracks.size());
    
    for (const auto& ptr : tracks) {
        if (ptr) {
            track_copies.push_back(*ptr);
        }
    }
    
    return export_json(path, track_copies, options);
}

std::vector<std::string> FileIO::import_playlist(const std::filesystem::path& path) {
    std::vector<std::string> filenames;
    std::ifstream file(path);
    
    if (!file.is_open()) {
        return filenames;
    }
    
    std::string line;
    bool extended_m3u = false;
    
    // Check for extended M3U format
    if (std::getline(file, line)) {
        if (line == "#EXTM3U") {
            extended_m3u = true;
        } else {
            // Not extended format, process first line
            file.seekg(0);
        }
    }
    
    while (std::getline(file, line)) {
        // Skip comments and metadata in extended format
        if (line.empty() || (extended_m3u && line[0] == '#')) {
            continue;
        }
        
        filenames.push_back(line);
    }
    
    return filenames;
}

bool FileIO::export_playlist(const std::filesystem::path& path,
                            const std::vector<Track>& tracks,
                            const std::string& playlist_name) {
    std::ofstream file(path);
    
    if (!file.is_open()) {
        return false;
    }
    
    // Write extended M3U header
    file << "#EXTM3U\n";
    
    if (!playlist_name.empty()) {
        file << "#PLAYLIST:" << playlist_name << "\n";
    }
    
    // Write tracks
    for (const auto& track : tracks) {
        // Write extended info
        file << "#EXTINF:" << track.duration_seconds() << ","
             << track.artist() << " - " << track.title() << "\n";
        
        // In a real implementation, we'd write the actual file path
        // For now, we'll use a placeholder format
        file << track.artist() << " - " << track.title() << ".mp3\n";
    }
    
    return file.good();
}

bool FileIO::validate_csv_format(const std::filesystem::path& path) {
    std::ifstream file(path);
    
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    if (!std::getline(file, line)) {
        return false;
    }
    
    auto fields = split_csv_line(line);
    
    // Check if it's a valid header or data line
    if (fields.size() < 3) {
        return false;
    }
    
    // Try to parse as data line
    auto track = Track::from_csv_line(line);
    if (track) {
        return true;
    }
    
    // Check if it's a header line
    // Common header variations
    std::vector<std::string> valid_headers = {
        "title", "artist", "duration",
        "Title", "Artist", "Duration",
        "TITLE", "ARTIST", "DURATION"
    };
    
    for (size_t i = 0; i < std::min(fields.size(), size_t(3)); ++i) {
        std::string lower_field = fields[i];
        std::transform(lower_field.begin(), lower_field.end(), lower_field.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        
        bool found = false;
        for (const auto& header : valid_headers) {
            std::string lower_header = header;
            std::transform(lower_header.begin(), lower_header.end(), lower_header.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            
            if (lower_field.find(lower_header) != std::string::npos) {
                found = true;
                break;
            }
        }
        
        if (!found && i < 3) {
            return false;
        }
    }
    
    return true;
}

bool FileIO::validate_json_format(const std::filesystem::path& path) {
    std::ifstream file(path);
    
    if (!file.is_open()) {
        return false;
    }
    
    // Simple JSON validation - check for array brackets and at least one object
    char ch;
    bool found_array_start = false;
    bool found_object_start = false;
    
    while (file >> ch) {
        if (ch == '[') {
            found_array_start = true;
        } else if (ch == '{' && found_array_start) {
            found_object_start = true;
            break;
        } else if (!std::isspace(ch)) {
            // Non-whitespace before array start
            return false;
        }
    }
    
    return found_array_start && found_object_start;
}

std::optional<std::string> FileIO::detect_file_format(const std::filesystem::path& path) {
    // Check extension first
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    if (ext == ".csv") {
        if (validate_csv_format(path)) {
            return "csv";
        }
    } else if (ext == ".json") {
        if (validate_json_format(path)) {
            return "json";
        }
    } else if (ext == ".m3u" || ext == ".m3u8") {
        return "m3u";
    }
    
    // Try content-based detection
    if (validate_json_format(path)) {
        return "json";
    }
    
    if (validate_csv_format(path)) {
        return "csv";
    }
    
    return std::nullopt;
}

std::vector<std::string> FileIO::split_csv_line(const std::string& line) {
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
                in_quotes = false;
                --i;
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
    return fields;
}

std::string FileIO::escape_csv_field(const std::string& field) {
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
}

bool FileIO::validate_track_data(const Track& track) {
    // Basic validation rules
    if (track.title().empty() || track.artist().empty()) {
        return false;
    }
    
    if (track.duration_seconds() <= 0 || track.duration_seconds() > 36000) { // Max 10 hours
        return false;
    }
    
    if (track.year() < 0 || (track.year() > 0 && track.year() < 1900) || 
        track.year() > 2100) {
        return false;
    }
    
    if (track.rating() < 0.0 || track.rating() > 5.0) {
        return false;
    }
    
    return true;
}

} // namespace audio_library
