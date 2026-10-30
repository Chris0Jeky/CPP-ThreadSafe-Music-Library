#include "audio_library/file_io.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <iomanip>
#include "csv_parser.h"
#include "json_parser.h"
#include <stdexcept>
#include <string_view>

namespace audio_library {

namespace {
size_t skip_utf8_bom(std::ifstream& file) {
    char prefix[3];
    file.read(prefix, sizeof(prefix));
    if (file.gcount() == 3 && std::string_view(prefix, 3) == "\xEF\xBB\xBF") return 3;
    // Preserve read errors; a short/non-BOM prefix must be parsed from byte zero.
    if (!file.bad() && (file.good() || file.eof())) {
        file.clear();
        file.seekg(0);
    }
    return 0;
}

[[noreturn]] void import_failure(const FileIO::ImportOptions& options,
                                 const std::string& message, size_t line = 0) {
    if (options.error_callback) options.error_callback(message, line);
    throw std::runtime_error(message);
}

bool csv_header(const std::vector<std::string>& fields) {
    if (fields.size() < 3) return false;
    const char* names[] = {"title", "artist", "duration"};
    for (size_t i = 0; i < 3; ++i) {
        auto name = fields[i];
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name != names[i]) return false;
    }
    return true;
}

std::optional<std::string> csv_record(std::ifstream& file, const FileIO::ImportOptions& options,
                                      size_t& line, size_t& bytes) {
    std::string record;
    auto state = detail::CsvState::Start;
    size_t lines = 1;
    bool previous_cr = false;
    char c;
    while (file.get(c)) {
        if (++bytes > options.max_input_bytes) import_failure(options, "CSV input byte limit exceeded", line);
        const bool quoted = state == detail::CsvState::Quoted;
        if (c == '\r' || c == '\n') {
            if (c == '\r' || !previous_cr) {
                ++line;
                if (quoted && ++lines > options.max_record_lines)
                    import_failure(options, "CSV record line limit exceeded", line);
            }
            if (!quoted) {
                if (c == '\r' && file.peek() == '\n') {
                    file.get(c);
                    if (++bytes > options.max_input_bytes) import_failure(options, "CSV input byte limit exceeded", line);
                }
                return record;
            }
        }
        previous_cr = c == '\r';
        if (record.size() >= options.max_record_bytes || lines > options.max_record_lines)
            import_failure(options, "CSV record size limit exceeded", line);
        record += c;
        if (quoted) {
            if (c == '"') state = detail::CsvState::Closed;
        } else if (c == '"' && (state == detail::CsvState::Start || state == detail::CsvState::Closed)) {
            state = detail::CsvState::Quoted;
        } else if (c == ',') state = detail::CsvState::Start;
        else state = detail::CsvState::Plain;
    }
    if (file.bad() || !file.eof()) import_failure(options, "Cannot read CSV input", line);
    if (state == detail::CsvState::Quoted) import_failure(options, "Unterminated quoted CSV field", line);
    if (record.empty()) return std::nullopt;
    return record;
}
} // namespace

std::vector<Track> FileIO::import_csv(const std::filesystem::path& path,
                                     const ImportOptions& options) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) import_failure(options, "Not a regular file: " + path.string());
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) import_failure(options, "Cannot open file: " + path.string());
    std::vector<Track> tracks;
    size_t line = 1, bytes = skip_utf8_bom(file), errors = 0, records = 0;
    if (bytes > options.max_input_bytes) import_failure(options, "CSV input byte limit exceeded", line);
    bool first = true;
    while (true) {
        const auto start_line = line;
        const auto record = csv_record(file, options, line, bytes);
        if (!record) break;
        if (first) {
            first = false;
            if (csv_header(detail::csv_fields(*record))) continue;
        }
        if (++records > options.max_tracks) import_failure(options, "CSV record count limit exceeded", start_line);
        auto track = Track::from_csv_line(*record);
        if (track && (!options.validate_data || validate_track_data(*track))) {
            tracks.push_back(std::move(*track));
        } else {
            const auto message = "Invalid CSV track at line " + std::to_string(start_line);
            if (!options.skip_invalid_lines) import_failure(options, message, start_line);
            if (options.error_callback) options.error_callback(message, start_line);
            if (++errors >= options.max_errors) import_failure(options, "CSV error limit exceeded", start_line);
        }
        if (options.progress_callback && records % 100 == 0) options.progress_callback(records, 0);
    }
    return tracks;
}

bool FileIO::export_csv(const std::filesystem::path& path, 
                       const std::vector<Track>& tracks,
                       const ExportOptions& options) {
    std::ofstream file(path, std::ios::binary);
    
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
    
    file.close();
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
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) import_failure(options, "Not a regular file: " + path.string());
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) import_failure(options, "Cannot open file: " + path.string());
    const auto bom_bytes = skip_utf8_bom(file);
    if (bom_bytes > options.max_input_bytes) import_failure(options, "JSON input byte limit exceeded");
    std::string content;
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() != 0) {
        const auto count = static_cast<size_t>(file.gcount());
        if (count > options.max_input_bytes - bom_bytes - content.size())
            import_failure(options, "JSON input byte limit exceeded");
        content.append(buffer, count);
    }
    if (file.bad() || !file.eof()) import_failure(options, "Cannot read JSON input");
    std::vector<std::string_view> objects;
    try {
        detail::JsonReader reader(content);
        objects = reader.array_objects(options.max_tracks);
    } catch (const std::runtime_error& error) {
        import_failure(options, error.what());
    }
    std::vector<Track> tracks;
    size_t errors = 0, count = 0;
    for (const auto object : objects) {
        ++count;
        const auto track = Track::from_json(std::string(object));
        if (track && (!options.validate_data || validate_track_data(*track))) {
            tracks.push_back(*track);
        } else {
            if (!options.skip_invalid_lines) import_failure(options, "Invalid JSON track", count);
            if (options.error_callback) options.error_callback("Invalid JSON track", count);
            if (++errors >= options.max_errors) import_failure(options, "JSON error limit exceeded", count);
        }
        if (options.progress_callback && count % 100 == 0) options.progress_callback(count, objects.size());
    }
    return tracks;
}

bool FileIO::export_json(const std::filesystem::path& path,
                        const std::vector<Track>& tracks,
                        const ExportOptions& options) {
    // Serialize first: to_json() throws on invalid UTF-8, and a throw must not
    // leave a truncated file behind or escape a bool-returning export.
    std::ostringstream out;
    try {
        out << "[";
        if (options.pretty_print) out << "\n";

        size_t count = 0;
        for (size_t i = 0; i < tracks.size(); ++i) {
            if (options.pretty_print) out << "  ";

            out << tracks[i].to_json();

            if (i < tracks.size() - 1) {
                out << ",";
            }

            if (options.pretty_print) out << "\n";

            ++count;
            if (options.progress_callback && count % 100 == 0) {
                options.progress_callback(count, tracks.size());
            }
        }
    } catch (const std::exception&) {
        return false;
    }

    std::ofstream file(path);

    if (!file.is_open()) {
        return false;
    }

    file << out.str();
    file << "]";
    if (options.pretty_print) file << "\n";
    
    file.close();
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
    
    file.close();
    return file.good();
}

bool FileIO::validate_csv_format(const std::filesystem::path& path) {
    std::ifstream file(path);
    
    if (!file.is_open()) {
        return false;
    }
    skip_utf8_bom(file);
    
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
    skip_utf8_bom(file);
    
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
    return detail::csv_fields(line);
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
