#include "audio_library/file_io.h"
#include <exception>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        audio_library::FileIO::ImportOptions options;
        options.skip_invalid_lines = false;
        const auto tracks = audio_library::FileIO::import_json(argv[1], options);
        return audio_library::FileIO::export_json(argv[2], tracks) ? 0 : 1;
    } catch (const std::exception&) {
        return 1;
    }
}
