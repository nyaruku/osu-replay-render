cmake_minimum_required(VERSION 3.11)
include(FetchContent)

project(osu-replay-render LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_BUILD_PARALLEL_LEVEL 16)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
    message(STATUS "No build type specified, defaulting to Release")
endif()

# Base directories
set(BASE_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/src/includes")
set(BASE_LIB_PATH "${CMAKE_SOURCE_DIR}/src/libs")

option(USE_STATIC_LIB "Use static library" ON)

# Debug/Release-specific paths
#[===[
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    #set(BUILD_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/cmake-template-project/includes/Debug/")
    #set(BUILD_LIB_PATH "${CMAKE_SOURCE_DIR}/src/libs/LIB_Debug/")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    #set(BUILD_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/cmake-template-project/includes/Release/")
    #set(BUILD_LIB_PATH "${CMAKE_SOURCE_DIR}/src/libs/LIB_Release/")
endif()
]===]

# Add the include directories for the project
include_directories(${BASE_INCLUDE_PATH})

# Add the library directories for linking
link_directories(${BASE_LIB_PATH} ${BUILD_LIB_PATH})

# Project-specific CMakeLists.txt
add_executable(${PROJECT_NAME} "./src/main.cpp")

FetchContent_Declare(
    raylib
    GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
    GIT_TAG "5.5"
    GIT_PROGRESS TRUE
)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)

# just in case
#FetchContent_Declare(cpr
#    GIT_REPOSITORY https://github.com/libcpr/cpr.git
#    GIT_TAG "1.14.2"
#)

#FetchContent_MakeAvailable(cpr)
FetchContent_Declare(
    SQLiteCpp
    GIT_REPOSITORY "https://github.com/SRombauts/SQLiteCpp.git"
    GIT_TAG "3.3.2"
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(raylib SQLiteCpp)

find_package(LibLZMA REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(${PROJECT_NAME} LibLZMA::LibLZMA raylib sqlite3 OpenSSL::Crypto)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)