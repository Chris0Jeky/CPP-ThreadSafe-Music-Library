#include "../include/catch.hpp"
#include "audio_library/file_io.h"
#include "audio_library/music_library.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace audio_library;

class TempFile {
public:
    explicit TempFile(const std::string& filename) 
        : path_(fs::temp_directory_path() / filename) {}
    
    ~TempFile() {
        if (fs::exists(path_)) {
            fs::remove(path_);
        }
    }
    
    const fs::path& path() const { return path_; }
    
    void write(const std::string& content) {
        std::ofstream file(path_);
        file << content;
    }
    
private:
    fs::path path_;
};

TEST_CASE("FileIO CSV operations", "[file_io]") {
    SECTION("Export and import CSV") {
        TempFile temp_file("test_tracks.csv");
        
        // Create test tracks
        std::vector<Track> tracks;
        tracks.emplace_back("Song 1", "Artist 1", 180);
        tracks.emplace_back("Song 2", "Artist 2", 240);
        
        tracks[0].set_album("Album 1");
        tracks[0].set_genre("Rock");
        tracks[0].set_year(2020);
        tracks[0].set_rating(4.5);
        
        // Export
        REQUIRE(FileIO::export_csv(temp_file.path(), tracks));
        
        // Import
        auto imported = FileIO::import_csv(temp_file.path());
        
        REQUIRE(imported.size() == 2);
        REQUIRE(imported[0].title() == "Song 1");
        REQUIRE(imported[0].artist() == "Artist 1");
        REQUIRE(imported[0].duration_seconds() == 180);
        REQUIRE(imported[0].album() == "Album 1");
        REQUIRE(imported[0].genre() == "Rock");
        REQUIRE(imported[0].year() == 2020);
        REQUIRE(imported[0].rating() == Approx(4.5));
    }
    
    SECTION("Handle CSV with special characters") {
        TempFile temp_file("test_special.csv");
        
        std::vector<Track> tracks;
        tracks.emplace_back("Title, with comma", "Artist \"quoted\"", 120);
        tracks.emplace_back("Title\nwith\nnewline", "Artist\twith\ttab", 150);
        
        REQUIRE(FileIO::export_csv(temp_file.path(), tracks));
        
        auto imported = FileIO::import_csv(temp_file.path());
        
        REQUIRE(imported.size() == 2);
        REQUIRE(imported[0].title() == "Title, with comma");
        REQUIRE(imported[0].artist() == "Artist \"quoted\"");
    }
    
    SECTION("Import invalid CSV") {
        TempFile temp_file("test_invalid.csv");
        temp_file.write("Title,Artist,Duration\nInvalid Line\nSong,Artist,not_a_number\n");
        
        FileIO::ImportOptions options;
        options.skip_invalid_lines = true;
        
        auto imported = FileIO::import_csv(temp_file.path(), options);
        
        REQUIRE(imported.empty()); // All lines were invalid
    }
}

TEST_CASE("FileIO JSON operations", "[file_io]") {
    SECTION("Export and import JSON") {
        TempFile temp_file("test_tracks.json");
        
        std::vector<Track> tracks;
        tracks.emplace_back("JSON Song", "JSON Artist", 200);
        tracks[0].set_album("JSON Album");
        tracks[0].set_genre("Electronic");
        tracks[0].set_year(2023);
        tracks[0].set_rating(5.0);
        
        // Export
        FileIO::ExportOptions export_options;
        export_options.pretty_print = true;
        
        REQUIRE(FileIO::export_json(temp_file.path(), tracks, export_options));
        
        // Import
        auto imported = FileIO::import_json(temp_file.path());
        
        REQUIRE(imported.size() == 1);
        REQUIRE(imported[0].title() == "JSON Song");
        REQUIRE(imported[0].artist() == "JSON Artist");
        REQUIRE(imported[0].album() == "JSON Album");
        REQUIRE(imported[0].genre() == "Electronic");
        REQUIRE(imported[0].year() == 2023);
        REQUIRE(imported[0].rating() == 5.0);
    }
    
    SECTION("Export multiple tracks to JSON") {
        TempFile temp_file("test_multiple.json");
        
        std::vector<Track> tracks;
        for (int i = 0; i < 5; ++i) {
            tracks.emplace_back(
                "Song " + std::to_string(i),
                "Artist " + std::to_string(i),
                100 + i * 10
            );
        }
        
        REQUIRE(FileIO::export_json(temp_file.path(), tracks));
        
        auto imported = FileIO::import_json(temp_file.path());
        REQUIRE(imported.size() == 5);
        
        for (int i = 0; i < 5; ++i) {
            REQUIRE(imported[i].title() == "Song " + std::to_string(i));
            REQUIRE(imported[i].duration_seconds() == 100 + i * 10);
        }
    }
}

