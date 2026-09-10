#include "../include/catch.hpp"
#include "audio_library/music_library.h"
#include <thread>
#include <atomic>

using namespace audio_library;

TEST_CASE("MusicLibrary basic operations", "[music_library]") {
    MusicLibrary library;
    
    SECTION("Empty library") {
        REQUIRE(library.empty());
        REQUIRE(library.size() == 0);
        
        auto stats = library.get_statistics();
        REQUIRE(stats.total_tracks == 0);
        REQUIRE(stats.total_artists == 0);
    }
    
    SECTION("Add tracks") {
        Track track1("Song 1", "Artist 1", 180);
        Track track2("Song 2", "Artist 2", 240);
        Track track3("Song 3", "Artist 1", 200);
        
        REQUIRE(library.add_track(track1));
        REQUIRE(library.add_track(track2));
        REQUIRE(library.add_track(track3));
        
        REQUIRE(library.size() == 3);
        REQUIRE_FALSE(library.empty());
    }
    
    SECTION("Prevent duplicates") {
        Track track("Song", "Artist", 180);
        
        REQUIRE(library.add_track(track));
        REQUIRE_FALSE(library.add_track(track)); // Duplicate
        
        REQUIRE(library.size() == 1);
    }
    
    SECTION("Remove tracks") {
        Track track1("Song 1", "Artist 1", 180);
        Track track2("Song 2", "Artist 2", 240);
        
        library.add_track(track1);
        library.add_track(track2);
        
        REQUIRE(library.remove_track("Song 1", "Artist 1"));
        REQUIRE(library.size() == 1);
        
        REQUIRE_FALSE(library.remove_track("Song 1", "Artist 1")); // Already removed
        REQUIRE(library.size() == 1);
    }
}

TEST_CASE("MusicLibrary search operations", "[music_library]") {
    MusicLibrary library;
    
    // Add test tracks
    library.add_track(Track("Bohemian Rhapsody", "Queen", 354));
    library.add_track(Track("Imagine", "John Lennon", 183));
    library.add_track(Track("Hotel California", "Eagles", 391));
    library.add_track(Track("Stairway to Heaven", "Led Zeppelin", 482));
    library.add_track(Track("Another One Bites the Dust", "Queen", 215));
    
    SECTION("Find by title") {
        auto results = library.find_by_title("Bohemian Rhapsody");
        REQUIRE(results.size() == 1);
        REQUIRE(results[0]->title() == "Bohemian Rhapsody");
        
        results = library.find_by_title("Nonexistent");
        REQUIRE(results.empty());
    }
    
    SECTION("Find by artist") {
        auto results = library.find_by_artist("Queen");
        REQUIRE(results.size() == 2);
        
        results = library.find_by_artist("John Lennon");
        REQUIRE(results.size() == 1);
        REQUIRE(results[0]->title() == "Imagine");
    }
    
    SECTION("General search") {
        auto results = library.search("Queen");
        REQUIRE(results.size() == 2);
        
        results = library.search("heaven");
        REQUIRE(results.size() >= 1);
        
        results = library.search("california");
        REQUIRE(results.size() >= 1);
    }
}

TEST_CASE("MusicLibrary advanced features", "[music_library]") {
    MusicLibrary library;
    
    // Add tracks with metadata
    Track track1("Song A", "Artist 1", 180);
    track1.set_album("Album 1");
    track1.set_genre("Rock");
    track1.set_year(2020);
    track1.set_rating(4.5);
    
    Track track2("Song B", "Artist 2", 240);
    track2.set_album("Album 2");
    track2.set_genre("Pop");
    track2.set_year(2021);
    track2.set_rating(3.5);
    
    Track track3("Song C", "Artist 1", 200);
    track3.set_album("Album 1");
    track3.set_genre("Rock");
    track3.set_year(2019);
    track3.set_rating(5.0);
    
    library.add_track(track1);
    library.add_track(track2);
    library.add_track(track3);
    
    SECTION("Find by album") {
        auto results = library.find_by_album("Album 1");
        REQUIRE(results.size() == 2);
    }
    
    SECTION("Find by genre") {
        auto results = library.find_by_genre("Rock");
        REQUIRE(results.size() == 2);
        
        results = library.find_by_genre("Pop");
        REQUIRE(results.size() == 1);
    }
    
    SECTION("Filter by year range") {
        auto results = library.get_tracks_by_year_range(2020, 2021);
        REQUIRE(results.size() == 2);
        
        results = library.get_tracks_by_year_range(2019, 2019);
        REQUIRE(results.size() == 1);
    }
    
    SECTION("Filter by rating") {
        auto results = library.get_tracks_by_rating_range(4.0, 5.0);
        REQUIRE(results.size() == 2);
        
        results = library.get_top_rated_tracks(1);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0]->rating() == 5.0);
    }
    
    SECTION("Get all categories") {
        auto artists = library.get_all_artists();
        REQUIRE(artists.size() == 2);
        
        auto albums = library.get_all_albums();
        REQUIRE(albums.size() == 2);
        
        auto genres = library.get_all_genres();
        REQUIRE(genres.size() == 2);
    }
}

