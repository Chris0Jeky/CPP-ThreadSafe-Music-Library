#include "../include/catch.hpp"
#include "audio_library/file_io.h"
#include "audio_library/music_library.h"
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace audio_library;

namespace {
struct ImportFixture {
    std::filesystem::path path = "import-regression.tmp";
    ~ImportFixture() { std::filesystem::remove(path); }
    void write(const std::string& text) const {
        std::ofstream out(path, std::ios::binary);
        out << text;
    }
};
}

TEST_CASE("CSV imports accept one leading UTF-8 BOM", "[regression][csv][bom]") {
    ImportFixture fixture;
    const auto csv = GENERATE(std::string("Title,Artist,Duration\r\nSong,Artist,180\r\n"),
                              std::string("Song,Artist,180\n"),
                              std::string("\"Song, live\",Artist,180\n"),
                              std::string("\"Song\r\nlive\",Artist,180\r\n"));
    const auto prefix = GENERATE(std::string(), std::string("\xEF\xBB\xBF"));
    fixture.write(prefix + csv);
    FileIO::ImportOptions options;
    options.skip_invalid_lines = false;
    size_t errors = 0;
    options.error_callback = [&errors](const std::string&, size_t) { ++errors; };
    const auto tracks = FileIO::import_csv(fixture.path, options);
    REQUIRE(tracks.size() == 1);
    const std::string title = csv.find("live") == std::string::npos ? "Song"
        : (csv.find("Song,") != std::string::npos ? "Song, live" : "Song\r\nlive");
    REQUIRE(tracks[0].title() == title);
    REQUIRE(tracks[0].artist() == "Artist");
    REQUIRE(tracks[0].duration_seconds() == 180);
    REQUIRE(errors == 0);
    MusicLibrary library;
    REQUIRE(library.import_from_csv(fixture.path.string()));
    REQUIRE(library.find_by_title(title).size() == 1);
}

TEST_CASE("JSON imports accept one leading UTF-8 BOM", "[regression][json][bom]") {
    ImportFixture fixture;
    const auto prefix = GENERATE(std::string(), std::string("\xEF\xBB\xBF"));
    fixture.write(prefix + " \r\n" + R"([{"title":"Song","artist":"Artist","duration":180}])");
    FileIO::ImportOptions options;
    options.skip_invalid_lines = false;
    size_t errors = 0;
    options.error_callback = [&errors](const std::string&, size_t) { ++errors; };
    const auto tracks = FileIO::import_json(fixture.path, options);
    REQUIRE(tracks.size() == 1);
    REQUIRE(tracks[0].title() == "Song");
    REQUIRE(tracks[0].artist() == "Artist");
    REQUIRE(tracks[0].duration_seconds() == 180);
    REQUIRE(errors == 0);
    MusicLibrary library;
    REQUIRE(library.import_from_json(fixture.path.string()));
    REQUIRE(library.find_by_title("Song").size() == 1);
}

TEST_CASE("Format detection accepts a leading UTF-8 BOM", "[regression][file_io][bom]") {
    ImportFixture fixture;
    const bool json = GENERATE(false, true);
    const auto prefix = GENERATE(std::string(), std::string("\xEF\xBB\xBF"));
    fixture.write(prefix + (json ? R"([{"title":"Song","artist":"Artist","duration":180}])"
                                 : "Title,Artist,Duration\nSong,Artist,180\n"));
    REQUIRE((json ? FileIO::validate_json_format(fixture.path) : FileIO::validate_csv_format(fixture.path)));
    REQUIRE(FileIO::detect_file_format(fixture.path) == (json ? "json" : "csv"));
}

TEST_CASE("BOM bytes count toward import input limits", "[regression][file_io][bom]") {
    ImportFixture fixture;
    const bool json = GENERATE(false, true);
    const std::string content = json ? "[]" : "Title,Artist,Duration\n";
    const std::string input = "\xEF\xBB\xBF" + content;
    fixture.write(input);
    FileIO::ImportOptions options;
    const auto import = [&] {
        return json ? FileIO::import_json(fixture.path, options) : FileIO::import_csv(fixture.path, options);
    };
    options.max_input_bytes = input.size();
    REQUIRE(import().empty());
    options.max_input_bytes = input.size() - 1;
    REQUIRE_THROWS_AS(import(), std::runtime_error);
    options.max_input_bytes = 2;
    REQUIRE_THROWS_AS(import(), std::runtime_error);
}

