#include "audio_library/music_library.h"
#include "audio_library/search_engine.h"
#include "audio_library/file_io.h"
#include <algorithm>
#include <numeric>
#include <fstream>

namespace audio_library {

MusicLibrary::MusicLibrary() 
    : search_engine_(std::make_unique<SearchEngine>()) {
    search_engine_->set_tracks(&tracks_);
}

MusicLibrary::~MusicLibrary() = default;

bool MusicLibrary::add_track(Track track) {
    std::unique_lock lock(mutex_);
    
    // Check for duplicates
    auto it = std::find_if(tracks_.begin(), tracks_.end(),
        [&track](const TrackPtr& t) {
            return t->title() == track.title() && t->artist() == track.artist();
        });
    
    if (it != tracks_.end()) {
        return false; // Duplicate found
    }
    
    auto tracks = tracks_;
    tracks.push_back(std::make_shared<Track>(std::move(track)));
    replace_tracks(std::move(tracks));
    
    return true;
}

bool MusicLibrary::add_track(TrackPtr track) {
    if (!track) return false;
    return add_track(*track);
}

bool MusicLibrary::update_track(const TrackPtr& expected, Track replacement) {
    std::unique_lock lock(mutex_);
    auto it = std::find(tracks_.begin(), tracks_.end(), expected);
    if (it == tracks_.end()) return false;

    const auto duplicate = std::find_if(tracks_.begin(), tracks_.end(),
        [&](const TrackPtr& track) {
            return track != expected && track->title() == replacement.title()
                && track->artist() == replacement.artist();
        });
    if (duplicate != tracks_.end()) return false;

    // Replacing the object preserves previously returned snapshots.
    auto tracks = tracks_;
    tracks[std::distance(tracks_.begin(), it)] = std::make_shared<Track>(std::move(replacement));
    replace_tracks(std::move(tracks));
    return true;
}

bool MusicLibrary::remove_track(const std::string& title, const std::string& artist) {
    std::unique_lock lock(mutex_);
    
    auto it = std::find_if(tracks_.begin(), tracks_.end(),
        [&title, &artist](const TrackPtr& t) {
            return t && t->title() == title && t->artist() == artist;
        });
    
    if (it == tracks_.end()) {
        return false;
    }
    
    auto tracks = tracks_;
    tracks.erase(tracks.begin() + std::distance(tracks_.begin(), it));
    replace_tracks(std::move(tracks));

    return true;
}

bool MusicLibrary::remove_tracks_by_title(const std::string& title) {
    std::unique_lock lock(mutex_);
    
    auto tracks = tracks_;
    auto new_end = std::remove_if(tracks.begin(), tracks.end(),
        [&title](const TrackPtr& t) {
            return t && t->title() == title;
        });
    
    if (new_end == tracks.end()) {
        return false;
    }
    
    tracks.erase(new_end, tracks.end());
    replace_tracks(std::move(tracks));
    
    return true;
}

bool MusicLibrary::remove_tracks_by_artist(const std::string& artist) {
    std::unique_lock lock(mutex_);
    
    auto tracks = tracks_;
    auto new_end = std::remove_if(tracks.begin(), tracks.end(),
        [&artist](const TrackPtr& t) {
            return t && t->artist() == artist;
        });
    
    if (new_end == tracks.end()) {
        return false;
    }
    
    tracks.erase(new_end, tracks.end());
    replace_tracks(std::move(tracks));
    
    return true;
}

void MusicLibrary::clear() {
    std::unique_lock lock(mutex_);
    replace_tracks({});
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::find_by_title(const std::string& title) const {
    std::shared_lock lock(mutex_);
    
    std::vector<TrackPtr> results;
    auto range = title_index_.equal_range(title);
    
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second < tracks_.size()) {
            results.push_back(tracks_[it->second]);
        }
    }
    
