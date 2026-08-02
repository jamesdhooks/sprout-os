add_library(
  sprout_launcher_core
  src/demo_household.cpp
  src/launcher_state.cpp
)

target_include_directories(sprout_launcher_core PUBLIC include)
target_compile_features(sprout_launcher_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(sprout_launcher_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(sprout_launcher_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SPROUT_BUILD_DESKTOP)
  add_executable(
    sprout-launcher
    src/desktop_view.cpp
    src/main.cpp
  )

  target_compile_features(sprout-launcher PRIVATE cxx_std_20)
  target_include_directories(sprout-launcher PRIVATE src)
  target_link_libraries(
    sprout-launcher
    PRIVATE
      sprout_launcher_core
      SDL2::SDL2main
      SDL2::SDL2-static
  )

  if(MSVC)
    target_compile_options(sprout-launcher PRIVATE /W4 /permissive-)
  else()
    target_compile_options(sprout-launcher PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

if(BUILD_TESTING)
  add_executable(sprout-launcher-state-test tests/launcher_state_test.cpp)
  target_compile_features(sprout-launcher-state-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-launcher-state-test PRIVATE sprout_launcher_core)

  add_test(NAME launcher-state COMMAND sprout-launcher-state-test)

  if(SPROUT_BUILD_DESKTOP)
    add_test(NAME launcher-desktop-smoke COMMAND sprout-launcher --smoke-test)
    set_tests_properties(
      launcher-desktop-smoke
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    )
  endif()
endif()
