#include "../include/catch.hpp"
#include "audio_library/music_library.h"
#include <future>
#include <thread>
#include <type_traits>
#include <atomic>

using namespace audio_library;

namespace {
template<class Results>
void require_immutable(const Results&) {
    static_assert(std::is_same_v<typename Results::value_type, std::shared_ptr<const Track>>,
                  "Library queries must not expose mutable tracks");
}

bool completes(const std::function<bool()>& operation) {
    // packaged_task futures do not block on destruction after a watchdog timeout.
    // Operations own their library so a stuck, detached worker cannot dangle.
    std::packaged_task<bool()> task(operation);
    auto result = task.get_future();
    std::thread worker(std::move(task));
    if (result.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
        worker.detach();
        return false;
    }
    worker.join();
    return result.get();
}
}

TEST_CASE("Every track query returns immutable handles", "[music_library][regression]") {
    MusicLibrary library;
    library.add_track(Track("Song", "Artist", 180));
    require_immutable(library.get_all_tracks());
    require_immutable(library.find_by_title("Song"));
    require_immutable(library.find_by_artist("Artist"));
    require_immutable(library.find_by_album("Album"));
    require_immutable(library.find_by_genre("Genre"));
    require_immutable(library.search("Song"));
    require_immutable(library.search_advanced("Song"));
    require_immutable(library.get_tracks_sorted([](const Track& a, const Track& b) { return a < b; }));
    require_immutable(library.filter_tracks(nullptr));
    require_immutable(library.get_tracks_by_year_range(2000, 2020));
    require_immutable(library.get_tracks_by_rating_range(0, 5));
    require_immutable(library.get_most_played_tracks());
    require_immutable(library.get_top_rated_tracks());
    REQUIRE(library.size() == 1);
}

TEST_CASE("Updates replace snapshots and rebuild every index", "[music_library][regression]") {
    MusicLibrary library;
    REQUIRE(library.add_track(Track("Other", "Artist", 100)));
    auto input = std::make_shared<Track>("Oldtitle", "Oldartist", 180);
    input->set_album("Oldalbum");
    input->set_genre("Oldgenre");
    REQUIRE(library.add_track(input));
    input->set_album("External");
    auto original = library.find_by_title("Oldtitle").at(0);
    REQUIRE(original->album() == "Oldalbum");

    Track replacement("Newtitle", "Newartist", 240);
    replacement.set_album("Newalbum");
    replacement.set_genre("Newgenre");
    replacement.set_year(2026);
    replacement.set_rating(4.5);
    replacement.increment_play_count();
    REQUIRE(library.update_track(original, replacement));
    REQUIRE(library.find_by_title("Oldtitle").empty());
    REQUIRE(library.find_by_artist("Oldartist").empty());
    REQUIRE(library.find_by_album("Oldalbum").empty());
    REQUIRE(library.find_by_genre("Oldgenre").empty());
    REQUIRE(library.search("Old").empty());
    auto current = library.find_by_title("Newtitle").at(0);
    REQUIRE(library.find_by_artist("Newartist").at(0) == current);
    REQUIRE(library.find_by_album("Newalbum").at(0) == current);
    REQUIRE(library.find_by_genre("Newgenre").at(0) == current);
    for (const auto& term : {"Newtitle", "Newartist", "Newalbum", "Newgenre"}) {
        REQUIRE(library.search(term).at(0) == current);
    }
    REQUIRE(library.get_tracks_by_year_range(2026, 2026).at(0) == current);
    REQUIRE(library.get_top_rated_tracks().at(0) == current);
    REQUIRE(library.get_statistics().total_play_count == 1);
    REQUIRE(library.get_statistics().average_rating == Approx(4.5));
    REQUIRE(original->title() == "Oldtitle");
    REQUIRE(original->album() == "Oldalbum");
    REQUIRE_FALSE(library.update_track(original, replacement)); // Stale version.
    REQUIRE_FALSE(library.update_track(nullptr, replacement));
    REQUIRE_FALSE(library.update_track(std::make_shared<Track>(replacement), replacement));
    REQUIRE_FALSE(library.update_track(current, Track("Other", "Artist", 100)));
    REQUIRE(library.find_by_title("Newtitle").at(0) == current);
    REQUIRE_FALSE(library.add_track(replacement)); // Still enforces duplicates.
    REQUIRE(library.remove_track("Other", "Artist")); // Rebuild after positions shift.
    REQUIRE(library.find_by_album("Newalbum").at(0) == current);
    REQUIRE(library.search("Newgenre").at(0) == current);
    REQUIRE(library.remove_track("Newtitle", "Newartist"));
    REQUIRE_FALSE(library.update_track(current, replacement));
    REQUIRE(current->album() == "Newalbum");
    REQUIRE(library.empty());
}

