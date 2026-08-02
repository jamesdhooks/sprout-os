cmake_minimum_required(VERSION 3.24)

project(Sprout LANGUAGES C CXX)

include(CTest)
include(FetchContent)
find_package(Threads REQUIRED)

option(SPROUT_BUILD_DESKTOP "Build the SDL desktop launcher preview" ON)
option(SPROUT_BUILD_ONION "Build Onion target diagnostics" OFF)

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
target_link_libraries(sprout_sqlite PUBLIC Threads::Threads)
if(SPROUT_BUILD_ONION)
  target_link_libraries(sprout_sqlite PUBLIC pthread)
endif()
target_compile_definitions(
  sprout_sqlite
  PRIVATE
    SQLITE_DEFAULT_FOREIGN_KEYS=1
    SQLITE_DQS=0
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_THREADSAFE=1
)

FetchContent_Declare(
  Argon2
  GIT_REPOSITORY https://github.com/P-H-C/phc-winner-argon2.git
  GIT_TAG 62358ba2123abd17fccf2a108a301d4b52c01a7c
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Argon2)

add_library(
  sprout_argon2 STATIC
  "${argon2_SOURCE_DIR}/src/argon2.c"
  "${argon2_SOURCE_DIR}/src/core.c"
  "${argon2_SOURCE_DIR}/src/encoding.c"
  "${argon2_SOURCE_DIR}/src/ref.c"
  "${argon2_SOURCE_DIR}/src/blake2/blake2b.c"
)
target_include_directories(
  sprout_argon2
  PUBLIC "${argon2_SOURCE_DIR}/include"
  PRIVATE "${argon2_SOURCE_DIR}/src" "${argon2_SOURCE_DIR}/src/blake2"
)
target_compile_definitions(sprout_argon2 PRIVATE ARGON2_NO_THREADS)

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

  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_VENDORED ON CACHE BOOL "" FORCE)
  set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
  set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_BMP ON CACHE BOOL "" FORCE)
  set(SDL2IMAGE_GIF OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_JPG ON CACHE BOOL "" FORCE)
  set(SDL2IMAGE_JPG_SAVE ON CACHE BOOL "" FORCE)
  set(SDL2IMAGE_JXL OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_LBM OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_PCX OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_PNG ON CACHE BOOL "" FORCE)
  set(SDL2IMAGE_PNG_SAVE ON CACHE BOOL "" FORCE)
  set(SDL2IMAGE_PNM OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_QOI OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_SVG OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_TGA OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_TIF OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_XCF OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_XPM OFF CACHE BOOL "" FORCE)
  set(SDL2IMAGE_XV OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    SDL2_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG c1bf2245b0ba63a25afe2f8574d305feca25af77
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL2_image)
  target_compile_definitions(SDL2_image PRIVATE STBI_MAX_DIMENSIONS=8192)
endif()

add_subdirectory(launcher)
