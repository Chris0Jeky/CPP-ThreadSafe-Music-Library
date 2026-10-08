# Audio Library Management System

A modern C++17 music library management system with advanced search capabilities, thread-safe operations, and comprehensive metadata support.

## Features

### Core Functionality
- 🎵 **Track Management**: Add, remove, and organize music tracks with rich metadata
- 🔍 **Advanced Search**: Multiple search modes including exact, prefix, substring, fuzzy, and regex matching
- 📊 **Statistics & Analytics**: Track play counts, ratings, and library statistics
- 💾 **Multiple File Formats**: Import/export support for CSV and JSON
- 🧵 **Thread-Safe Operations**: Concurrent read/write support with shared mutexes
- ⚡ **High Performance**: Optimized indexing with O(1) average search complexity

### Search Capabilities
- **Multi-field Search**: Search across title, artist, album, and genre
- **Search Modes**:
  - Exact matching
  - Prefix matching
  - Substring matching
  - Fuzzy matching with edit distance
  - Regular expression support
- **Autocomplete**: Suggestion system for search queries
- **Case-sensitive/insensitive** search options

### Track Metadata
- Basic: Title, Artist, Duration
- Extended: Album, Genre, Year, Play Count, Rating
- Custom formatting for duration display
- CSV and JSON serialization

## Building the Project

### Requirements
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.14 or higher
- Threading support (pthread on Unix-like systems)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/SWECW2-AudioLibrary.git
cd SWECW2-AudioLibrary

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

# Run tests
make test

# Install (optional)
sudo make install
```

### Build Options
```bash
# Disable tests
cmake -DBUILD_TESTS=OFF ..

# Enable benchmarks
cmake -DBUILD_BENCHMARKS=ON ..

# Release build with optimizations
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## Usage

### Command Line Application

```bash
# Run with default sample data
./music_manager

# Run with custom data file
./music_manager path/to/tracks.csv

# Run with path and filename
./music_manager /data/music tracks.csv
```

### Library Integration

```cpp
#include "audio_library/music_library.h"

using namespace audio_library;

// Create a library
MusicLibrary library;

// Add tracks
Track track("Bohemian Rhapsody", "Queen", 354);
track.set_album("A Night at the Opera");
track.set_year(1975);
track.set_rating(5.0);

library.add_track(track);

// Search
auto results = library.search("Queen");

// Advanced search
SearchEngine::SearchOptions options;
options.mode = SearchEngine::SearchMode::FUZZY;
auto fuzzy_results = library.search_advanced("Queeen", nullptr);

// Get statistics
auto stats = library.get_statistics();
std::cout << "Total tracks: " << stats.total_tracks << "\n";
```

## Project Structure

```
SWECW2-AudioLibrary/
├── include/audio_library/     # Public headers
│   ├── track.h               # Track class definition
│   ├── music_library.h       # Main library interface
│   ├── search_engine.h       # Advanced search functionality
│   └── file_io.h            # Import/export operations
├── src/
│   ├── audio_library/        # Implementation files
│   │   ├── track.cpp
│   │   ├── music_library.cpp
│   │   ├── search_engine.cpp
│   │   └── file_io.cpp
│   └── main.cpp              # CLI application
├── tests/                    # Unit tests
├── examples/                 # Example programs
├── benchmarks/              # Performance benchmarks
├── data/                    # Sample data files
└── CMakeLists.txt          # Build configuration
```

## Data Formats

### CSV Format
```csv
Title,Artist,Duration,Album,Genre,Year,PlayCount,Rating
Bohemian Rhapsody,Queen,354,A Night at the Opera,Rock,1975,1543,5.0
```

### JSON Format
```json
[
  {
    "title": "Bohemian Rhapsody",
    "artist": "Queen",
    "duration": 354,
    "album": "A Night at the Opera",
    "genre": "Rock",
    "year": 1975,
    "play_count": 1543,
    "rating": 5.0
  }
]
```

## API Documentation

### Track Class
```cpp
Track(title, artist, duration_seconds)
set_album(album)
set_genre(genre)
set_year(year)
set_rating(rating)  // 0.0 to 5.0
increment_play_count()
format_duration()   // Returns "MM:SS" or "H:MM:SS"
to_string()        // Human-readable format
to_csv()           // CSV format
to_json()          // JSON format
```

### MusicLibrary Class
```cpp
// Track management
add_track(track)
remove_track(title, artist)
remove_tracks_by_title(title)
remove_tracks_by_artist(artist)
clear()

// Search operations
find_by_title(title)
find_by_artist(artist)
find_by_album(album)
find_by_genre(genre)
search(query)
search_advanced(query, filter)

// Listing and filtering
get_all_tracks()
get_tracks_sorted(comparator)
filter_tracks(filter)
get_tracks_by_year_range(start, end)
get_tracks_by_rating_range(min, max)
get_most_played_tracks(limit)
get_top_rated_tracks(limit)

// Import/Export
import_from_csv(filename)
export_to_csv(filename)
import_from_json(filename)
export_to_json(filename)

// Statistics
get_statistics()
```

## Performance

- **O(1)** average case for exact searches
- **O(n)** for fuzzy and regex searches
- Thread-safe concurrent reads
- Efficient memory usage with shared pointers
- Optimized string operations with string_view where applicable

## Testing

The project includes comprehensive unit tests using Catch2:

```bash
# Run all tests
./tests/audio_library_tests

# Run specific test suite
./tests/audio_library_tests "[track]"
./tests/audio_library_tests "[search_engine]"
```

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

The owner-authored project code is licensed under GNU GPL version 3 only
(`GPL-3.0-only`). See `LICENSE` and `RELICENSING.md`. The bundled Catch2 header
`include/catch.hpp` remains under the Boost Software License 1.0; see
`THIRD_PARTY_NOTICES.md` and `LICENSES/BSL-1.0.txt`.

## Acknowledgments

- Original project concept from CST2550 Coursework 2
- Catch2 testing framework
- Modern C++ design patterns and best practices

## Future Enhancements

- [ ] SQLite database backend
- [ ] REST API server
- [ ] Audio file metadata extraction
- [ ] Playlist management
- [ ] Music recommendation engine
- [ ] GUI application
- [ ] Cloud synchronization
