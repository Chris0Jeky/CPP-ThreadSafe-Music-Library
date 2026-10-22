#pragma once

#include "track.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <optional>
#include <filesystem>

namespace audio_library {

class FileIO {
public:
    using TrackPtr = std::shared_ptr<Track>;
    using ProgressCallback = std::function<void(size_t current, size_t total)>;
    using ErrorCallback = std::function<void(const std::string& error, size_t line_number)>;
    
    struct ImportOptions {
        bool skip_invalid_lines = true;
        bool validate_data = true;
        size_t max_errors = 100;
        ProgressCallback progress_callback = nullptr;
        ErrorCallback error_callback = nullptr;
        size_t max_record_bytes = 1024 * 1024;
        size_t max_record_lines = 10000;
        size_t max_input_bytes = 16 * 1024 * 1024;
        size_t max_tracks = 100000;
    };
    
    struct ExportOptions {
        bool pretty_print = true;
        bool include_metadata = true;
        ProgressCallback progress_callback = nullptr;
    };
    
    // Import failures throw std::runtime_error (after error_callback, if set).
    // CSV may skip invalid records; JSON syntax and resource errors always fail.
    // CSV operations
    static std::vector<Track> import_csv(const std::filesystem::path& path, 
                                        const ImportOptions& options);
    static std::vector<Track> import_csv(const std::filesystem::path& path) {
        return import_csv(path, ImportOptions{});
    }
    
    static bool export_csv(const std::filesystem::path& path, 
                          const std::vector<Track>& tracks,
                          const ExportOptions& options);
    static bool export_csv(const std::filesystem::path& path, 
                          const std::vector<Track>& tracks) {
        return export_csv(path, tracks, ExportOptions{});
    }
    
    static bool export_csv(const std::filesystem::path& path, 
                          const std::vector<TrackPtr>& tracks,
                          const ExportOptions& options);
    static bool export_csv(const std::filesystem::path& path, 
                          const std::vector<TrackPtr>& tracks) {
        return export_csv(path, tracks, ExportOptions{});
    }
    
    // JSON operations
    static std::vector<Track> import_json(const std::filesystem::path& path,
                                         const ImportOptions& options);
    static std::vector<Track> import_json(const std::filesystem::path& path) {
        return import_json(path, ImportOptions{});
    }
    
    static bool export_json(const std::filesystem::path& path,
                           const std::vector<Track>& tracks,
                           const ExportOptions& options);
    static bool export_json(const std::filesystem::path& path,
                           const std::vector<Track>& tracks) {
        return export_json(path, tracks, ExportOptions{});
    }
    
    static bool export_json(const std::filesystem::path& path,
                           const std::vector<TrackPtr>& tracks,
                           const ExportOptions& options);
    static bool export_json(const std::filesystem::path& path,
                           const std::vector<TrackPtr>& tracks) {
        return export_json(path, tracks, ExportOptions{});
    }
    
    // Playlist operations (M3U format)
    static std::vector<std::string> import_playlist(const std::filesystem::path& path);
    static bool export_playlist(const std::filesystem::path& path,
                               const std::vector<Track>& tracks,
                               const std::string& playlist_name = "");
    
    // Utility methods
    static bool validate_csv_format(const std::filesystem::path& path);
    static bool validate_json_format(const std::filesystem::path& path);
    static std::optional<std::string> detect_file_format(const std::filesystem::path& path);
    
private:
    // Helper methods
    static std::vector<std::string> split_csv_line(const std::string& line);
    static std::string escape_csv_field(const std::string& field);
    static bool validate_track_data(const Track& track);
};

} // namespace audio_library
