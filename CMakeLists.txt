cmake_minimum_required(VERSION 3.20)
project(NewStarSoccerRebooted C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED CONFIG)
find_package(ZLIB REQUIRED)

# The code that has no window: the archives, the import, and later the rules.
add_library(nsr_core STATIC
    src/core/nsr_zip.c
    src/core/nsr_import.c
)
target_include_directories(nsr_core PUBLIC src/core)
target_link_libraries(nsr_core PUBLIC ZLIB::ZLIB)
target_compile_options(nsr_core PRIVATE -Wall -Wextra)

# The app.
add_executable(nsr src/app/main.c)
target_link_libraries(nsr PRIVATE nsr_core SDL3::SDL3)
target_compile_options(nsr PRIVATE -Wall -Wextra)

enable_testing()

# The helpers that each test uses: the checks and a writer for zip archives.
add_library(nsr_testkit STATIC tests/nsr_zipwriter.c)
target_include_directories(nsr_testkit PUBLIC tests)
target_link_libraries(nsr_testkit PUBLIC nsr_core)

function(nsr_add_test name)
    add_executable(${name} tests/${name}.c)
    target_link_libraries(${name} PRIVATE nsr_testkit)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

nsr_add_test(test_zip)
nsr_add_test(test_import)
