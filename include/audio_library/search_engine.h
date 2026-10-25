#pragma once

#include "track.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <set>

namespace audio_library {

class SearchEngine {
public:
    using TrackPtr = std::shared_ptr<Track>;
    using SearchResult = std::pair<TrackPtr, double>; // Track and relevance score
    
    enum class SearchMode {
        EXACT,          // Exact match
        PREFIX,         // Prefix match
        SUBSTRING,      // Substring match
        FUZZY,          // Fuzzy matching with edit distance
        REGEX           // Regular expression matching
    };
    
    struct SearchOptions {
        SearchMode mode = SearchMode::SUBSTRING;
        // Modes match individual tokens. FUZZY counts case differences as edits.
        bool case_sensitive = false;
        bool search_title = true;
        bool search_artist = true;
        bool search_album = true;
        bool search_genre = true;
        size_t max_results = 100;
        double min_relevance = 0.0;
    };
    
    SearchEngine();
    ~SearchEngine();
    
    // Index management
    void index_track(size_t id, const Track& track);
    void remove_track(size_t id);
    void clear_index();
    void rebuild_index(const std::vector<TrackPtr>& tracks);
    void set_tracks(const std::vector<TrackPtr>* tracks) { tracks_ = tracks; }
    
    // Search operations
    [[nodiscard]] std::vector<SearchResult> search(const std::string& query, 
                                                   const SearchOptions& options) const;
    [[nodiscard]] std::vector<SearchResult> search(const std::string& query) const {
        return search(query, SearchOptions{});
    }
    
    // Autocomplete/Suggestions
    [[nodiscard]] std::vector<std::string> get_suggestions(const std::string& prefix, 
                                                           size_t max_suggestions = 10) const;
    
    // Statistics
    [[nodiscard]] size_t get_indexed_terms_count() const;
    [[nodiscard]] size_t get_indexed_tracks_count() const;
    
private:
    struct IndexEntry {
        size_t track_id;
        std::string field; // "title", "artist", "album", "genre"
        double weight;     // Importance weight for ranking
    };
    
    // Keep original tokens as well as normalized postings for case-sensitive search.
    using PostingIndex = std::unordered_map<std::string, std::vector<IndexEntry>>;
    PostingIndex inverted_index_;
    PostingIndex case_sensitive_index_;
    
    // Trie for autocomplete
    struct TrieNode {
        std::unordered_map<char, std::unique_ptr<TrieNode>> children;
        std::set<std::string> suggestions;
        bool is_end_of_word = false;
    };
    std::unique_ptr<TrieNode> trie_root_;
    
    // Reference to tracks for search results
    const std::vector<TrackPtr>* tracks_ = nullptr;
    
    // Helper methods
    std::vector<std::string> tokenize(const std::string& text) const;
    std::string normalize(const std::string& text, bool case_sensitive) const;
    double calculate_relevance(const std::string& query, const std::string& text) const;
    size_t levenshtein_distance(const std::string& s1, const std::string& s2) const;
    
    void add_to_trie(const std::string& word);
    void remove_from_trie(const std::string& word);
    void add_to_inverted_index(const std::string& term, size_t track_id, 
                              const std::string& field, double weight);
    
    // Search mode implementations
    void search_exact(const std::string& query, const SearchOptions& options,
                     std::unordered_map<size_t, double>& track_scores) const;
    void search_prefix(const std::string& query, const SearchOptions& options,
                      std::unordered_map<size_t, double>& track_scores) const;
    void search_substring(const std::string& query, const SearchOptions& options,
                         std::unordered_map<size_t, double>& track_scores) const;
    void search_fuzzy(const std::string& query, const SearchOptions& options,
                     std::unordered_map<size_t, double>& track_scores) const;
    void search_regex(const std::string& pattern, const SearchOptions& options,
                     std::unordered_map<size_t, double>& track_scores) const;
    
    bool should_search_field(const std::string& field, const SearchOptions& options) const;
};

} // namespace audio_library
