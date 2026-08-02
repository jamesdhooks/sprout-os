cmake_minimum_required(VERSION 3.24)

project(Sprout LANGUAGES CXX)

include(CTest)
include(FetchContent)

option(SPROUT_BUILD_DESKTOP "Build the SDL desktop launcher preview" ON)

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
