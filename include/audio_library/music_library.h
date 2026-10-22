#pragma once

#include "track.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <functional>

namespace audio_library {

// Forward declarations
class SearchEngine;

class MusicLibrary {
public:
    using TrackPtr = std::shared_ptr<const Track>;
    using TrackFilter = std::function<bool(const Track&)>;
    using TrackComparator = std::function<bool(const Track&, const Track&)>;
    
    // Statistics structure
    struct Statistics {
        size_t total_tracks{0};
        size_t total_artists{0};
        size_t total_albums{0};
        std::chrono::seconds total_duration{0};
        double average_rating{0.0};
        size_t total_play_count{0};
    };
    
    // Constructor/Destructor
    MusicLibrary();
    ~MusicLibrary();
    
    // Track management
    bool add_track(Track track);
    bool add_track(TrackPtr track);
    // Replace only if expected is still current; false for stale/foreign handles
    // or a replacement duplicating another track's title and artist.
    bool update_track(const TrackPtr& expected, Track replacement);
    bool remove_track(const std::string& title, const std::string& artist);
    bool remove_tracks_by_title(const std::string& title);
    bool remove_tracks_by_artist(const std::string& artist);
    void clear();
    
    // Queries return immutable handles that remain valid across updates/removals.
    // Callbacks run without the library lock, on a snapshot taken before callbacks
    // begin. They may reenter/mutate the library; those changes do not alter the snapshot.
    // Search operations
    [[nodiscard]] std::vector<TrackPtr> find_by_title(const std::string& title) const;
    [[nodiscard]] std::vector<TrackPtr> find_by_artist(const std::string& artist) const;
    [[nodiscard]] std::vector<TrackPtr> find_by_album(const std::string& album) const;
    [[nodiscard]] std::vector<TrackPtr> find_by_genre(const std::string& genre) const;
    [[nodiscard]] std::vector<TrackPtr> search(const std::string& query) const;
    [[nodiscard]] std::vector<TrackPtr> search_advanced(const std::string& query, 
                                                        const TrackFilter& filter = nullptr) const;
    
    // Listing operations
    [[nodiscard]] std::vector<TrackPtr> get_all_tracks() const;
    [[nodiscard]] std::vector<TrackPtr> get_tracks_sorted(const TrackComparator& comparator) const;
    [[nodiscard]] std::vector<std::string> get_all_artists() const;
    [[nodiscard]] std::vector<std::string> get_all_albums() const;
    [[nodiscard]] std::vector<std::string> get_all_genres() const;
    
    // Filter operations
    [[nodiscard]] std::vector<TrackPtr> filter_tracks(const TrackFilter& filter) const;
    [[nodiscard]] std::vector<TrackPtr> get_tracks_by_year_range(int start_year, int end_year) const;
    [[nodiscard]] std::vector<TrackPtr> get_tracks_by_rating_range(double min_rating, double max_rating) const;
    [[nodiscard]] std::vector<TrackPtr> get_most_played_tracks(size_t limit = 10) const;
    [[nodiscard]] std::vector<TrackPtr> get_top_rated_tracks(size_t limit = 10) const;
    
    // Statistics
    [[nodiscard]] size_t size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] Statistics get_statistics() const;
    
    // Import/Export. Imports return false on open/read/parse/validation failure;
    // input validation completes before any tracks are added. Duplicates are ignored.
    bool import_from_csv(const std::string& filename);
    bool export_to_csv(const std::string& filename) const;
    bool import_from_json(const std::string& filename);
    bool export_to_json(const std::string& filename) const;
    
    // Thread-safe iteration
    void for_each_track(const std::function<void(const Track&)>& func) const;
    
private:
    // Internal data structures
    mutable std::shared_mutex mutex_;
    // Mutable pointer type for SearchEngine/FileIO compatibility only. Stored
    // objects are never mutated in place or exposed through mutable handles.
    std::vector<std::shared_ptr<Track>> tracks_;
    
    // Indexes for fast lookup
    std::unordered_multimap<std::string, size_t> title_index_;
    std::unordered_multimap<std::string, size_t> artist_index_;
    std::unordered_multimap<std::string, size_t> album_index_;
    std::unordered_multimap<std::string, size_t> genre_index_;
    
    // Search engine
    std::unique_ptr<SearchEngine> search_engine_;
    
    // Helper methods
    void rebuild_indexes();
    void add_to_indexes(size_t index, const Track& track);
    void remove_from_indexes(size_t index, const Track& track);
    [[nodiscard]] std::vector<TrackPtr> get_tracks_by_indexes(const std::vector<size_t>& indexes) const;
};

} // namespace audio_library
