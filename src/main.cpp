#include "audio_library/music_library.h"
#include "audio_library/file_io.h"
#include "audio_library/search_engine.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <limits>
#include <cstdlib>

namespace fs = std::filesystem;
using namespace audio_library;

class MusicLibraryApp {
public:
    MusicLibraryApp() = default;
    
    void run(int argc, char* argv[]) {
        print_welcome();
        
        // Load initial file if provided
        if (argc > 1) {
            std::string filename = argv[1];
            if (argc > 2) {
                // Handle path and filename separately
                fs::path dir(argv[1]);
                fs::path file(argv[2]);
                filename = (dir / file).string();
            }
            
            std::cout << "Loading tracks from: " << filename << "\n";
            if (library_.import_from_csv(filename)) {
                std::cout << "Successfully loaded " << library_.size() << " tracks.\n\n";
            } else {
                std::cerr << "Failed to load tracks from file.\n\n";
            }
        }
        
        // Main menu loop
        while (running_) {
            display_menu();
            if (!process_command()) {
                break; // Exit on EOF or input error
            }
        }
        
        print_goodbye();
    }
    
private:
    MusicLibrary library_;
    bool running_ = true;
    
    void print_welcome() {
        clear_screen();
        std::cout << R"(
╔════════════════════════════════════════════════════╗
║           Audio Library Management System          ║
║                    Version 2.0                     ║
╚════════════════════════════════════════════════════╝
)" << "\n";
    }
    
    void print_goodbye() {
        std::cout << "\nThank you for using Audio Library. Goodbye!\n";
    }
    
    void display_menu() {
        auto stats = library_.get_statistics();
        
        std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "Library Statistics: " 
                  << stats.total_tracks << " tracks, "
                  << stats.total_artists << " artists, "
                  << format_duration(stats.total_duration) << " total\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
        
        std::cout << "MAIN MENU:\n";
        std::cout << "  1. Search tracks\n";
        std::cout << "  2. List all tracks\n";
        std::cout << "  3. Add track manually\n";
        std::cout << "  4. Remove track\n";
        std::cout << "  5. Import tracks from file\n";
        std::cout << "  6. Export tracks to file\n";
        std::cout << "  7. Browse by category\n";
        std::cout << "  8. Top tracks\n";
        std::cout << "  9. Advanced search\n";
        std::cout << "  0. Exit\n\n";
        std::cout << "Enter your choice: ";
        std::cout.flush(); // Ensure the prompt is displayed
    }
    
    bool process_command() {
        std::string input;
        
        // Check if input is available
        if (!std::getline(std::cin, input)) {
            // EOF or error occurred
            if (std::cin.eof()) {
                std::cout << "\nEOF detected. Exiting...\n";
            } else {
                std::cout << "\nInput error. Exiting...\n";
            }
            running_ = false;
            return false;
        }
        
        // Trim whitespace
        input.erase(0, input.find_first_not_of(" \t\r\n"));
        input.erase(input.find_last_not_of(" \t\r\n") + 1);
        
        if (input.empty()) {
            return true; // Empty input, just redisplay menu
        }
        
        try {
            int choice = std::stoi(input);
            
            switch (choice) {
                case 1: search_tracks(); break;
                case 2: list_all_tracks(); break;
                case 3: add_track_manually(); break;
                case 4: remove_track(); break;
                case 5: import_tracks(); break;
                case 6: export_tracks(); break;
                case 7: browse_by_category(); break;
                case 8: show_top_tracks(); break;
                case 9: advanced_search(); break;
                case 0: 
                    running_ = false; 
                    break;
                default:
                    std::cout << "\n❌ Invalid choice. Please enter a number between 0-9.\n";
                    press_enter_to_continue();
            }
        } catch (const std::exception&) {
            std::cout << "\n❌ Invalid input. Please enter a number.\n";
            press_enter_to_continue();
        }
        
        return true;
    }
    
    void search_tracks() {
        clear_screen();
        std::cout << "SEARCH TRACKS\n";
        std::cout << "─────────────\n\n";
        std::cout << "Enter search query (or 'back' to return): ";
        
        std::string query;
        if (!safe_getline(query) || query == "back") return;
        
        auto results = library_.search(query);
        display_tracks(results, "Search Results for \"" + query + "\"");
        
        if (!results.empty()) {
            export_search_results_prompt(results);
        }
        
        press_enter_to_continue();
    }
    
    void list_all_tracks() {
        clear_screen();
        std::cout << "LIST ALL TRACKS\n";
        std::cout << "───────────────\n\n";
        
        std::cout << "Sort by:\n";
        std::cout << "  1. Title (A-Z)\n";
        std::cout << "  2. Artist (A-Z)\n";
        std::cout << "  3. Duration (shortest first)\n";
        std::cout << "  4. Year (oldest first)\n";
        std::cout << "  5. Rating (highest first)\n";
        std::cout << "  6. Play count (most played first)\n";
        std::cout << "  0. No sorting\n\n";
        std::cout << "Choice: ";
        
        std::string input;
        if (!safe_getline(input)) return;
        
        std::vector<MusicLibrary::TrackPtr> tracks;
        
        try {
            int choice = input.empty() ? 0 : std::stoi(input);
            
            switch (choice) {
                case 1:
                    tracks = library_.get_tracks_sorted(
                        [](const Track& a, const Track& b) { return a.title() < b.title(); });
                    break;
                case 2:
                    tracks = library_.get_tracks_sorted(
                        [](const Track& a, const Track& b) { return a.artist() < b.artist(); });
                    break;
                case 3:
                    tracks = library_.get_tracks_sorted(
                        [](const Track& a, const Track& b) { return a.duration() < b.duration(); });
                    break;
                case 4:
                    tracks = library_.get_tracks_sorted(
                        [](const Track& a, const Track& b) { return a.year() < b.year(); });
                    break;
                case 5:
                    tracks = library_.get_tracks_sorted(
                        [](const Track& a, const Track& b) { return a.rating() > b.rating(); });
                    break;
                case 6:
                    tracks = library_.get_tracks_sorted(
                        [](const Track& a, const Track& b) { return a.play_count() > b.play_count(); });
                    break;
                default:
                    tracks = library_.get_all_tracks();
            }
        } catch (...) {
            tracks = library_.get_all_tracks();
        }
        
        display_tracks(tracks, "All Tracks");
        
        if (!tracks.empty()) {
            export_search_results_prompt(tracks);
        }
        
        press_enter_to_continue();
    }
    
    void add_track_manually() {
        clear_screen();
        std::cout << "ADD NEW TRACK\n";
        std::cout << "─────────────\n\n";
        
        std::string title, artist, duration_str;
        
        std::cout << "Title (required): ";
        if (!safe_getline(title) || title.empty()) {
            std::cout << "\n❌ Title is required.\n";
            press_enter_to_continue();
            return;
        }
        
        std::cout << "Artist (required): ";
        if (!safe_getline(artist) || artist.empty()) {
            std::cout << "\n❌ Artist is required.\n";
            press_enter_to_continue();
            return;
        }
        
        std::cout << "Duration in seconds (required): ";
        if (!safe_getline(duration_str)) return;
        
        try {
            int duration = std::stoi(duration_str);
            if (duration <= 0 || duration > 36000) { // Max 10 hours
                std::cout << "\n❌ Invalid duration. Must be between 1 and 36000 seconds.\n";
                press_enter_to_continue();
                return;
            }
            
            Track track(title, artist, duration);
            
            // Optional fields
            std::cout << "\nOptional fields (press Enter to skip):\n";
            
            std::cout << "Album: ";
            std::string album;
            if (safe_getline(album) && !album.empty()) {
                track.set_album(album);
            }
            
            std::cout << "Genre: ";
            std::string genre;
            if (safe_getline(genre) && !genre.empty()) {
                track.set_genre(genre);
            }
            
            std::cout << "Year: ";
            std::string year_str;
            if (safe_getline(year_str) && !year_str.empty()) {
                try {
                    int year = std::stoi(year_str);
                    if (year >= 1900 && year <= 2100) {
                        track.set_year(year);
                    }
                } catch (...) {}
            }
            
            std::cout << "Rating (0-5): ";
            std::string rating_str;
            if (safe_getline(rating_str) && !rating_str.empty()) {
                try {
                    double rating = std::stod(rating_str);
                    track.set_rating(rating);
                } catch (...) {}
            }
            
            if (library_.add_track(std::move(track))) {
                std::cout << "\n✅ Track added successfully!\n";
            } else {
                std::cout << "\n❌ Failed to add track (duplicate?).\n";
            }
        } catch (const std::exception& e) {
            std::cout << "\n❌ Invalid input: " << e.what() << "\n";
        }
        
        press_enter_to_continue();
    }
    
    void remove_track() {
        clear_screen();
        std::cout << "REMOVE TRACK\n";
        std::cout << "────────────\n\n";
        std::cout << "Enter exact title: ";
        
        std::string title;
        if (!safe_getline(title) || title.empty()) return;
        
        std::cout << "Enter exact artist: ";
        std::string artist;
        if (!safe_getline(artist) || artist.empty()) return;
        
        if (library_.remove_track(title, artist)) {
            std::cout << "\n✅ Track removed successfully!\n";
        } else {
            std::cout << "\n❌ Track not found.\n";
            
            // Offer to search for similar tracks
            auto results = library_.search(title + " " + artist);
            if (!results.empty()) {
                std::cout << "\nDid you mean one of these?\n";
                display_tracks(results, "Similar Tracks", 5);
            }
        }
        
        press_enter_to_continue();
    }
    
    void import_tracks() {
        clear_screen();
        std::cout << "IMPORT TRACKS\n";
        std::cout << "─────────────\n\n";
        std::cout << "Enter filename (or 'back' to return): ";
        
        std::string filename;
        if (!safe_getline(filename) || filename == "back") return;
        
        // Check if file exists
        if (!fs::exists(filename)) {
            std::cout << "\n❌ File not found: " << filename << "\n";
            press_enter_to_continue();
            return;
        }
        
        // Auto-detect format
        auto format = FileIO::detect_file_format(filename);
        
        bool success = false;
        size_t prev_size = library_.size();
        
        std::cout << "\nImporting from " << (format.value_or("unknown")) << " format...\n";
        
        if (format == "json") {
            success = library_.import_from_json(filename);
        } else {
            success = library_.import_from_csv(filename);
        }
        
        if (success) {
            size_t added = library_.size() - prev_size;
            std::cout << "\n✅ Successfully imported " << added << " tracks.\n";
        } else {
            std::cout << "\n❌ Failed to import tracks. Check file format.\n";
        }
        
        press_enter_to_continue();
    }
    
    void export_tracks() {
        clear_screen();
        std::cout << "EXPORT TRACKS\n";
        std::cout << "─────────────\n\n";
        
        if (library_.empty()) {
            std::cout << "❌ Library is empty. Nothing to export.\n";
            press_enter_to_continue();
            return;
        }
        
        std::cout << "Export format:\n";
        std::cout << "  1. CSV\n";
        std::cout << "  2. JSON\n";
        std::cout << "Choice: ";
        
        std::string format_choice;
        if (!safe_getline(format_choice)) return;
        
        std::cout << "Enter filename: ";
        std::string filename;
        if (!safe_getline(filename) || filename.empty()) return;
        
        bool success = false;
        
        try {
            int choice = std::stoi(format_choice);
            
            // Add appropriate extension if missing
            if (choice == 2 && filename.find(".json") == std::string::npos) {
                filename += ".json";
            } else if (choice == 1 && filename.find(".csv") == std::string::npos) {
                filename += ".csv";
            }
            
            std::cout << "\nExporting to " << filename << "...\n";
            
            if (choice == 2) {
                success = library_.export_to_json(filename);
            } else {
                success = library_.export_to_csv(filename);
            }
        } catch (...) {
            success = library_.export_to_csv(filename);
        }
        
        if (success) {
            std::cout << "\n✅ Successfully exported " << library_.size() << " tracks to " << filename << "\n";
        } else {
            std::cout << "\n❌ Failed to export tracks.\n";
        }
        
        press_enter_to_continue();
    }
    
    void browse_by_category() {
        clear_screen();
        std::cout << "BROWSE BY CATEGORY\n";
        std::cout << "──────────────────\n\n";
        
        std::cout << "Browse by:\n";
        std::cout << "  1. Artist\n";
        std::cout << "  2. Album\n";
        std::cout << "  3. Genre\n";
        std::cout << "  4. Year\n";
        std::cout << "  0. Back\n";
        std::cout << "Choice: ";
        
        std::string choice_str;
        if (!safe_getline(choice_str)) return;
        
        try {
            int choice = std::stoi(choice_str);
            
            switch (choice) {
                case 1: browse_artists(); break;
                case 2: browse_albums(); break;
                case 3: browse_genres(); break;
                case 4: browse_years(); break;
                case 0: return;
                default:
                    std::cout << "\n❌ Invalid choice.\n";
                    press_enter_to_continue();
            }
        } catch (...) {
            std::cout << "\n❌ Invalid input.\n";
            press_enter_to_continue();
        }
    }
    
    void browse_artists() {
        auto artists = library_.get_all_artists();
        if (artists.empty()) {
            std::cout << "\n❌ No artists in library.\n";
            press_enter_to_continue();
            return;
        }
        
        std::sort(artists.begin(), artists.end());
        
        clear_screen();
        std::cout << "BROWSE BY ARTIST\n";
        std::cout << "────────────────\n\n";
        std::cout << "Found " << artists.size() << " artists:\n\n";
        
        // Paginate if too many
        const size_t page_size = 20;
        size_t current_page = 0;
        
        while (true) {
            size_t start = current_page * page_size;
            size_t end = std::min(start + page_size, artists.size());
            
            for (size_t i = start; i < end; ++i) {
                std::cout << "  " << std::setw(3) << (i + 1) << ". " << artists[i] << "\n";
            }
            
            std::cout << "\nEnter artist number (or 'n' for next page, 'p' for previous, '0' to go back): ";
            std::string input;
            if (!safe_getline(input)) return;
            
            if (input == "0") return;
            if (input == "n" && end < artists.size()) {
                current_page++;
                clear_screen();
                std::cout << "BROWSE BY ARTIST (Page " << (current_page + 1) << ")\n";
                std::cout << "────────────────\n\n";
                continue;
            }
            if (input == "p" && current_page > 0) {
                current_page--;
                clear_screen();
                std::cout << "BROWSE BY ARTIST (Page " << (current_page + 1) << ")\n";
                std::cout << "────────────────\n\n";
                continue;
            }
            
            try {
                size_t index = std::stoul(input);
                if (index > 0 && index <= artists.size()) {
                    auto tracks = library_.find_by_artist(artists[index - 1]);
                    display_tracks(tracks, "Tracks by " + artists[index - 1]);
                    press_enter_to_continue();
                    return;
                }
            } catch (...) {}
        }
    }
    
    void browse_albums() {
        auto albums = library_.get_all_albums();
        if (albums.empty()) {
            std::cout << "\n❌ No albums in library.\n";
            press_enter_to_continue();
            return;
        }
        
        std::sort(albums.begin(), albums.end());
        
        clear_screen();
        std::cout << "BROWSE BY ALBUM\n";
        std::cout << "───────────────\n\n";
        std::cout << "Found " << albums.size() << " albums:\n\n";
        
        for (size_t i = 0; i < albums.size(); ++i) {
            std::cout << "  " << std::setw(3) << (i + 1) << ". " << albums[i] << "\n";
        }
        
        std::cout << "\nEnter album number (or '0' to go back): ";
        std::string input;
        if (!safe_getline(input)) return;
        
        try {
            size_t index = std::stoul(input);
            if (index == 0) return;
            if (index > 0 && index <= albums.size()) {
                auto tracks = library_.find_by_album(albums[index - 1]);
                display_tracks(tracks, "Tracks from \"" + albums[index - 1] + "\"");
                press_enter_to_continue();
            }
        } catch (...) {}
    }
    
    void browse_genres() {
        auto genres = library_.get_all_genres();
        if (genres.empty()) {
            std::cout << "\n❌ No genres in library.\n";
            press_enter_to_continue();
            return;
        }
        
        std::sort(genres.begin(), genres.end());
        
        clear_screen();
        std::cout << "BROWSE BY GENRE\n";
        std::cout << "───────────────\n\n";
        std::cout << "Found " << genres.size() << " genres:\n\n";
        
        for (size_t i = 0; i < genres.size(); ++i) {
            std::cout << "  " << std::setw(3) << (i + 1) << ". " << genres[i] << "\n";
        }
        
        std::cout << "\nEnter genre number (or '0' to go back): ";
        std::string input;
        if (!safe_getline(input)) return;
        
        try {
            size_t index = std::stoul(input);
            if (index == 0) return;
            if (index > 0 && index <= genres.size()) {
                auto tracks = library_.find_by_genre(genres[index - 1]);
                display_tracks(tracks, genres[index - 1] + " Tracks");
                press_enter_to_continue();
            }
        } catch (...) {}
    }
    
    void browse_years() {
        clear_screen();
        std::cout << "BROWSE BY YEAR\n";
        std::cout << "──────────────\n\n";
        
        std::cout << "Enter year range (e.g., '2010 2020' or just '2020' for single year): ";
        std::string input;
        if (!safe_getline(input)) return;
        
        std::istringstream iss(input);
        int start_year, end_year;
        
        if (iss >> start_year) {
            if (!(iss >> end_year)) {
                end_year = start_year; // Single year
            }
            
            if (start_year > end_year) {
                std::swap(start_year, end_year);
            }
            
            auto tracks = library_.get_tracks_by_year_range(start_year, end_year);
            std::string title = (start_year == end_year) 
                ? "Tracks from " + std::to_string(start_year)
                : "Tracks from " + std::to_string(start_year) + " to " + std::to_string(end_year);
            
            display_tracks(tracks, title);
            press_enter_to_continue();
        } else {
            std::cout << "\n❌ Invalid input format.\n";
            press_enter_to_continue();
        }
    }
    
    void show_top_tracks() {
        clear_screen();
        std::cout << "TOP TRACKS\n";
        std::cout << "──────────\n\n";
        
        std::cout << "Show top tracks by:\n";
        std::cout << "  1. Play count\n";
        std::cout << "  2. Rating\n";
        std::cout << "  0. Back\n";
        std::cout << "Choice: ";
        
        std::string choice_str;
        if (!safe_getline(choice_str)) return;
        
        try {
            int choice = std::stoi(choice_str);
            
            if (choice == 1) {
                auto tracks = library_.get_most_played_tracks(20);
                display_tracks(tracks, "Most Played Tracks");
                press_enter_to_continue();
            } else if (choice == 2) {
                auto tracks = library_.get_top_rated_tracks(20);
                if (tracks.empty()) {
                    std::cout << "\n❌ No rated tracks in library.\n";
                } else {
                    display_tracks(tracks, "Top Rated Tracks");
                }
                press_enter_to_continue();
            }
        } catch (...) {
            std::cout << "\n❌ Invalid input.\n";
            press_enter_to_continue();
        }
    }
    
    void advanced_search() {
        clear_screen();
        std::cout << "ADVANCED SEARCH\n";
        std::cout << "───────────────\n\n";
        std::cout << "Search query: ";
        
        std::string query;
        if (!safe_getline(query) || query.empty()) return;
        
        SearchEngine::SearchOptions options;
        
        std::cout << "\nSearch mode:\n";
        std::cout << "  1. Substring (default)\n";
        std::cout << "  2. Exact match\n";
        std::cout << "  3. Prefix match\n";
        std::cout << "  4. Fuzzy match\n";
        std::cout << "  5. Regular expression\n";
        std::cout << "Choice [1]: ";
        
        std::string mode_str;
        if (!safe_getline(mode_str)) return;
        
        try {
            int mode = mode_str.empty() ? 1 : std::stoi(mode_str);
            switch (mode) {
                case 2: options.mode = SearchEngine::SearchMode::EXACT; break;
                case 3: options.mode = SearchEngine::SearchMode::PREFIX; break;
                case 4: options.mode = SearchEngine::SearchMode::FUZZY; break;
                case 5: options.mode = SearchEngine::SearchMode::REGEX; break;
                default: options.mode = SearchEngine::SearchMode::SUBSTRING;
            }
        } catch (...) {}
        
        std::cout << "Case sensitive? (y/N): ";
        std::string case_input;
        if (!safe_getline(case_input)) return;
        
        options.case_sensitive = !case_input.empty() && 
                                (case_input[0] == 'y' || case_input[0] == 'Y');
        
        std::cout << "\nSearching...\n";
        auto results = library_.search_advanced(query, nullptr);
        display_tracks(results, "Advanced Search Results");
        
        if (!results.empty()) {
            export_search_results_prompt(results);
        }
        
        press_enter_to_continue();
    }
    
    void display_tracks(const std::vector<MusicLibrary::TrackPtr>& tracks,
                       const std::string& title,
                       size_t limit = 0) {
        if (tracks.empty()) {
            std::cout << "\n❌ No tracks found.\n";
            return;
        }
        
        size_t count = limit > 0 ? std::min(tracks.size(), limit) : tracks.size();
        
        std::cout << "\n" << title << " (" << count;
        if (count < tracks.size()) {
            std::cout << " of " << tracks.size();
        }
        std::cout << "):\n";
        std::cout << "────────────────────────────────────────────────────────────────────────────────\n";
        
        // Table header
        std::cout << std::left
                  << std::setw(4) << "#"
                  << std::setw(30) << "Title"
                  << std::setw(25) << "Artist"
                  << std::setw(10) << "Duration"
                  << std::setw(8) << "Rating"
                  << "Year\n";
        std::cout << "────────────────────────────────────────────────────────────────────────────────\n";
        
        // Display tracks
        for (size_t i = 0; i < count; ++i) {
            if (!tracks[i]) continue;
            
            const auto& track = *tracks[i];
            
            std::cout << std::left
                      << std::setw(4) << (i + 1)
                      << std::setw(30) << truncate(track.title(), 29)
                      << std::setw(25) << truncate(track.artist(), 24)
                      << std::setw(10) << track.format_duration();
            
            if (track.rating() > 0) {
                std::cout << std::setw(8) << std::fixed << std::setprecision(1) 
                          << track.rating();
            } else {
                std::cout << std::setw(8) << "-";
            }
            
            if (track.year() > 0) {
                std::cout << track.year();
            } else {
                std::cout << "-";
            }
            
            std::cout << "\n";
        }
        
        if (count < tracks.size()) {
            std::cout << "... and " << (tracks.size() - count) << " more tracks.\n";
        }
    }
    
    void export_search_results_prompt(const std::vector<MusicLibrary::TrackPtr>& results) {
        std::cout << "\nExport these results? (y/N): ";
        std::string export_choice;
        if (!safe_getline(export_choice)) return;
        
        if (!export_choice.empty() && (export_choice[0] == 'y' || export_choice[0] == 'Y')) {
            std::cout << "Export filename: ";
            std::string filename;
            if (!safe_getline(filename) || filename.empty()) return;
            
            if (filename.find(".json") == std::string::npos && 
                filename.find(".csv") == std::string::npos) {
                filename += ".csv";
            }
            
            bool success = filename.find(".json") != std::string::npos
                ? FileIO::export_json(filename, results)
                : FileIO::export_csv(filename, results);
                
            if (success) {
                std::cout << "✅ Exported to " << filename << "\n";
            } else {
                std::cout << "❌ Export failed.\n";
            }
        }
    }
    
    static std::string format_duration(std::chrono::seconds duration) {
        auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
        duration -= hours;
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
        
        std::ostringstream oss;
        if (hours.count() > 0) {
            oss << hours.count() << "h " << minutes.count() << "m";
        } else {
            oss << minutes.count() << " minutes";
        }
        return oss.str();
    }
    
    static std::string truncate(const std::string& str, size_t max_length) {
        if (str.length() <= max_length) {
            return str;
        }
        return str.substr(0, max_length - 3) + "...";
    }
    
    void press_enter_to_continue() {
        std::cout << "\nPress Enter to continue...";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    
    bool safe_getline(std::string& str) {
        if (!std::getline(std::cin, str)) {
            if (std::cin.eof()) {
                std::cout << "\nEOF detected.\n";
            } else {
                std::cout << "\nInput error.\n";
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
            return false;
        }
        
        // Trim whitespace
        str.erase(0, str.find_first_not_of(" \t\r\n"));
        str.erase(str.find_last_not_of(" \t\r\n") + 1);
        
        return true;
    }
    
    void clear_screen() {
        #ifdef _WIN32
            const int clear_result = std::system("cls");
        #else
            const int clear_result = std::system("clear");
        #endif
        (void)clear_result;
    }
};

int main(int argc, char* argv[]) {
    try {
        MusicLibraryApp app;
        app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