    return results;
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::find_by_artist(const std::string& artist) const {
    std::shared_lock lock(mutex_);
    
    std::vector<TrackPtr> results;
    auto range = artist_index_.equal_range(artist);
    
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second < tracks_.size()) {
            results.push_back(tracks_[it->second]);
        }
    }
    
    return results;
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::find_by_album(const std::string& album) const {
    std::shared_lock lock(mutex_);
    
    std::vector<TrackPtr> results;
    auto range = album_index_.equal_range(album);
    
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second < tracks_.size()) {
            results.push_back(tracks_[it->second]);
        }
    }
    
    return results;
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::find_by_genre(const std::string& genre) const {
    std::shared_lock lock(mutex_);
    
    std::vector<TrackPtr> results;
    auto range = genre_index_.equal_range(genre);
    
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second < tracks_.size()) {
            results.push_back(tracks_[it->second]);
        }
    }
    
    return results;
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::search(const std::string& query) const {
    std::shared_lock lock(mutex_);
    
    auto search_results = search_engine_->search(query);
    std::vector<TrackPtr> results;
    
    for (const auto& [track_ptr, score] : search_results) {
        results.push_back(track_ptr);
    }
    
    return results;
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::search_advanced(
    const std::string& query, const TrackFilter& filter) const {
    
    auto results = search(query);
    if (filter) {
        results.erase(std::remove_if(results.begin(), results.end(),
            [&filter](const TrackPtr& track) { return !filter(*track); }), results.end());
    }
    return results;
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::get_all_tracks() const {
    std::shared_lock lock(mutex_);
    return {tracks_.begin(), tracks_.end()};
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::get_tracks_sorted(
    const TrackComparator& comparator) const {
    
    auto sorted_tracks = get_all_tracks();
    
    std::sort(sorted_tracks.begin(), sorted_tracks.end(),
        [&comparator](const TrackPtr& a, const TrackPtr& b) {
            return comparator(*a, *b);
        });
    
    return sorted_tracks;
}

std::vector<std::string> MusicLibrary::get_all_artists() const {
    std::shared_lock lock(mutex_);
    
    std::unordered_set<std::string> unique_artists;
    for (const auto& track : tracks_) {
        if (track) {
            unique_artists.insert(track->artist());
        }
    }
    
    return std::vector<std::string>(unique_artists.begin(), unique_artists.end());
}

std::vector<std::string> MusicLibrary::get_all_albums() const {
    std::shared_lock lock(mutex_);
    
    std::unordered_set<std::string> unique_albums;
    for (const auto& track : tracks_) {
        if (track && !track->album().empty()) {
            unique_albums.insert(track->album());
        }
    }
    
    return std::vector<std::string>(unique_albums.begin(), unique_albums.end());
}

std::vector<std::string> MusicLibrary::get_all_genres() const {
    std::shared_lock lock(mutex_);
    
    std::unordered_set<std::string> unique_genres;
    for (const auto& track : tracks_) {
        if (track && !track->genre().empty()) {
            unique_genres.insert(track->genre());
        }
    }
    
    return std::vector<std::string>(unique_genres.begin(), unique_genres.end());
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::filter_tracks(const TrackFilter& filter) const {
    if (!filter) return get_all_tracks();
    
    auto snapshot = get_all_tracks();
    std::vector<TrackPtr> results;
    
    for (const auto& track : snapshot) {
        if (track && filter(*track)) {
            results.push_back(track);
        }
    }
    
    return results;
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::get_tracks_by_year_range(
    int start_year, int end_year) const {
    
    return filter_tracks([start_year, end_year](const Track& track) {
        return track.year() >= start_year && track.year() <= end_year;
    });
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::get_tracks_by_rating_range(
    double min_rating, double max_rating) const {
    
    return filter_tracks([min_rating, max_rating](const Track& track) {
        return track.rating() >= min_rating && track.rating() <= max_rating;
    });
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::get_most_played_tracks(size_t limit) const {
    auto tracks = get_all_tracks();
    
    std::sort(tracks.begin(), tracks.end(),
        [](const TrackPtr& a, const TrackPtr& b) {
            return a->play_count() > b->play_count();
        });
    
    if (tracks.size() > limit) {
        tracks.resize(limit);
    }
    
    return tracks;
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::get_top_rated_tracks(size_t limit) const {
    auto tracks = get_all_tracks();
    
    // Filter out unrated tracks
    tracks.erase(std::remove_if(tracks.begin(), tracks.end(),
        [](const TrackPtr& t) { return t->rating() == 0.0; }), tracks.end());
    
    std::sort(tracks.begin(), tracks.end(),
        [](const TrackPtr& a, const TrackPtr& b) {
            return a->rating() > b->rating();
        });
    
    if (tracks.size() > limit) {
        tracks.resize(limit);
    }
    
    return tracks;
}

size_t MusicLibrary::size() const {
    std::shared_lock lock(mutex_);
    return tracks_.size();
}

bool MusicLibrary::empty() const {
    std::shared_lock lock(mutex_);
    return tracks_.empty();
}

MusicLibrary::Statistics MusicLibrary::get_statistics() const {
    std::shared_lock lock(mutex_);
    
    Statistics stats;
    stats.total_tracks = tracks_.size();
    
    std::unordered_set<std::string> unique_artists;
    std::unordered_set<std::string> unique_albums;
    double total_rating = 0.0;
    size_t rated_tracks = 0;
    
    for (const auto& track : tracks_) {
        if (!track) continue;
        
        unique_artists.insert(track->artist());
        if (!track->album().empty()) {
            unique_albums.insert(track->album());
        }
        
        stats.total_duration += track->duration();
        stats.total_play_count += track->play_count();
        
        if (track->rating() > 0) {
            total_rating += track->rating();
            ++rated_tracks;
        }
    }
    
    stats.total_artists = unique_artists.size();
    stats.total_albums = unique_albums.size();
    stats.average_rating = rated_tracks > 0 ? total_rating / rated_tracks : 0.0;
    
    return stats;
}

bool MusicLibrary::import_from_csv(const std::string& filename) {
    try {
        FileIO::ImportOptions options;
        options.skip_invalid_lines = false;
        auto tracks = FileIO::import_csv(filename, options);
        
        for (auto& track : tracks) {
            add_track(std::move(track));
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool MusicLibrary::export_to_csv(const std::string& filename) const {
    std::shared_lock lock(mutex_);
    return FileIO::export_csv(filename, tracks_);
}

bool MusicLibrary::import_from_json(const std::string& filename) {
    try {
        FileIO::ImportOptions options;
        options.skip_invalid_lines = false;
        auto tracks = FileIO::import_json(filename, options);
        
        for (auto& track : tracks) {
            add_track(std::move(track));
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool MusicLibrary::export_to_json(const std::string& filename) const {
    std::shared_lock lock(mutex_);
    return FileIO::export_json(filename, tracks_);
}

void MusicLibrary::for_each_track(const std::function<void(const Track&)>& func) const {
    auto snapshot = get_all_tracks();
    
    for (const auto& track : snapshot) {
        if (track) {
            func(*track);
        }
    }
}

void MusicLibrary::replace_tracks(std::vector<std::shared_ptr<Track>> tracks) {
    std::unordered_multimap<std::string, size_t> title_index, artist_index, album_index, genre_index;
    auto search_engine = std::make_unique<SearchEngine>();
    search_engine->set_tracks(&tracks_);

    // All allocations, including the search trie, complete before changing live state.
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i]) {
            const auto& track = *tracks[i];
            title_index.emplace(track.title(), i);
            artist_index.emplace(track.artist(), i);
            if (!track.album().empty()) album_index.emplace(track.album(), i);
            if (!track.genre().empty()) genre_index.emplace(track.genre(), i);
            search_engine->index_track(i, track);
        }
    }

    static_assert(noexcept(tracks_.swap(tracks))
                  && noexcept(title_index_.swap(title_index))
                  && noexcept(artist_index_.swap(artist_index))
                  && noexcept(album_index_.swap(album_index))
                  && noexcept(genre_index_.swap(genre_index))
                  && noexcept(search_engine_.swap(search_engine)),
                  "Committing replacement tracks and indexes must not throw");
    tracks_.swap(tracks);
    title_index_.swap(title_index);
    artist_index_.swap(artist_index);
    album_index_.swap(album_index);
    genre_index_.swap(genre_index);
    search_engine_.swap(search_engine);
}

std::vector<MusicLibrary::TrackPtr> MusicLibrary::get_tracks_by_indexes(
    const std::vector<size_t>& indexes) const {
    
    std::vector<TrackPtr> results;
    for (size_t index : indexes) {
        if (index < tracks_.size()) {
            results.push_back(tracks_[index]);
        }
    }
    return results;
}

} // namespace audio_library