TEST_CASE("FileIO format detection", "[file_io]") {
    SECTION("Detect CSV format") {
        TempFile csv_file("test.csv");
        csv_file.write("Title,Artist,Duration\nSong,Artist,180\n");
        
        auto format = FileIO::detect_file_format(csv_file.path());
        REQUIRE(format.has_value());
        REQUIRE(*format == "csv");
    }
    
    SECTION("Detect JSON format") {
        TempFile json_file("test.json");
        json_file.write("[{\"title\":\"Song\",\"artist\":\"Artist\",\"duration\":180}]");
        
        auto format = FileIO::detect_file_format(json_file.path());
        REQUIRE(format.has_value());
        REQUIRE(*format == "json");
    }
    
    SECTION("Detect by content") {
        TempFile txt_file("test.txt");
        txt_file.write("[{\"title\":\"Song\",\"artist\":\"Artist\",\"duration\":180}]");
        
        auto format = FileIO::detect_file_format(txt_file.path());
        REQUIRE(format.has_value());
        REQUIRE(*format == "json");
    }
}

TEST_CASE("FileIO validation", "[file_io]") {
    SECTION("Validate CSV format") {
        TempFile valid_csv("valid.csv");
        valid_csv.write("Title,Artist,Duration\nSong,Artist,180\n");
        
        REQUIRE(FileIO::validate_csv_format(valid_csv.path()));
        
        TempFile invalid_csv("invalid.csv");
        invalid_csv.write("Only,Two\nFields,Here\n");
        
        REQUIRE_FALSE(FileIO::validate_csv_format(invalid_csv.path()));
    }
    
    SECTION("Validate JSON format") {
        TempFile valid_json("valid.json");
        valid_json.write("[{\"title\":\"Song\"}]");
        
        REQUIRE(FileIO::validate_json_format(valid_json.path()));
        
        TempFile invalid_json("invalid.json");
        invalid_json.write("{not an array}");
        
        REQUIRE_FALSE(FileIO::validate_json_format(invalid_json.path()));
    }
}

TEST_CASE("FileIO error handling", "[file_io]") {
    SECTION("Import from non-existent file") {
        fs::path non_existent = fs::temp_directory_path() / "does_not_exist.csv";
        
        REQUIRE_THROWS_AS(FileIO::import_csv(non_existent), std::runtime_error);
    }
    
    SECTION("Export to invalid path") {
        std::vector<Track> tracks;
        tracks.emplace_back("Song", "Artist", 180);
        
        fs::path invalid_path = "/invalid/path/that/does/not/exist/file.csv";
        
        REQUIRE_FALSE(FileIO::export_csv(invalid_path, tracks));
    }
    
    SECTION("Import with error callback") {
        TempFile temp_file("test_errors.csv");
        temp_file.write("Title,Artist,Duration\nGood,Artist,180\nBad,Artist,not_number\n");
        
        int error_count = 0;
        FileIO::ImportOptions options;
        options.error_callback = [&error_count](const std::string&, size_t) {
            ++error_count;
        };
        
        auto tracks = FileIO::import_csv(temp_file.path(), options);
        
        REQUIRE(tracks.size() == 1); // Only the good track
        REQUIRE(error_count == 1);    // One error for bad line
    }
}

TEST_CASE("FileIO progress callback", "[file_io]") {
    SECTION("Export progress") {
        TempFile temp_file("test_progress.csv");
        
        std::vector<Track> tracks;
        for (int i = 0; i < 500; ++i) {
            tracks.emplace_back("Song " + std::to_string(i), "Artist", 180);
        }
        
        int progress_calls = 0;
        FileIO::ExportOptions options;
        options.progress_callback = [&progress_calls](size_t current, size_t total) {
            ++progress_calls;
            REQUIRE(current <= total);
        };
        
        REQUIRE(FileIO::export_csv(temp_file.path(), tracks, options));
        REQUIRE(progress_calls > 0); // Should have received progress updates
    }
}

#if defined(__linux__)
TEST_CASE("Exports report buffered write failures", "[file_io][export][regression]") {
    // /dev/full accepts opens but rejects writes, including the final buffer flush.
    const fs::path full_path = "/dev/full";
    std::ofstream full(full_path, std::ios::binary);
    REQUIRE(full.is_open());
    full << "probe";
    REQUIRE(full.good());
    full.flush();
    REQUIRE_FALSE(full.good());

    const std::vector<Track> tracks{Track("Song", "Artist", 180)};
    const std::vector<FileIO::TrackPtr> pointers{std::make_shared<Track>(tracks[0])};
    MusicLibrary library;
    REQUIRE(library.add_track(tracks[0]));

    SECTION("CSV values") {
        REQUIRE_FALSE(FileIO::export_csv(full_path, tracks));
    }
    SECTION("JSON values") {
        REQUIRE_FALSE(FileIO::export_json(full_path, tracks));
    }
    SECTION("M3U values") {
        REQUIRE_FALSE(FileIO::export_playlist(full_path, tracks, "Favorites"));

        TempFile destination("test_export_playlist.m3u");
        REQUIRE(FileIO::export_playlist(destination.path(), tracks, "Favorites"));
        const auto imported = FileIO::import_playlist(destination.path());
        REQUIRE(imported == std::vector<std::string>{"Artist - Song.mp3"});
    }
    SECTION("CSV pointers") {
        REQUIRE_FALSE(FileIO::export_csv(full_path, pointers));
    }
    SECTION("JSON pointers") {
        REQUIRE_FALSE(FileIO::export_json(full_path, pointers));
    }
    SECTION("CSV library snapshot") {
        REQUIRE_FALSE(library.export_to_csv(full_path.string()));
    }
    SECTION("JSON library snapshot") {
        REQUIRE_FALSE(library.export_to_json(full_path.string()));
    }
}
#endif
