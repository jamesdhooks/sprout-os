add_library(
  sprout_runtime_package
  src/package.cpp
)
target_include_directories(sprout_runtime_package PUBLIC include)
target_compile_features(sprout_runtime_package PUBLIC cxx_std_20)
target_link_libraries(sprout_runtime_package PRIVATE yyjson)

add_library(
  sprout_runtime_core
  src/session.cpp
)

target_include_directories(sprout_runtime_core PUBLIC include)
target_compile_features(sprout_runtime_core PUBLIC cxx_std_20)
target_link_libraries(sprout_runtime_core PUBLIC sprout_runtime_package)
target_link_libraries(sprout_runtime_core PRIVATE sprout_lua yyjson)

if(MSVC)
  target_compile_options(sprout_runtime_package PRIVATE /W4 /permissive-)
  target_compile_options(sprout_runtime_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(sprout_runtime_package PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_runtime_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SPROUT_BUILD_DESKTOP)
  add_executable(sprout-runtime src/main.cpp)
  target_compile_features(sprout-runtime PRIVATE cxx_std_20)
  target_link_libraries(
    sprout-runtime
    PRIVATE sprout_runtime_core SDL2::SDL2main SDL2::SDL2-static
  )
  if(MSVC)
    target_compile_options(sprout-runtime PRIVATE /W4 /permissive-)
  else()
    target_compile_options(sprout-runtime PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

if(BUILD_TESTING)
  add_executable(sprout-runtime-test tests/runtime_test.cpp)
  target_compile_features(sprout-runtime-test PRIVATE cxx_std_20)
  target_compile_definitions(
    sprout-runtime-test PRIVATE SPROUT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  target_link_libraries(sprout-runtime-test PRIVATE sprout_runtime_core)
  add_test(NAME runtime-core COMMAND sprout-runtime-test)

  if(SPROUT_BUILD_DESKTOP)
    add_test(
      NAME arcade-snake-smoke
      COMMAND sprout-runtime --package "${PROJECT_SOURCE_DIR}/games/snake"
              --storage "${CMAKE_CURRENT_BINARY_DIR}/snake-smoke-data"
              --seed 7 --smoke-test
    )
  endif()
endif()
