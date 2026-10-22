#include "audio_library/music_library.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
thread_local std::ptrdiff_t allocations_before_failure = -1;

struct FailAllocation {
    explicit FailAllocation(std::ptrdiff_t count) {
        allocations_before_failure = count;
    }
    ~FailAllocation() { allocations_before_failure = -1; }
};
}

// This override is linked only into this test executable. Fail one allocation
// during the mutation, then allow exception handling and state checks to allocate.
void* operator new(std::size_t size) {
    if (allocations_before_failure == 0) {
        allocations_before_failure = -1;
        throw std::bad_alloc();
    }
    if (allocations_before_failure > 0) --allocations_before_failure;
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete(void* memory, std::size_t) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }

namespace {
using audio_library::MusicLibrary;
using audio_library::Track;
using Tracks = std::vector<MusicLibrary::TrackPtr>;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Track make_track(const char* title, const char* artist, const char* album, const char* genre) {
    Track track(title, artist, 180);
    track.set_album(album);
    track.set_genre(genre);
    return track;
}

void require_same_tracks(const Tracks& actual, const Tracks& expected) {
    require(std::is_permutation(actual.begin(), actual.end(), expected.begin(), expected.end()),
            "lookup/search results changed");
}

void verify_library(const MusicLibrary& library, const Tracks& expected, const Tracks& probes) {
    require(library.get_all_tracks() == expected, "stored tracks changed");
    require(library.size() == expected.size(), "library size changed");
    require(library.empty() == expected.empty(), "library emptiness changed");
    for (const auto& probe : probes) {
        Tracks titles, artists, albums, genres;
        for (const auto& track : expected) {
            if (track->title() == probe->title()) titles.push_back(track);
            if (track->artist() == probe->artist()) artists.push_back(track);
            if (track->album() == probe->album()) albums.push_back(track);
            if (track->genre() == probe->genre()) genres.push_back(track);
        }
        require_same_tracks(library.find_by_title(probe->title()), titles);
        require_same_tracks(library.find_by_artist(probe->artist()), artists);
        require_same_tracks(library.find_by_album(probe->album()), albums);
        require_same_tracks(library.find_by_genre(probe->genre()), genres);
        require_same_tracks(library.search(probe->title()), titles);
        require_same_tracks(library.search(probe->artist()), artists);
        require_same_tracks(library.search(probe->album()), albums);
        require_same_tracks(library.search(probe->genre()), genres);
    }
}

enum class Mutation { Update, Remove, RemoveTitle, RemoveArtist, Clear };

void exercise(Mutation mutation, const char* name) {
    // Sweep every allocation site, including allocations late in trie rebuilding.
    // The first successful call also proves mutations still work after the change.
    for (std::ptrdiff_t fail_at = 0; fail_at < 10000; ++fail_at) {
        MusicLibrary library;
        library.add_track(make_track("oldtitle", "oldartist", "oldalbum", "oldgenre"));
        library.add_track(make_track("oldtitle", "otherartist", "otheralbum", "othergenre"));
        library.add_track(make_track("othertitle", "oldartist", "otheralbum", "othergenre"));
        library.add_track(make_track("survivortitle", "survivorartist", "survivoralbum", "survivorgenre"));
        const auto before = library.get_all_tracks();
        Track replacement = make_track("newtitle", "newartist", "newalbum", "newgenre");
        auto probes = before;
        probes.push_back(std::make_shared<Track>(replacement));
        bool changed = false;
        bool failed = false;
        {
            FailAllocation failure(fail_at);
            try {
                switch (mutation) {
                case Mutation::Update:
                    changed = library.update_track(before[0], std::move(replacement));
                    break;
                case Mutation::Remove:
                    changed = library.remove_track(before[0]->title(), before[0]->artist());
                    break;
                case Mutation::RemoveTitle:
                    changed = library.remove_tracks_by_title(before[0]->title());
                    break;
                case Mutation::RemoveArtist:
                    changed = library.remove_tracks_by_artist(before[0]->artist());
                    break;
                case Mutation::Clear:
                    library.clear();
                    changed = true;
                    break;
                }
            } catch (const std::bad_alloc&) {
                failed = true;
            }
        }
        if (failed) {
            verify_library(library, before, probes);
            continue;
        }
        require(changed, "mutation unexpectedly rejected");
        require(fail_at > 0, "allocation failure was never exercised");
        Tracks expected = before;
        switch (mutation) {
        case Mutation::Update: {
            const auto current = library.get_all_tracks().at(0);
            require(current != before[0], "update mutated the original snapshot");
            require(current->title() == "newtitle" && current->artist() == "newartist"
                    && current->album() == "newalbum" && current->genre() == "newgenre",
                    "replacement metadata is incorrect");
            require(before[0]->title() == "oldtitle", "original snapshot changed");
            expected[0] = current;
            break;
        }
        case Mutation::Remove:
            expected.erase(expected.begin());
            break;
        case Mutation::RemoveTitle:
            expected.erase(expected.begin(), expected.begin() + 2);
            break;
        case Mutation::RemoveArtist:
            expected = {before[1], before[3]};
            break;
        case Mutation::Clear:
            expected.clear();
            break;
        }
        verify_library(library, expected, probes);
        // Exercise the committed search engine's reference again after mutation.
        require(library.add_track(make_track("followuptitle", "followupartist", "followupalbum", "followupgenre")),
                "addition after commit failed");
        require_same_tracks(library.search("followuptitle"), library.find_by_title("followuptitle"));
        std::cout << name << ": preserved state at " << fail_at << " allocation failure sites\n";
        return;
    }
    throw std::runtime_error("allocation sweep did not reach a successful mutation");
}
}

int main() {
    try {
        exercise(Mutation::Update, "update_track");
        exercise(Mutation::Remove, "remove_track");
        exercise(Mutation::RemoveTitle, "remove_tracks_by_title");
        exercise(Mutation::RemoveArtist, "remove_tracks_by_artist");
        exercise(Mutation::Clear, "clear");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
