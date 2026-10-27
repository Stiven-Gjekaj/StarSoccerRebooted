cmake_minimum_required(VERSION 3.20)
project(StarSoccerRebooted VERSION 0.6.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SDL3 comes from its source by default, so that the app holds it. A build
# for iOS or Android needs this. SSR_SYSTEM_SDL uses the SDL3 of the system.
option(SSR_SYSTEM_SDL "Use the SDL3 of the system" OFF)
if(SSR_SYSTEM_SDL)
    find_package(SDL3 REQUIRED CONFIG)
    set(SSR_SDL SDL3::SDL3)
else()
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
        URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    FetchContent_MakeAvailable(SDL3)
    set(SSR_SDL SDL3::SDL3-static)
endif()
find_package(ZLIB REQUIRED)

# The code that has no window: the archives, the import, the images, and later the rules.
add_library(ssr_core STATIC
    src/core/ssr_zip.c
    src/core/ssr_import.c
    src/core/ssr_image.c
    src/core/ssr_tsv.c
    src/core/ssr_world.c
    src/core/ssr_text.c
    src/core/ssr_font.c
    src/core/ssr_diff.c
    src/core/ssr_fixdata.c
    src/core/ssr_season.c
    src/core/ssr_expr.c
    src/core/ssr_ui.c
    src/core/ssr_atlas.c
    src/core/ssr_tweaks.c
    src/core/ssr_scene.c
    src/core/ssr_sound.c
    src/core/ssr_fs.c
    src/core/ssr_spine.c
    src/core/ssr_random.c
    src/core/ssr_game.c
    src/core/ssr_career.c
    src/core/ssr_ticker.c
    src/core/ssr_ball.c
    src/core/ssr_ex.c
    src/core/ssr_tapball.c
    src/core/ssr_players.c
    src/core/ssr_chance.c
    src/core/ssr_match.c
    src/core/ssr_aftermatch.c
    src/core/ssr_training.c
)
target_include_directories(ssr_core PUBLIC src/core)
target_include_directories(ssr_core PRIVATE src/third_party)
target_link_libraries(ssr_core PUBLIC ZLIB::ZLIB)
target_compile_options(ssr_core PRIVATE -Wall -Wextra)

# The app. On macOS it is an app bundle. On Android it is libmain.so, which
# the activity of SDL loads.
set(SSR_APP_SOURCES src/app/main.c src/app/ssr_gfx.c src/app/ssr_audio.c)
if(ANDROID)
    # SDL has C++ code on Android, so the library links the C++ runtime.
    enable_language(CXX)
    add_library(ssr SHARED ${SSR_APP_SOURCES})
    set_target_properties(ssr PROPERTIES LINKER_LANGUAGE CXX)
else()
    add_executable(ssr MACOSX_BUNDLE ${SSR_APP_SOURCES})
endif()
target_link_libraries(ssr PRIVATE ssr_core ${SSR_SDL})
target_compile_options(ssr PRIVATE -Wall -Wextra)
target_compile_definitions(ssr PRIVATE SSR_VERSION="${PROJECT_VERSION}")
if(IOS)
    # The player copies the APK, XAPK, or IPA into the Documents folder of
    # the app with Finder or the Files app.
    set_target_properties(ssr PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/ios/Info.plist.in
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    )
endif()
# The layouts of the remake go into the app as resources.
file(GLOB SSR_LAYOUTS ${CMAKE_SOURCE_DIR}/layouts/*.uis ${CMAKE_SOURCE_DIR}/layouts/*.uih)
if(SSR_LAYOUTS)
    target_sources(ssr PRIVATE ${SSR_LAYOUTS})
    set_source_files_properties(${SSR_LAYOUTS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/layouts)
endif()
set_target_properties(ssr PROPERTIES
    OUTPUT_NAME "Star Soccer Rebooted"
    MACOSX_BUNDLE_BUNDLE_NAME "Star Soccer Rebooted"
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.stivengjekaj.starsoccerrebooted
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)
if(ANDROID)
    set_target_properties(ssr PROPERTIES OUTPUT_NAME main)
endif()

# The tests run on the computer that builds the app.
if(IOS OR ANDROID)
    return()
endif()

# The tool that compares a screen of the remake with the original game.
add_executable(screendiff tools/screendiff.c tests/ssr_imagewriter.c)
target_include_directories(screendiff PRIVATE src/third_party)
target_link_libraries(screendiff PRIVATE ssr_core)

enable_testing()

# The helpers that each test uses: the checks and a writer for zip archives.
add_library(ssr_testkit STATIC tests/ssr_zipwriter.c tests/ssr_imagewriter.c)
target_include_directories(ssr_testkit PUBLIC tests src/third_party)
target_link_libraries(ssr_testkit PUBLIC ssr_core)

function(ssr_add_test name)
    add_executable(${name} tests/${name}.c)
    target_link_libraries(${name} PRIVATE ssr_testkit)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

ssr_add_test(test_zip)
ssr_add_test(test_import)
ssr_add_test(test_image)
ssr_add_test(test_tsv)
ssr_add_test(test_world)
ssr_add_test(test_text)
ssr_add_test(test_font)
ssr_add_test(test_diff)
ssr_add_test(test_season)
ssr_add_test(test_expr)
ssr_add_test(test_ui)
ssr_add_test(test_atlas)
ssr_add_test(test_tweaks)
ssr_add_test(test_scene)
ssr_add_test(test_sound)
ssr_add_test(test_fs)
ssr_add_test(test_spine)
ssr_add_test(test_random)
ssr_add_test(test_game)
ssr_add_test(test_career)
ssr_add_test(test_ticker)
ssr_add_test(test_ball)
ssr_add_test(test_ex)
ssr_add_test(test_tapball)
ssr_add_test(test_players)
ssr_add_test(test_chance)
ssr_add_test(test_match)
ssr_add_test(test_aftermatch)
ssr_add_test(test_training)
# The font test skips its layout checks when the system has no TTF font.
set_tests_properties(test_font PROPERTIES SKIP_RETURN_CODE 77)
