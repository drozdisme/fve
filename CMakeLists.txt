cmake_minimum_required(VERSION 3.10)
project(fve CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_compile_options(-O2 -Wall -Wextra -Wno-unknown-pragmas -frounding-math -ffp-contract=off)
include_directories(${CMAKE_SOURCE_DIR})

file(GLOB CORE_SRC
    core/*.cpp core/*/*.cpp
    extractor/*.cpp extractor/*/*.cpp
    oracle/*.cpp
    model/*/*.cpp
    semantic/*.cpp
    wrapper/*.cpp
    registry/*.cpp
    audit/*.cpp
    db/*.cpp
    platform/*.cpp
    api/*.cpp
    l0/*.cpp
    casebase/*.cpp)
add_library(core STATIC ${CORE_SRC})

add_executable(verify cmd/verify.cpp)
target_link_libraries(verify core)

add_executable(extract cmd/extract.cpp)
target_link_libraries(extract core)

add_executable(server cmd/server.cpp)
target_link_libraries(server core Threads::Threads)

add_executable(crawl cmd/crawl.cpp)
target_link_libraries(crawl core)

file(GLOB TEST_SRC tests/*.cpp)
add_executable(run_tests ${TEST_SRC})
target_link_libraries(run_tests core Threads::Threads)

enable_testing()
add_test(NAME unit COMMAND run_tests)
