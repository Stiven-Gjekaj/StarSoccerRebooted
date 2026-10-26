cmake_minimum_required(VERSION 3.20)
project(StarSoccerRebooted C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED CONFIG)
find_package(ZLIB REQUIRED)

# The code that has no window: the archives, the import, and later the rules.
add_library(ssr_core STATIC
    src/core/ssr_zip.c
    src/core/ssr_import.c
)
target_include_directories(ssr_core PUBLIC src/core)
target_link_libraries(ssr_core PUBLIC ZLIB::ZLIB)
target_compile_options(ssr_core PRIVATE -Wall -Wextra)

# The app.
add_executable(ssr src/app/main.c)
target_link_libraries(ssr PRIVATE ssr_core SDL3::SDL3)
target_compile_options(ssr PRIVATE -Wall -Wextra)

enable_testing()

# The helpers that each test uses: the checks and a writer for zip archives.
add_library(ssr_testkit STATIC tests/ssr_zipwriter.c)
target_include_directories(ssr_testkit PUBLIC tests)
target_link_libraries(ssr_testkit PUBLIC ssr_core)

function(ssr_add_test name)
    add_executable(${name} tests/${name}.c)
    target_link_libraries(${name} PRIVATE ssr_testkit)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

ssr_add_test(test_zip)
ssr_add_test(test_import)