TEST_CASE("BOM-only inputs follow empty import rules", "[regression][file_io][bom]") {
    ImportFixture fixture;
    const auto prefix = GENERATE(std::string(), std::string("\xEF\xBB\xBF"));
    fixture.write(prefix);
    FileIO::ImportOptions options;
    options.skip_invalid_lines = false;
    REQUIRE(FileIO::import_csv(fixture.path, options).empty());
    REQUIRE_THROWS_AS(FileIO::import_json(fixture.path, options), std::runtime_error);
    REQUIRE_FALSE(FileIO::validate_csv_format(fixture.path));
    REQUIRE_FALSE(FileIO::validate_json_format(fixture.path));
}

TEST_CASE("Imports preserve UTF-8 BOM bytes inside track metadata", "[regression][unicode][bom]") {
    ImportFixture fixture;
    const std::string bom = "\xEF\xBB\xBF";
    fixture.write(bom + "\"" + bom + "Song\",Artist,180\n");
    const auto csv = FileIO::import_csv(fixture.path);
    REQUIRE(csv.size() == 1);
    REQUIRE(csv[0].title() == bom + "Song");
    fixture.write(bom + "[{\"title\":\"" + bom + "Song\",\"artist\":\"Artist\",\"duration\":180}]");
    const auto json = FileIO::import_json(fixture.path);
    REQUIRE(json.size() == 1);
    REQUIRE(json[0].title() == bom + "Song");
}

TEST_CASE("JSON rejects misplaced repeated and incomplete BOMs", "[regression][json][bom]") {
    ImportFixture fixture;
    const auto prefix = GENERATE(std::string(" \xEF\xBB\xBF"),
                                 std::string("\xEF\xBB\xBF\xEF\xBB\xBF"),
                                 std::string("\xEF"), std::string("\xEF\xBB"));
    fixture.write(prefix + "[]");
    REQUIRE_THROWS_AS(FileIO::import_json(fixture.path), std::runtime_error);
}

TEST_CASE("CSV logical records preserve multiline metadata", "[regression][csv]") {
    ImportFixture fixture;
    Track track("Title\nwith, \"quotes\"", "Artist\r\nsecond line", 180);
    track.set_album("Album\rpart\nend");
    track.set_genre("Genre\nnext");
    std::vector<Track> tracks{track, Track("Next", "Artist", 200)};
    REQUIRE(FileIO::export_csv(fixture.path, tracks));
    auto imported = FileIO::import_csv(fixture.path);
    REQUIRE(imported.size() == 2);
    REQUIRE(imported[0].title() == track.title());
    REQUIRE(imported[0].artist() == track.artist());
    REQUIRE(imported[0].album() == track.album());
    REQUIRE(imported[0].genre() == track.genre());
    REQUIRE(imported[1].title() == "Next");

    fixture.write("Title,Artist,Duration\r\n\"A\r\nB\",\"C,\"\"D\"\"\",180\r\nNext,Artist,200\r\n");
    imported = FileIO::import_csv(fixture.path);
    REQUIRE(imported.size() == 2);
    REQUIRE(imported[0].title() == "A\r\nB");
    REQUIRE(imported[0].artist() == "C,\"D\"");
}

TEST_CASE("CSV strict imports fail closed", "[regression][csv]") {
    ImportFixture fixture;
    const auto bad = GENERATE(std::string("bad row"), std::string("Bad,Artist,12junk"),
                              std::string("\"unterminated,Artist,180"),
                              std::string("\"Bad\"junk,Artist,180"),
                              std::string("Bad,Artist,0"));
    fixture.write("Title,Artist,Duration\nGood,Artist,180\n" + bad + "\n");
    FileIO::ImportOptions options;
    options.skip_invalid_lines = false;
    REQUIRE_THROWS(FileIO::import_csv(fixture.path, options));
}