TEST_CASE("MusicLibrary sorting", "[music_library]") {
    MusicLibrary library;
    
    library.add_track(Track("C Song", "B Artist", 300));
    library.add_track(Track("A Song", "C Artist", 100));
    library.add_track(Track("B Song", "A Artist", 200));
    
    SECTION("Sort by title") {
        auto tracks = library.get_tracks_sorted([](const Track& a, const Track& b) {
            return a.title() < b.title();
        });
        
        REQUIRE(tracks.size() == 3);
        REQUIRE(tracks[0]->title() == "A Song");
        REQUIRE(tracks[1]->title() == "B Song");
        REQUIRE(tracks[2]->title() == "C Song");
    }
    
    SECTION("Sort by artist") {
        auto tracks = library.get_tracks_sorted([](const Track& a, const Track& b) {
            return a.artist() < b.artist();
        });
        
        REQUIRE(tracks.size() == 3);
        REQUIRE(tracks[0]->artist() == "A Artist");
        REQUIRE(tracks[1]->artist() == "B Artist");
        REQUIRE(tracks[2]->artist() == "C Artist");
    }
    
    SECTION("Sort by duration") {
        auto tracks = library.get_tracks_sorted([](const Track& a, const Track& b) {
            return a.duration() < b.duration();
        });
        
        REQUIRE(tracks.size() == 3);
        REQUIRE(tracks[0]->duration_seconds() == 100);
        REQUIRE(tracks[1]->duration_seconds() == 200);
        REQUIRE(tracks[2]->duration_seconds() == 300);
    }
}

TEST_CASE("MusicLibrary statistics", "[music_library]") {
    MusicLibrary library;
    
    Track track1("Song 1", "Artist 1", 180);
    track1.set_rating(4.0);
    track1.increment_play_count();
    track1.increment_play_count();
    
    Track track2("Song 2", "Artist 2", 240);
    track2.set_rating(5.0);
    track2.increment_play_count();
    
    Track track3("Song 3", "Artist 1", 120);
    // No rating
    
    library.add_track(track1);
    library.add_track(track2);
    library.add_track(track3);
    
    auto stats = library.get_statistics();
    
    REQUIRE(stats.total_tracks == 3);
    REQUIRE(stats.total_artists == 2);
    REQUIRE(stats.total_duration == std::chrono::seconds(540));
    REQUIRE(stats.average_rating == Approx(4.5)); // (4.0 + 5.0) / 2
    REQUIRE(stats.total_play_count == 3);
}

TEST_CASE("MusicLibrary thread safety", "[music_library][thread]") {
    MusicLibrary library;
    const int num_threads = 4;
    const int tracks_per_thread = 25;
    
    SECTION("Concurrent additions") {
        std::vector<std::thread> threads;
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&library, t, tracks_per_thread]() {
                for (int i = 0; i < tracks_per_thread; ++i) {
                    Track track(
                        "Song " + std::to_string(t * tracks_per_thread + i),
                        "Artist " + std::to_string(t),
                        100 + i
                    );
                    library.add_track(track);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        REQUIRE(library.size() == num_threads * tracks_per_thread);
    }
    
    SECTION("Concurrent reads") {
        // Add some tracks first
        for (int i = 0; i < 50; ++i) {
            library.add_track(Track("Song " + std::to_string(i), "Artist", 100));
        }
        
        std::vector<std::thread> threads;
        std::atomic<std::size_t> total_found(0);
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&library, &total_found]() {
                for (int i = 0; i < 10; ++i) {
                    auto results = library.find_by_artist("Artist");
                    total_found += results.size();
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        REQUIRE(total_found == num_threads * 10 * 50);
    }
}

TEST_CASE("MusicLibrary edge cases", "[music_library]") {
    MusicLibrary library;
    
    SECTION("Remove from empty library") {
        REQUIRE_FALSE(library.remove_track("Title", "Artist"));
        REQUIRE_FALSE(library.remove_tracks_by_title("Title"));
        REQUIRE_FALSE(library.remove_tracks_by_artist("Artist"));
    }
    
    SECTION("Clear library") {
        library.add_track(Track("Song 1", "Artist 1", 100));
        library.add_track(Track("Song 2", "Artist 2", 200));
        
        REQUIRE(library.size() == 2);
        
        library.clear();
        
        REQUIRE(library.empty());
        REQUIRE(library.size() == 0);
    }
    
    SECTION("Multiple tracks same title different artists") {
        library.add_track(Track("Same Song", "Artist 1", 180));
        library.add_track(Track("Same Song", "Artist 2", 180));
        library.add_track(Track("Same Song", "Artist 3", 180));
        
        auto results = library.find_by_title("Same Song");
        REQUIRE(results.size() == 3);
        
        // Remove by specific artist
        REQUIRE(library.remove_track("Same Song", "Artist 2"));
        
        results = library.find_by_title("Same Song");
        REQUIRE(results.size() == 2);
        
        // Remove all with same title
        REQUIRE(library.remove_tracks_by_title("Same Song"));
        
        results = library.find_by_title("Same Song");
        REQUIRE(results.empty());
    }
}
