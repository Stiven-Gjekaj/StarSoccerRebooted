cmake_minimum_required(VERSION 3.20)
project(StarSoccerRebooted VERSION 0.1.0 LANGUAGES C)

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
)
target_include_directories(ssr_core PUBLIC src/core)
target_include_directories(ssr_core PRIVATE src/third_party)
target_link_libraries(ssr_core PUBLIC ZLIB::ZLIB)
target_compile_options(ssr_core PRIVATE -Wall -Wextra)

# The app. On macOS it is an app bundle.
add_executable(ssr MACOSX_BUNDLE src/app/main.c)
target_link_libraries(ssr PRIVATE ssr_core ${SSR_SDL})
target_compile_options(ssr PRIVATE -Wall -Wextra)
target_compile_definitions(ssr PRIVATE SSR_VERSION="${PROJECT_VERSION}")
set_target_properties(ssr PROPERTIES
    OUTPUT_NAME "Star Soccer Rebooted"
    MACOSX_BUNDLE_BUNDLE_NAME "Star Soccer Rebooted"
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.stivengjekaj.starsoccerrebooted
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

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
