cmake_minimum_required(VERSION 3.24)

project(Sprout LANGUAGES C CXX)

include(CTest)
include(FetchContent)

option(SPROUT_BUILD_DESKTOP "Build the SDL desktop launcher preview" ON)

FetchContent_Declare(
  SQLite3
  URL https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
  URL_HASH SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR cmake-not-provided
)
FetchContent_MakeAvailable(SQLite3)

add_library(sprout_sqlite STATIC "${sqlite3_SOURCE_DIR}/sqlite3.c")
target_include_directories(sprout_sqlite PUBLIC "${sqlite3_SOURCE_DIR}")
target_compile_definitions(
  sprout_sqlite
  PRIVATE
    SQLITE_DEFAULT_FOREIGN_KEYS=1
    SQLITE_DQS=0
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_THREADSAFE=1
)

FetchContent_Declare(
  yyjson
  GIT_REPOSITORY https://github.com/ibireme/yyjson.git
  GIT_TAG 7871d321ff4cd8068c1f777c97975dc2fb640ab3
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(yyjson)
target_compile_definitions(
  yyjson
  PRIVATE
    YYJSON_DISABLE_FAST_FP_CONV=1
    YYJSON_DISABLE_NON_STANDARD=1
)

if(SPROUT_BUILD_DESKTOP)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG 5d249570393f7a37e037abf22cd6012a4cc56a71
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL2)
endif()

add_subdirectory(launcher)
