# Usage Guide

## Windows (PowerShell/Command Prompt)

### Building the Project

```powershell
# Using Make (if you have Make installed via Cygwin/MinGW)
make clean
make -j4

# Or build individual components
make music_manager
make tests
```

### Running the Application

```powershell
# Run with default sample data
.\music_manager.exe data\sample_tracks.csv

# Or just run without arguments (will start with empty library)
.\music_manager.exe

# Run the demo
.\demo.bat
```

### Running Tests

```powershell
# Run all tests
.\audio_library_tests.exe

# Run specific test suite
.\audio_library_tests.exe "[track]"
.\audio_library_tests.exe "[music_library]"
.\audio_library_tests.exe "[search_engine]"
```

## Linux/WSL

### Building the Project

```bash
# Clean and build
make clean
make -j$(nproc)

# Or use specific number of threads
make -j4
```

### Running the Application

```bash
# Run with sample data
./music_manager data/sample_tracks.csv

# Run the demo script
bash demo.sh
```

## Application Commands

Once the application is running, you can use these menu options:

1. **Search tracks** - Search by title, artist, album, or genre
2. **List all tracks** - View all tracks with sorting options
3. **Add track manually** - Add a new track interactively
4. **Remove track** - Remove a specific track by title and artist
5. **Import tracks from file** - Load tracks from CSV or JSON
6. **Export tracks to file** - Save library to CSV or JSON
7. **Browse by category** - Browse by artist, album, genre, or year
8. **Top tracks** - View most played or highest rated tracks
9. **Advanced search** - Use regex, fuzzy matching, etc.
0. **Exit** - Close the application

## Sample Data Format

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

## Quick Start Examples

### Search for Queen tracks
1. Press `1` for Search
2. Type `Queen`
3. View results

### Export your library
1. Press `6` for Export
2. Choose format (1 for CSV, 2 for JSON)
3. Enter filename (e.g., `my_library.csv`)

### Browse by artist
1. Press `7` for Browse
2. Press `1` for Artist
3. Select artist number to view their tracks

## Updating tracks through the C++ API

Query results are immutable `std::shared_ptr<const Track>` snapshots. Edit a copy
and submit it with the handle you read:

```cpp
auto matches = library.find_by_title("Imagine");
if (!matches.empty()) {
    auto expected = matches.front();
    Track edited = *expected;
    edited.set_album("Updated Album");
    edited.set_genre("Rock");
    edited.increment_play_count();
    if (!library.update_track(expected, edited)) {
        // Requery and resolve a concurrent edit or a duplicate title/artist pair.
    }
}
```

The update rebuilds lookup and search indexes under the library lock. The old
handle keeps its original data. A stale, removed, null, or foreign handle is
rejected. Query again for the new version before another update.

Iteration, filter, comparator, and advanced-search callbacks run on a snapshot
without holding the library lock. They may call `size()`, `search()`, `add_track`,
`update_track`, or removal methods; changes apply to subsequent queries and do
not change the current snapshot. Caller-owned mutable tracks require caller
synchronization. See the README for source compatibility and GUI migration notes.

## Troubleshooting

### "make: command not found"
- Install Make via Cygwin, MinGW, or use WSL
- Alternatively, compile manually:
  ```powershell
  g++ -std=c++17 -Iinclude src/audio_library/*.cpp src/main.cpp -o music_manager.exe
  ```

### Line ending issues on Windows
- The project uses Unix line endings (LF)
- If you get errors about `\r`, use `dos2unix` or open in a text editor and save with Unix line endings

### Missing sample data
- Ensure you're in the project root directory
- Check that `data/sample_tracks.csv` exists
- Use relative path: `data\sample_tracks.csv` on Windows
