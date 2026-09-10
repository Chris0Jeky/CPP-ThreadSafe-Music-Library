#include "../include/catch.hpp"
#include "audio_library/track.h"

using namespace audio_library;

TEST_CASE("Track construction and getters", "[track]") {
    SECTION("Basic construction") {
        Track track("Bohemian Rhapsody", "Queen", 354);
        
        REQUIRE(track.title() == "Bohemian Rhapsody");
        REQUIRE(track.artist() == "Queen");
        REQUIRE(track.duration_seconds() == 354);
    }
    
    SECTION("Construction with chrono duration") {
        Track track("Imagine", "John Lennon", std::chrono::seconds(183));
        
        REQUIRE(track.duration() == std::chrono::seconds(183));
        REQUIRE(track.duration_seconds() == 183);
    }
}

TEST_CASE("Track metadata", "[track]") {
    Track track("Hotel California", "Eagles", 391);
    
    SECTION("Optional metadata") {
        track.set_album("Hotel California");
        track.set_genre("Rock");
        track.set_year(1976);
        track.set_rating(4.8);
        
        REQUIRE(track.album() == "Hotel California");
        REQUIRE(track.genre() == "Rock");
        REQUIRE(track.year() == 1976);
        REQUIRE(track.rating() == Approx(4.8));
    }
    
    SECTION("Rating bounds") {
        track.set_rating(-1.0);
        REQUIRE(track.rating() == 0.0);
        
        track.set_rating(6.0);
        REQUIRE(track.rating() == 5.0);
        
        track.set_rating(3.5);
        REQUIRE(track.rating() == 3.5);
    }
}

TEST_CASE("Track formatting", "[track]") {
    SECTION("Duration formatting") {
        Track track1("Short", "Artist", 45);
        REQUIRE(track1.format_duration() == "0:45");
        
        Track track2("Medium", "Artist", 183);
        REQUIRE(track2.format_duration() == "3:03");
        
        Track track3("Long", "Artist", 3661);
        REQUIRE(track3.format_duration() == "1:01:01");
    }
    
    SECTION("String representation") {
        Track track("Song Title", "Artist Name", 180);
        track.set_album("Album Name");
        track.set_year(2020);
        track.set_rating(4.5);
        
        std::string str = track.to_string();
        REQUIRE(str.find("Song Title") != std::string::npos);
        REQUIRE(str.find("Artist Name") != std::string::npos);
        REQUIRE(str.find("3:00") != std::string::npos);
        REQUIRE(str.find("Album Name") != std::string::npos);
        REQUIRE(str.find("2020") != std::string::npos);
        REQUIRE(str.find("4.5") != std::string::npos);
    }
}

TEST_CASE("Track CSV operations", "[track]") {
    SECTION("Export to CSV") {
        Track track("Title, with comma", "Artist \"quoted\"", 120);
        track.set_album("Album");
        track.set_genre("Pop");
        track.set_year(2021);
        track.set_rating(3.5);
        
        std::string csv = track.to_csv();
        REQUIRE(csv.find("\"Title, with comma\"") != std::string::npos);
        REQUIRE(csv.find("\"Artist \"\"quoted\"\"\"") != std::string::npos);
    }
    
    SECTION("Import from CSV") {
        std::string csv_line = "Song Title,Artist Name,240,Album Name,Rock,1985,100,4.2";
        auto track_opt = Track::from_csv_line(csv_line);
        
        REQUIRE(track_opt.has_value());
        Track track = *track_opt;
        
        REQUIRE(track.title() == "Song Title");
        REQUIRE(track.artist() == "Artist Name");
        REQUIRE(track.duration_seconds() == 240);
        REQUIRE(track.album() == "Album Name");
        REQUIRE(track.genre() == "Rock");
        REQUIRE(track.year() == 1985);
        REQUIRE(track.play_count() == 100);
        REQUIRE(track.rating() == Approx(4.2));
    }
    
    SECTION("Import CSV with quotes") {
        std::string csv_line = "\"Title, with comma\",\"Artist \"\"Name\"\"\",180";
        auto track_opt = Track::from_csv_line(csv_line);
        
        REQUIRE(track_opt.has_value());
        Track track = *track_opt;
        
        REQUIRE(track.title() == "Title, with comma");
        REQUIRE(track.artist() == "Artist \"Name\"");
        REQUIRE(track.duration_seconds() == 180);
    }
}

TEST_CASE("Track JSON operations", "[track]") {
    SECTION("Export to JSON") {
        Track track("Test Song", "Test Artist", 200);
        track.set_album("Test Album");
        track.set_genre("Test Genre");
        track.set_year(2022);
        track.set_rating(5.0);
        
        std::string json = track.to_json();
        REQUIRE(json.find("\"title\":\"Test Song\"") != std::string::npos);
        REQUIRE(json.find("\"artist\":\"Test Artist\"") != std::string::npos);
        REQUIRE(json.find("\"duration\":200") != std::string::npos);
        REQUIRE(json.find("\"album\":\"Test Album\"") != std::string::npos);
        REQUIRE(json.find("\"genre\":\"Test Genre\"") != std::string::npos);
        REQUIRE(json.find("\"year\":2022") != std::string::npos);
        REQUIRE(json.find("\"rating\":5.0") != std::string::npos);
    }
    
    SECTION("Import from JSON") {
        std::string json = R"({
            "title": "JSON Song",
            "artist": "JSON Artist",
            "duration": 150,
            "album": "JSON Album",
            "genre": "Electronic",
            "year": 2023,
            "play_count": 50,
            "rating": 4.0
        })";
        
        auto track_opt = Track::from_json(json);
        
        REQUIRE(track_opt.has_value());
        Track track = *track_opt;
        
        REQUIRE(track.title() == "JSON Song");
        REQUIRE(track.artist() == "JSON Artist");
        REQUIRE(track.duration_seconds() == 150);
        REQUIRE(track.album() == "JSON Album");
        REQUIRE(track.genre() == "Electronic");
        REQUIRE(track.year() == 2023);
        REQUIRE(track.play_count() == 50);
        REQUIRE(track.rating() == 4.0);
    }
}

TEST_CASE("Track comparison", "[track]") {
    Track track1("Song A", "Artist 1", 100);
    Track track2("Song A", "Artist 1", 200);
    Track track3("Song B", "Artist 1", 100);
    Track track4("Song A", "Artist 2", 100);
    
    SECTION("Equality") {
        REQUIRE(track1 == track2); // Same title and artist
        REQUIRE(!(track1 == track3)); // Different title
        REQUIRE(!(track1 == track4)); // Different artist
    }
    
    SECTION("Ordering") {
        REQUIRE(track1 < track4); // Artist 1 < Artist 2
        REQUIRE(track1 < track3); // Same artist, Song A < Song B
    }
}

TEST_CASE("Track search matching", "[track]") {
    Track track("Bohemian Rhapsody", "Queen", 354);
    track.set_album("A Night at the Opera");
    track.set_genre("Rock");
    
    SECTION("Case insensitive matching") {
        REQUIRE(track.matches_query("bohemian"));
        REQUIRE(track.matches_query("QUEEN"));
        REQUIRE(track.matches_query("opera"));
        REQUIRE(track.matches_query("rock"));
    }
    
    SECTION("Partial matching") {
        REQUIRE(track.matches_query("rhap"));
        REQUIRE(track.matches_query("que"));
        REQUIRE(track.matches_query("night"));
    }
    
    SECTION("No match") {
        REQUIRE_FALSE(track.matches_query("mozart"));
        REQUIRE_FALSE(track.matches_query("jazz"));
    }
}
