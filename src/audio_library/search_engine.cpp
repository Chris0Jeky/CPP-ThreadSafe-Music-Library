#include "audio_library/search_engine.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <regex>
#include <queue>
#include <unordered_set>

namespace audio_library {

SearchEngine::SearchEngine() 
    : trie_root_(std::make_unique<TrieNode>()) {
}

SearchEngine::~SearchEngine() = default;

void SearchEngine::index_track(size_t id, const Track& track) {
    // Tokenize and index each field
    auto index_field = [this, id](const std::string& text, const std::string& field, double weight) {
        auto tokens = tokenize(text);
        for (const auto& token : tokens) {
            add_to_inverted_index(token, id, field, weight);
            add_to_trie(token);
        }
    };
    
    index_field(track.title(), "title", 1.0);
    index_field(track.artist(), "artist", 0.8);
    if (!track.album().empty()) {
        index_field(track.album(), "album", 0.6);
    }
    if (!track.genre().empty()) {
        index_field(track.genre(), "genre", 0.4);
    }
}

void SearchEngine::remove_track(size_t id) {
    // Remove from inverted index
    for (auto& [term, entries] : inverted_index_) {
        entries.erase(std::remove_if(entries.begin(), entries.end(),
            [id](const IndexEntry& entry) { return entry.track_id == id; }),
            entries.end());
    }
    
    // Clean up empty entries
    for (auto it = inverted_index_.begin(); it != inverted_index_.end(); ) {
        if (it->second.empty()) {
            it = inverted_index_.erase(it);
        } else {
            ++it;
        }
    }
}

void SearchEngine::clear_index() {
    inverted_index_.clear();
    trie_root_ = std::make_unique<TrieNode>();
}

void SearchEngine::rebuild_index(const std::vector<TrackPtr>& tracks) {
    clear_index();
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i]) {
            index_track(i, *tracks[i]);
        }
    }
}

std::vector<SearchEngine::SearchResult> SearchEngine::search(
    const std::string& query, const SearchOptions& options) const {
    
    if (query.empty()) {
        return {};
    }
    
    // Normalize query
    std::string normalized_query = normalize(query, options.case_sensitive);
    
    // Different search strategies based on mode
    std::unordered_map<size_t, double> track_scores;
    
    switch (options.mode) {
        case SearchMode::EXACT:
            search_exact(normalized_query, options, track_scores);
            break;
        case SearchMode::PREFIX:
            search_prefix(normalized_query, options, track_scores);
            break;
        case SearchMode::SUBSTRING:
            search_substring(normalized_query, options, track_scores);
            break;
        case SearchMode::FUZZY:
            search_fuzzy(normalized_query, options, track_scores);
            break;
        case SearchMode::REGEX:
            search_regex(query, options, track_scores); // Use original query for regex
            break;
    }
    
    // Convert to result vector and sort by score
    std::vector<SearchResult> results;
    for (const auto& [track_id, score] : track_scores) {
        if (score >= options.min_relevance && tracks_ && track_id < tracks_->size()) {
            results.emplace_back((*tracks_)[track_id], score);
        }
    }
    
    // Sort by relevance score
    std::sort(results.begin(), results.end(),
        [](const SearchResult& a, const SearchResult& b) {
            return a.second > b.second;
        });
    
    // Limit results
    if (results.size() > options.max_results) {
        results.resize(options.max_results);
    }
    
    return results;
}

std::vector<std::string> SearchEngine::get_suggestions(const std::string& prefix, size_t max_suggestions) const {
    if (prefix.empty() || !trie_root_) {
        return {};
    }
    
    // Navigate to prefix node
    std::string lower_prefix = normalize(prefix, false);
    TrieNode* current = trie_root_.get();
    
    for (char c : lower_prefix) {
        auto it = current->children.find(c);
        if (it == current->children.end()) {
            return {}; // Prefix not found
        }
        current = it->second.get();
    }
    
    // Collect suggestions from this node
    std::vector<std::string> suggestions;
    std::queue<std::pair<TrieNode*, std::string>> queue;
    queue.push({current, lower_prefix});
    
    while (!queue.empty() && suggestions.size() < max_suggestions) {
        auto [node, word] = queue.front();
        queue.pop();
        
        if (node->is_end_of_word) {
            suggestions.push_back(word);
        }
        
        // Add children to queue
        for (const auto& [ch, child] : node->children) {
            queue.push({child.get(), word + ch});
        }
    }
    
    return suggestions;
}

size_t SearchEngine::get_indexed_terms_count() const {
    return inverted_index_.size();
}

size_t SearchEngine::get_indexed_tracks_count() const {
    std::unordered_set<size_t> unique_tracks;
    for (const auto& [term, entries] : inverted_index_) {
        for (const auto& entry : entries) {
            unique_tracks.insert(entry.track_id);
        }
    }
    return unique_tracks.size();
}

std::vector<std::string> SearchEngine::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::stringstream ss(text);
    std::string token;
    
    while (ss >> token) {
        // Remove punctuation from beginning and end
        while (!token.empty() && std::ispunct(token.front())) {
            token.erase(0, 1);
        }
        while (!token.empty() && std::ispunct(token.back())) {
            token.pop_back();
        }
        
        if (!token.empty()) {
            tokens.push_back(normalize(token, false));
        }
    }
    
    return tokens;
}

