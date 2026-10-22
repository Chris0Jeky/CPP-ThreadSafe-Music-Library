#include "audio_library/music_library.h"
#include <iostream>

using namespace audio_library;

int main() {
    std::cout << "Audio Library - Basic Usage Example\n";
    std::cout << "===================================\n\n";
    
    // Create a music library
    MusicLibrary library;
    
    // Add some tracks
    std::cout << "Adding tracks to the library...\n";
    
    Track track1("Bohemian Rhapsody", "Queen", 354);
    track1.set_album("A Night at the Opera");
    track1.set_genre("Rock");
    track1.set_year(1975);
    track1.set_rating(5.0);
    
    Track track2("Imagine", "John Lennon", 183);
    track2.set_album("Imagine");
    track2.set_genre("Rock");
    track2.set_year(1971);
    track2.set_rating(4.8);
    
    Track track3("Hotel California", "Eagles", 391);
    track3.set_album("Hotel California");
    track3.set_genre("Rock");
    track3.set_year(1976);
    track3.set_rating(4.7);
    
    library.add_track(track1);
    library.add_track(track2);
    library.add_track(track3);
    
    std::cout << "Added " << library.size() << " tracks.\n\n";
    
    // Search for tracks
    std::cout << "Searching for 'Queen'...\n";
    auto results = library.search("Queen");
    
    for (const auto& track : results) {
        std::cout << "  Found: " << track->to_string() << "\n";
    }
    
    // Find by artist
    std::cout << "\nFinding all tracks by Eagles...\n";
    results = library.find_by_artist("Eagles");
    
    for (const auto& track : results) {
        std::cout << "  " << track->title() << " (" << track->format_duration() << ")\n";
    }

    // Query handles are immutable. Edit a copy and replace the expected version.
    if (!results.empty()) {
        Track edited = *results.front();
        edited.increment_play_count();
        if (!library.update_track(results.front(), std::move(edited))) {
            std::cerr << "Track changed or the replacement duplicates another track.\n";
        }
    }
    
    // Get statistics
    std::cout << "\nLibrary Statistics:\n";
    auto stats = library.get_statistics();
    std::cout << "  Total tracks: " << stats.total_tracks << "\n";
    std::cout << "  Total artists: " << stats.total_artists << "\n";
    std::cout << "  Average rating: " << stats.average_rating << "\n";
    
    // Sort tracks
    std::cout << "\nAll tracks sorted by year:\n";
    auto sorted = library.get_tracks_sorted([](const Track& a, const Track& b) {
        return a.year() < b.year();
    });
    
    for (const auto& track : sorted) {
        std::cout << "  " << track->year() << " - " << track->title() 
                  << " by " << track->artist() << "\n";
    }
    
    return 0;
}
