#include "../include/catch.hpp"
#include "audio_library/search_engine.h"
#include "audio_library/track.h"
#include <algorithm>
#include <cctype>

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
        
        auto exact_case = engine.search("Queen", options);
        REQUIRE(exact_case.size() == 1);
        REQUIRE(exact_case[0].first == track1);

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

TEST_CASE("SearchEngine preserves token case in every field and mode", "[search_engine][case_sensitive]") {
    using Mode = SearchEngine::SearchMode;
    struct FieldCase {
        const char* name;
        std::string token;
        double weight;
    };
    const FieldCase fields[] = {
        {"title", "BoHemian", 1.0}, {"artist", "QuEen", 0.8},
        {"album", "NiGhtOpera", 0.6}, {"genre", "ArTRock", 0.4}
    };

    for (size_t field = 0; field < 4; ++field) {
        for (auto mode : {Mode::EXACT, Mode::PREFIX, Mode::SUBSTRING, Mode::FUZZY, Mode::REGEX}) {
            DYNAMIC_SECTION(fields[field].name << " mode " << static_cast<int>(mode)) {
                SearchEngine engine;
                auto track = std::make_shared<Track>("(BoHemian!)", "QuEen", 354);
                track->set_album("NiGhtOpera");
                track->set_genre("ArTRock");
                std::vector<SearchEngine::TrackPtr> tracks{track};
                engine.set_tracks(&tracks);
                engine.index_track(0, *track);

                SearchEngine::SearchOptions options;
                options.mode = mode;
                options.case_sensitive = true;
                options.search_title = field == 0;
                options.search_artist = field == 1;
                options.search_album = field == 2;
                options.search_genre = field == 3;
                // FUZZY may accept case changes as edits; require near-exact relevance here.
                if (mode == Mode::FUZZY) options.min_relevance = fields[field].weight * 0.9;

                auto query = fields[field].token;
                if (mode == Mode::PREFIX) query = query.substr(0, 4);
                if (mode == Mode::SUBSTRING) query = query.substr(1, 4);
                if (mode == Mode::REGEX) query = "^" + query + "$";
                const auto results = engine.search(query, options);
                REQUIRE(results.size() == 1);
                REQUIRE(results[0].first == track);

                for (bool uppercase : {false, true}) {
                    auto wrong_case = query;
                    std::transform(wrong_case.begin(), wrong_case.end(), wrong_case.begin(),
                        [uppercase](unsigned char c) {
                            return static_cast<char>(uppercase ? std::toupper(c) : std::tolower(c));
                        });
                    REQUIRE(engine.search(wrong_case, options).empty());
                    options.case_sensitive = false;
                    const auto insensitive = engine.search(wrong_case, options);
                    REQUIRE(insensitive.size() == 1);
                    REQUIRE(insensitive[0].first == track);
                    REQUIRE(insensitive[0].second == Approx(results[0].second));
                    options.case_sensitive = true;
                }

                // Disabling the selected field must suppress either posting index.
                options.search_title = options.search_artist = false;
                options.search_album = options.search_genre = false;
                REQUIRE(engine.search(query, options).empty());
                options.case_sensitive = false;
                REQUIRE(engine.search(query, options).empty());
            }
        }
    }
}

TEST_CASE("SearchEngine fuzzy relevance counts case changes as edits", "[search_engine][case_sensitive]") {
    SearchEngine engine;
    std::vector<SearchEngine::TrackPtr> tracks{std::make_shared<Track>("Queen", "Other", 354)};
    engine.set_tracks(&tracks);
    engine.index_track(0, *tracks[0]);
    SearchEngine::SearchOptions options;
    options.mode = SearchEngine::SearchMode::FUZZY;
    options.case_sensitive = true;
    options.search_artist = false;

    const auto exact = engine.search("Queen", options);
    REQUIRE(exact.size() == 1);
    REQUIRE(exact[0].second == Approx(1.0));
    for (const auto* query : {"queen", "Queem"}) {
        const auto fuzzy = engine.search(query, options);
        REQUIRE(fuzzy.size() == 1);
        REQUIRE(fuzzy[0].second == Approx(0.32));
    }
    REQUIRE(engine.search("QUEEN", options).empty());
    options.case_sensitive = false;
    const auto insensitive = engine.search("QUEEN", options);
    REQUIRE(insensitive.size() == 1);
    REQUIRE(insensitive[0].second == Approx(1.0));
}

TEST_CASE("SearchEngine maintains both case indexes", "[search_engine][case_sensitive]") {
    SearchEngine engine;
    std::vector<SearchEngine::TrackPtr> tracks{
        std::make_shared<Track>("Queen", "Other", 100),
        std::make_shared<Track>("queen", "Other", 100)
    };
    engine.set_tracks(&tracks);
    engine.rebuild_index(tracks);
    SearchEngine::SearchOptions options;
    options.mode = SearchEngine::SearchMode::EXACT;
    options.case_sensitive = true;

    REQUIRE(engine.get_indexed_terms_count() == 2);
    REQUIRE(engine.get_indexed_tracks_count() == 2);
    for (size_t id = 0; id < tracks.size(); ++id) {
        const auto results = engine.search(tracks[id]->title(), options);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].first == tracks[id]);
    }
    REQUIRE(engine.search("QUEEN").size() == 2);
    REQUIRE(engine.get_suggestions("QUE") == std::vector<std::string>{"queen"});

    SECTION("Remove") {
        engine.remove_track(0);
        REQUIRE(engine.search("Queen", options).empty());
        REQUIRE(engine.search("queen", options).size() == 1);
        REQUIRE(engine.search("QUEEN").size() == 1);
        REQUIRE(engine.get_indexed_tracks_count() == 1);
    }
    SECTION("Clear") {
        engine.clear_index();
        REQUIRE(engine.search("Queen", options).empty());
        REQUIRE(engine.search("queen", options).empty());
        REQUIRE(engine.search("QUEEN").empty());
        REQUIRE(engine.get_indexed_terms_count() == 0);
        REQUIRE(engine.get_suggestions("QUE").empty());
    }
    SECTION("Rebuild") {
        tracks[0] = std::make_shared<Track>("Updated", "Other", 100);
        engine.rebuild_index(tracks);
        REQUIRE(engine.search("Queen", options).empty());
        REQUIRE(engine.search("queen", options).size() == 1);
        REQUIRE(engine.search("Updated", options).size() == 1);
        REQUIRE(engine.search("updated").size() == 1);
    }
}
