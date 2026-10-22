#include "../include/catch.hpp"
#include "audio_library/search_engine.h"
#include "audio_library/track.h"

using namespace audio_library;

TEST_CASE("SearchEngine basic operations", "[search_engine]") {
    SearchEngine engine;
    std::vector<std::shared_ptr<Track>> tracks;
    
    // Create test tracks
    auto track1 = std::make_shared<Track>("Bohemian Rhapsody", "Queen", 354);
    track1->set_album("A Night at the Opera");
    track1->set_genre("Rock");
    tracks.push_back(track1);
    
    auto track2 = std::make_shared<Track>("Imagine", "John Lennon", 183);
    track2->set_album("Imagine");
    track2->set_genre("Rock");
    tracks.push_back(track2);
    
    auto track3 = std::make_shared<Track>("Hotel California", "Eagles", 391);
    track3->set_album("Hotel California");
    track3->set_genre("Rock");
    tracks.push_back(track3);
    
    // Set tracks reference and index them
    engine.set_tracks(&tracks);
    for (size_t i = 0; i < tracks.size(); ++i) {
        engine.index_track(i, *tracks[i]);
    }
    
    SECTION("Exact search") {
        SearchEngine::SearchOptions options;
        options.mode = SearchEngine::SearchMode::EXACT;
        
        auto results = engine.search("queen", options);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].first->artist() == "Queen");
    }
    
    SECTION("Substring search") {
        SearchEngine::SearchOptions options;
        options.mode = SearchEngine::SearchMode::SUBSTRING;
        
        auto results = engine.search("cal", options);
        REQUIRE(results.size() >= 1);
        
        bool found_california = false;
        for (const auto& [track, score] : results) {
            if (track->title() == "Hotel California") {
                found_california = true;
                break;
            }
        }
        REQUIRE(found_california);
    }
    
    SECTION("Prefix search") {
        SearchEngine::SearchOptions options;
        options.mode = SearchEngine::SearchMode::PREFIX;
        
        auto results = engine.search("hot", options);
        REQUIRE(results.size() >= 1);
    }
    
    SECTION("Case sensitivity") {
        SearchEngine::SearchOptions options;
        options.mode = SearchEngine::SearchMode::EXACT;
        options.case_sensitive = true;
        
        auto results = engine.search("QUEEN", options);
        REQUIRE(results.empty()); // No match with case sensitive
        
        options.case_sensitive = false;
        results = engine.search("QUEEN", options);
        REQUIRE(results.size() == 1);
    }
}

TEST_CASE("SearchEngine field selection", "[search_engine]") {
    SearchEngine engine;
    std::vector<std::shared_ptr<Track>> tracks;
    
    auto track = std::make_shared<Track>("Test Song", "Test Artist", 200);
    track->set_album("Test Album");
    track->set_genre("Test Genre");
    tracks.push_back(track);
    
    engine.set_tracks(&tracks);
    engine.index_track(0, *track);
    
    SearchEngine::SearchOptions options;
    options.mode = SearchEngine::SearchMode::EXACT;
    
    SECTION("Search only titles") {
        options.search_artist = false;
        options.search_album = false;
        options.search_genre = false;
        
        auto results = engine.search("test", options);
        REQUIRE(results.size() == 1);
        
        results = engine.search("artist", options);
        REQUIRE(results.empty());
    }
    
    SECTION("Search only artists") {
        options.search_title = false;
        options.search_album = false;
        options.search_genre = false;
        
        auto results = engine.search("test", options);
        REQUIRE(results.size() == 1);
        
        results = engine.search("song", options);
        REQUIRE(results.empty());
    }
}

TEST_CASE("SearchEngine suggestions", "[search_engine]") {
    SearchEngine engine;
    std::vector<std::shared_ptr<Track>> tracks;
    
    tracks.push_back(std::make_shared<Track>("Paradise City", "Guns N' Roses", 346));
    tracks.push_back(std::make_shared<Track>("Paradise", "Coldplay", 278));
    tracks.push_back(std::make_shared<Track>("Paranoid", "Black Sabbath", 172));
    
    engine.set_tracks(&tracks);
    for (size_t i = 0; i < tracks.size(); ++i) {
        engine.index_track(i, *tracks[i]);
    }
    
    SECTION("Get suggestions") {
        auto suggestions = engine.get_suggestions("para", 10);
        REQUIRE(suggestions.size() >= 2); // "paradise" and "paranoid"
    }
    
    SECTION("Limited suggestions") {
        auto suggestions = engine.get_suggestions("para", 1);
        REQUIRE(suggestions.size() == 1);
    }
}

TEST_CASE("SearchEngine statistics", "[search_engine]") {
    SearchEngine engine;
    std::vector<std::shared_ptr<Track>> tracks;
    
    SECTION("Empty index") {
        REQUIRE(engine.get_indexed_terms_count() == 0);
        REQUIRE(engine.get_indexed_tracks_count() == 0);
    }
    
    SECTION("After indexing") {
        tracks.push_back(std::make_shared<Track>("Song One", "Artist One", 180));
        tracks.push_back(std::make_shared<Track>("Song Two", "Artist Two", 240));
        
        engine.set_tracks(&tracks);
        for (size_t i = 0; i < tracks.size(); ++i) {
            engine.index_track(i, *tracks[i]);
        }
        
        REQUIRE(engine.get_indexed_terms_count() > 0);
        REQUIRE(engine.get_indexed_tracks_count() == 2);
    }
}

TEST_CASE("SearchEngine removal", "[search_engine]") {
    SearchEngine engine;
    std::vector<std::shared_ptr<Track>> tracks;
    
    tracks.push_back(std::make_shared<Track>("Track to Remove", "Artist", 200));
    tracks.push_back(std::make_shared<Track>("Track to Keep", "Artist", 180));
    
    engine.set_tracks(&tracks);
    engine.index_track(0, *tracks[0]);
    engine.index_track(1, *tracks[1]);
    
    SearchEngine::SearchOptions options;
    options.mode = SearchEngine::SearchMode::EXACT;
    
    // Verify both tracks are found
    auto results = engine.search("track", options);
    REQUIRE(results.size() == 2);
    
    // Remove first track
    engine.remove_track(0);
    
    // Verify only one track is found
    results = engine.search("track", options);
    REQUIRE(results.size() == 1);
    
    results = engine.search("remove", options);
    REQUIRE(results.empty());
    
    results = engine.search("keep", options);
    REQUIRE(results.size() == 1);
}

TEST_CASE("SearchEngine suggestions reflect track removal", "[search_engine]") {
    SearchEngine engine;
    std::vector<std::shared_ptr<Track>> tracks{
        std::make_shared<Track>("Paradise", "Shared Artist", 200),
        std::make_shared<Track>("Paranoid", "Shared Artist", 180)
    };
    engine.set_tracks(&tracks);
    engine.rebuild_index(tracks);

    REQUIRE(engine.get_suggestions("para").size() == 2);
    engine.remove_track(0);

    REQUIRE(engine.get_suggestions("parad").empty());
    REQUIRE(engine.get_suggestions("para") == std::vector<std::string>{"paranoid"});
    REQUIRE(engine.get_suggestions("shar") == std::vector<std::string>{"shared"});
    REQUIRE(engine.search("shared").size() == 1);

    engine.remove_track(1);
    REQUIRE(engine.get_suggestions("para").empty());
    REQUIRE(engine.get_suggestions("shar").empty());
    REQUIRE(engine.get_indexed_terms_count() == 0);
}