TEST_CASE("JSON UTF-8 and Unicode escapes round trip", "[regression][unicode]") {
    const auto title = GENERATE(std::string(u8"Caf\u00e9"), std::string(u8"\u0218tiin\u021b\u0103"),
                               std::string(u8"\u041c\u0443\u0437\u044b\u043a\u0430"),
                               std::string(u8"\u97f3\u697d"), std::string(u8"\U0001f3b5"));
    Track track(title, u8"Beyonc\u00e9", 180);
    track.set_album(title);
    track.set_genre(title);
    const auto json = track.to_json();
    REQUIRE(json.find(title) != std::string::npos);
    const auto parsed = Track::from_json(json);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->title() == title);
    REQUIRE(parsed->artist() == track.artist());
    REQUIRE(parsed->album() == title);
    REQUIRE(parsed->genre() == title);

    const auto escaped = Track::from_json(R"({"title":"Caf\u00e9 \uD83C\uDFB5","artist":"A\/B","duration":180})");
    REQUIRE(escaped.has_value());
    REQUIRE(escaped->title() == u8"Caf\u00e9 \U0001f3b5");
    REQUIRE(escaped->artist() == "A/B");
}

TEST_CASE("JSON rejects malformed strings and syntax", "[regression][json]") {
    const auto value = GENERATE(std::string(R"("\uD800")"), std::string(R"("\uDC00")"),
                               std::string(R"("\uD800\u0041")"), std::string(R"("\u12X4")"),
                               std::string(R"("\q")"), std::string("\"raw\nnewline\""),
                               std::string("\"\xc0\xaf\""), std::string("\"\xed\xa0\x80\""));
    REQUIRE_FALSE(Track::from_json("{\"title\":" + value + ",\"artist\":\"A\",\"duration\":180}"));
}

TEST_CASE("JSON objects ignore braces inside strings", "[regression][json]") {
    ImportFixture fixture;
    std::vector<Track> tracks;
    for (const auto& title : {"A } B", "{live}", "{", "quote \" } slash \\ {"}) {
        tracks.emplace_back(title, "Artist", 180);
        tracks.back().set_album("} { \" \\");
    }
    REQUIRE(FileIO::export_json(fixture.path, tracks));
    auto imported = FileIO::import_json(fixture.path);
    REQUIRE(imported.size() == tracks.size());
    for (size_t i = 0; i < tracks.size(); ++i) {
        REQUIRE(imported[i].title() == tracks[i].title());
        REQUIRE(imported[i].album() == tracks[i].album());
    }
}

TEST_CASE("JSON malformed documents never return a partial list", "[regression][json]") {
    ImportFixture fixture;
    const std::string object = R"({"title":"Good","artist":"Artist","duration":180})";
    const auto suffix = GENERATE(std::string(",]"), std::string(" {}]"), std::string(",{"),
                                std::string("]junk"), std::string(",null]"));
    fixture.write("[" + object + suffix);
    size_t errors = 0;
    FileIO::ImportOptions options;
    options.error_callback = [&errors](const std::string&, size_t) { ++errors; };
    REQUIRE_THROWS(FileIO::import_json(fixture.path, options));
    REQUIRE(errors == 1);
}

TEST_CASE("MusicLibrary imports report failures without adding a prefix", "[regression][library]") {
    ImportFixture fixture;
    MusicLibrary library;
    library.add_track(Track("Existing", "Artist", 100));
    REQUIRE_FALSE(library.import_from_csv("missing-import-regression.csv"));
    REQUIRE_FALSE(library.import_from_json("missing-import-regression.json"));
    REQUIRE_FALSE(library.import_from_csv("."));
    REQUIRE_FALSE(library.import_from_json("."));
    fixture.write("Title,Artist,Duration\nGood,Artist,180\nBad,Artist,nope\n");
    REQUIRE_FALSE(library.import_from_csv(fixture.path.string()));
    REQUIRE(library.size() == 1);
    fixture.write("not,a,header\n");
    REQUIRE_FALSE(library.import_from_csv(fixture.path.string()));
    fixture.write(R"([{"title":"Good","artist":"Artist","duration":180},])");
    REQUIRE_FALSE(library.import_from_json(fixture.path.string()));
    REQUIRE(library.size() == 1);
    fixture.write(R"([{"title":"Good","artist":"Artist","duration":180},{"title":"Bad","artist":"Artist","duration":0}])");
    REQUIRE_FALSE(library.import_from_json(fixture.path.string()));
    REQUIRE(library.size() == 1);
}

