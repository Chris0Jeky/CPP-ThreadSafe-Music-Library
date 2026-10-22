# Testing configuration
include(CTest)

# Test executable
add_executable(audio_library_tests
    test_main.cpp
    test_track.cpp
    test_music_library.cpp
    test_thread_safety.cpp
    test_search_engine.cpp
    test_file_io.cpp
    test_import_regressions.cpp
)

target_link_libraries(audio_library_tests
    PRIVATE audio_library
)

# Add tests
add_test(NAME audio_library_tests COMMAND audio_library_tests)

# Keep the allocation override out of the regular (including concurrent) tests.
add_executable(music_library_exception_tests test_music_library_exceptions.cpp)
target_link_libraries(music_library_exception_tests PRIVATE audio_library)
add_test(NAME music_library_exception_tests COMMAND music_library_exception_tests)

# Python's standard-library JSON parser supplies an independent interoperability oracle.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_executable(json_interop_driver json_interop_driver.cpp)
    target_link_libraries(json_interop_driver PRIVATE audio_library)
    add_test(NAME json_interop
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/json_interop.py"
                $<TARGET_FILE:json_interop_driver>)
endif()

# Enable verbose output for tests
set(CMAKE_CTEST_ARGUMENTS "--verbose")