TEST_CASE("Concurrent metadata updates preserve reader snapshots", "[music_library][regression][thread]") {
    auto library = std::make_shared<MusicLibrary>();
    Track initial("Song", "Artist", 180);
    initial.set_album("Before");
    library->add_track(initial);
    REQUIRE(completes([library] {
        std::atomic<bool> start{false};
        std::atomic<bool> valid{true};
        std::thread writer([&] {
            while (!start.load()) std::this_thread::yield();
            for (int i = 0; i < 200; ++i) {
                auto current = library->get_all_tracks().at(0);
                Track edited = *current;
                edited.set_album("After");
                edited.set_genre(i % 2 == 0 ? "Rock" : "Jazz");
                edited.set_rating(i % 2 == 0 ? 5.0 : 4.0);
                edited.increment_play_count();
                if (!library->update_track(current, edited)) valid = false;
            }
        });
        start = true;
        for (int i = 0; i < 200; ++i) {
            auto snapshot = library->get_all_tracks().at(0);
            const Track before = *snapshot;
            (void)library->get_statistics();
            (void)library->filter_tracks([](const Track& t) { return t.rating() >= 4.0; });
            (void)library->get_tracks_sorted([](const Track& a, const Track& b) { return a < b; });
            (void)library->search("Song");
            (void)library->get_most_played_tracks();
            if (snapshot->album() != before.album() || snapshot->genre() != before.genre()
                || snapshot->rating() != before.rating() || snapshot->play_count() != before.play_count()) {
                valid = false;
            }
        }
        writer.join();
        return valid.load() && library->get_statistics().total_play_count == 200
            && library->find_by_album("Before").empty()
            && library->find_by_genre("Jazz").size() == 1;
    }));
}

TEST_CASE("Library callbacks can reenter and retain their snapshot", "[music_library][regression][reentrant]") {
    auto library = std::make_shared<MusicLibrary>();
    library->add_track(Track("Zulu", "Artist", 180));
    library->add_track(Track("Alpha", "Artist", 150));

    SECTION("Iteration can read, search and remove its entries") {
        REQUIRE(completes([library] {
            size_t visited = 0;
            library->for_each_track([&](const Track& track) {
                (void)library->size();
                (void)library->search("Artist");
                library->clear();
                if (track.artist() == "Artist") ++visited;
            });
            return visited == 2 && library->empty();
        }));
    }
    SECTION("Filter can replace the live collection") {
        REQUIRE(completes([library] {
            auto results = library->filter_tracks([&](const Track& track) {
                library->clear();
                library->add_track(Track("New", "Other", 100));
                return track.artist() == "Artist";
            });
            return results.size() == 2 && library->size() == 1
                && results[0]->title() == "Zulu" && results[1]->title() == "Alpha";
        }));
    }
    SECTION("Comparator can clear the library") {
        REQUIRE(completes([library] {
            auto results = library->get_tracks_sorted([&](const Track& a, const Track& b) {
                (void)library->size();
                library->clear();
                return a.title() < b.title();
            });
            return library->empty() && results.size() == 2
                && results[0]->title() == "Alpha" && results[1]->title() == "Zulu";
        }));
    }
    SECTION("Advanced search filter can remove matching tracks") {
        REQUIRE(completes([library] {
            auto results = library->search_advanced("Artist", [&](const Track& track) {
                library->remove_track(track.title(), track.artist());
                return track.artist() == "Artist";
            });
            return results.size() == 2 && library->empty()
                && results[0]->artist() == "Artist" && results[1]->artist() == "Artist";
        }));
    }
}