TEST_CASE("MusicLibrary accepts valid empty and two-track imports", "[regression][library]") {
    ImportFixture fixture;
    MusicLibrary library;
    fixture.write("");
    REQUIRE(library.import_from_csv(fixture.path.string()));
    fixture.write("Title,Artist,Duration\n");
    REQUIRE(library.import_from_csv(fixture.path.string()));
    fixture.write("");
    REQUIRE_FALSE(library.import_from_json(fixture.path.string()));
    fixture.write("[]");
    REQUIRE(library.import_from_json(fixture.path.string()));
    REQUIRE(library.empty());
    const std::vector<Track> tracks{Track("One", "A", 100), Track("Two", "B", 200)};
    REQUIRE(FileIO::export_csv(fixture.path, tracks));
    REQUIRE(library.import_from_csv(fixture.path.string()));
    REQUIRE(library.size() == 2);
    library.clear();
    REQUIRE(FileIO::export_json(fixture.path, tracks));
    REQUIRE(library.import_from_json(fixture.path.string()));
    REQUIRE(library.size() == 2);
    REQUIRE(library.import_from_json(fixture.path.string()));
    REQUIRE(library.size() == 2);
}

TEST_CASE("CSV bounds reject oversized logical records", "[regression][csv]") {
    ImportFixture fixture;
    SECTION("Byte bound") {
        fixture.write("\"" + std::string(1024 * 1024 + 1, 'x') + "\",Artist,180\n");
    }
    SECTION("Physical line bound") {
        fixture.write("\"" + std::string(10001, '\n') + "Title\",Artist,180\n");
    }
    REQUIRE_THROWS(FileIO::import_csv(fixture.path));
}

TEST_CASE("JSON input and object count are bounded", "[regression][json]") {
    ImportFixture fixture;
    const std::string object = R"({"title":"Good","artist":"Artist","duration":180})";
    SECTION("Byte bound") {
        fixture.write("[" + object + "]" + std::string(16 * 1024 * 1024, ' '));
    }
    SECTION("Object bound") {
        std::string input = "[" + object;
        for (size_t i = 0; i < 100000; ++i) input += "," + object;
        fixture.write(input + "]");
    }
    REQUIRE_THROWS(FileIO::import_json(fixture.path));
}

TEST_CASE("FileIO reports failed opens explicitly", "[regression][file_io]") {
    REQUIRE_THROWS_AS(FileIO::import_csv("missing-import-regression.csv"), std::runtime_error);
    REQUIRE_THROWS_AS(FileIO::import_json("missing-import-regression.json"), std::runtime_error);
}

#ifdef _WIN32
TEST_CASE("Imports fail when an existing file denies read sharing", "[regression][library]") {
    ImportFixture fixture;
    const bool json = GENERATE(false, true);
    fixture.write(json ? R"([{"title":"One","artist":"A","duration":180}])"
                       : "One,A,180\n");
    struct ReadLock {
        HANDLE handle;
        ~ReadLock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    } lock{CreateFileW(fixture.path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, nullptr)};
    REQUIRE(lock.handle != INVALID_HANDLE_VALUE);
    MusicLibrary library;
    if (json) {
        REQUIRE_THROWS(FileIO::import_json(fixture.path));
        REQUIRE_FALSE(library.import_from_json(fixture.path.string()));
    } else {
        REQUIRE_THROWS(FileIO::import_csv(fixture.path));
        REQUIRE_FALSE(library.import_from_csv(fixture.path.string()));
    }
    REQUIRE(library.empty());
}
#endif

TEST_CASE("JSON serialization rejects invalid UTF-8", "[regression][unicode]") {
    REQUIRE_THROWS(Track(std::string("\xc0\xaf"), "Artist", 180).to_json());
}

TEST_CASE("CSV does not discard a malformed first record as a header", "[regression][csv]") {
    ImportFixture fixture;
    fixture.write("Bad,Artist,nope\nGood,Artist,180\n");
    FileIO::ImportOptions options;
    options.skip_invalid_lines = false;
    REQUIRE_THROWS(FileIO::import_csv(fixture.path, options));
}

TEST_CASE("JSON export of invalid UTF-8 fails without writing a file", "[regression][unicode]") {
    ImportFixture fixture;
    std::vector<Track> tracks{Track("Caf\xE9", "Artist", 180)};
    REQUIRE_FALSE(FileIO::export_json(fixture.path, tracks));
    REQUIRE_FALSE(std::filesystem::exists(fixture.path));
}

TEST_CASE("Imports reject a directory path", "[regression][file_io]") {
    REQUIRE_THROWS(FileIO::import_csv("."));
    REQUIRE_THROWS(FileIO::import_json("."));
}