std::string SearchEngine::normalize(const std::string& text, bool case_sensitive) const {
    if (case_sensitive) {
        return text;
    }
    
    std::string normalized = text;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}

double SearchEngine::calculate_relevance(const std::string& query, const std::string& text) const {
    if (text.empty() || query.empty()) {
        return 0.0;
    }
    
    std::string lower_query = normalize(query, false);
    std::string lower_text = normalize(text, false);
    
    // Exact match
    if (lower_text == lower_query) {
        return 1.0;
    }
    
    // Prefix match
    if (lower_text.find(lower_query) == 0) {
        return 0.8;
    }
    
    // Contains match
    if (lower_text.find(lower_query) != std::string::npos) {
        return 0.6;
    }
    
    // Fuzzy match using edit distance
    size_t distance = levenshtein_distance(lower_query, lower_text);
    size_t max_len = std::max(lower_query.length(), lower_text.length());
    
    if (max_len > 0) {
        double similarity = 1.0 - (static_cast<double>(distance) / max_len);
        if (similarity > 0.5) {
            return similarity * 0.4;
        }
    }
    
    return 0.0;
}

size_t SearchEngine::levenshtein_distance(const std::string& s1, const std::string& s2) const {
    const size_t m = s1.length();
    const size_t n = s2.length();
    
    if (m == 0) return n;
    if (n == 0) return m;
    
    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));
    
    for (size_t i = 0; i <= m; ++i) {
        dp[i][0] = i;
    }
    for (size_t j = 0; j <= n; ++j) {
        dp[0][j] = j;
    }
    
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            size_t cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i-1][j] + 1,      // deletion
                dp[i][j-1] + 1,      // insertion
                dp[i-1][j-1] + cost  // substitution
            });
        }
    }
    
    return dp[m][n];
}

void SearchEngine::add_to_trie(const std::string& word) {
    if (word.empty()) return;
    
    TrieNode* current = trie_root_.get();
    
    for (char c : word) {
        if (current->children.find(c) == current->children.end()) {
            current->children[c] = std::make_unique<TrieNode>();
        }
        current = current->children[c].get();
    }
    
    current->is_end_of_word = true;
    current->suggestions.insert(word);
}

void SearchEngine::add_to_inverted_index(const std::string& term, size_t track_id,
                                       const std::string& field, double weight) {
    inverted_index_[term].push_back({track_id, field, weight});
}

// Private helper methods for different search modes
void SearchEngine::search_exact(const std::string& query, const SearchOptions& options,
                               std::unordered_map<size_t, double>& track_scores) const {
    auto it = inverted_index_.find(query);
    if (it != inverted_index_.end()) {
        for (const auto& entry : it->second) {
            if (should_search_field(entry.field, options)) {
                track_scores[entry.track_id] += entry.weight;
            }
        }
    }
}

void SearchEngine::search_prefix(const std::string& query, const SearchOptions& options,
                                std::unordered_map<size_t, double>& track_scores) const {
    for (const auto& [term, entries] : inverted_index_) {
        if (term.find(query) == 0) { // Prefix match
            for (const auto& entry : entries) {
                if (should_search_field(entry.field, options)) {
                    track_scores[entry.track_id] += entry.weight * 0.8;
                }
            }
        }
    }
}

void SearchEngine::search_substring(const std::string& query, const SearchOptions& options,
                                   std::unordered_map<size_t, double>& track_scores) const {
    for (const auto& [term, entries] : inverted_index_) {
        if (term.find(query) != std::string::npos) { // Substring match
            for (const auto& entry : entries) {
                if (should_search_field(entry.field, options)) {
                    double score = (term.find(query) == 0) ? 0.8 : 0.6;
                    track_scores[entry.track_id] += entry.weight * score;
                }
            }
        }
    }
}

void SearchEngine::search_fuzzy(const std::string& query, const SearchOptions& options,
                               std::unordered_map<size_t, double>& track_scores) const {
    for (const auto& [term, entries] : inverted_index_) {
        double relevance = calculate_relevance(query, term);
        if (relevance > 0) {
            for (const auto& entry : entries) {
                if (should_search_field(entry.field, options)) {
                    track_scores[entry.track_id] += entry.weight * relevance;
                }
            }
        }
    }
}

void SearchEngine::search_regex(const std::string& pattern, const SearchOptions& options,
                               std::unordered_map<size_t, double>& track_scores) const {
    try {
        std::regex regex_pattern(pattern, 
            options.case_sensitive ? std::regex::ECMAScript : std::regex::icase);
        
        for (const auto& [term, entries] : inverted_index_) {
            if (std::regex_search(term, regex_pattern)) {
                for (const auto& entry : entries) {
                    if (should_search_field(entry.field, options)) {
                        track_scores[entry.track_id] += entry.weight;
                    }
                }
            }
        }
    } catch (const std::regex_error&) {
        // Invalid regex pattern
    }
}

bool SearchEngine::should_search_field(const std::string& field, const SearchOptions& options) const {
    if (field == "title") return options.search_title;
    if (field == "artist") return options.search_artist;
    if (field == "album") return options.search_album;
    if (field == "genre") return options.search_genre;
    return false;
}

} // namespace audio_library
