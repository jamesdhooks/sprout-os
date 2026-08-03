add_library(
  sprout_runtime_package
  src/assets.cpp
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
  target_include_directories(sprout-runtime PRIVATE ${PROJECT_SOURCE_DIR}/shared/include)
  target_link_libraries(
    sprout-runtime
    PRIVATE sprout_runtime_core SDL2::SDL2main SDL2::SDL2-static SDL2_image
  )
  add_custom_command(
    TARGET sprout-runtime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${PROJECT_SOURCE_DIR}/shared/assets
            $<TARGET_FILE_DIR:sprout-runtime>/assets
    COMMENT "Copying shared runtime visual assets"
  )
  if(MSVC)
    target_compile_options(sprout-runtime PRIVATE /W4 /permissive-)
  else()
    target_compile_options(sprout-runtime PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  add_executable(sprout-runtime-test tests/runtime_test.cpp)
  target_compile_features(sprout-runtime-test PRIVATE cxx_std_20)
  target_compile_definitions(
    sprout-runtime-test PRIVATE SPROUT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  target_link_libraries(sprout-runtime-test PRIVATE sprout_runtime_core)
  add_test(NAME runtime-core COMMAND sprout-runtime-test)
  add_test(
    NAME arcade-asset-reproducibility
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/generate-arcade-assets.py" --check
  )
  add_test(
    NAME arcade-atlas-importer
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/import-sprite-atlases-test.py"
  )
  foreach(game_package IN ITEMS mouse-maze blocks-buttons snake)
    add_test(
      NAME "arcade-${game_package}-atlas-import"
      COMMAND "${Python3_EXECUTABLE}"
              "${PROJECT_SOURCE_DIR}/tools/import-sprite-atlases.py"
              --config "${PROJECT_SOURCE_DIR}/games/${game_package}/assets-src/atlas-import.json"
              --output "${PROJECT_SOURCE_DIR}/games/${game_package}/rich-asset-manifest.json"
              --check
    )
  endforeach()

  if(SPROUT_BUILD_DESKTOP)
    add_test(
      NAME arcade-snake-smoke
      COMMAND sprout-runtime --package "${PROJECT_SOURCE_DIR}/games/snake"
              --storage "${CMAKE_CURRENT_BINARY_DIR}/snake-smoke-data"
              --seed 7 --smoke-test
    )
    add_test(
      NAME arcade-mouse-maze-smoke
      COMMAND sprout-runtime --package "${PROJECT_SOURCE_DIR}/games/mouse-maze"
              --storage "${CMAKE_CURRENT_BINARY_DIR}/mouse-maze-smoke-data"
              --seed 7 --smoke-test
    )
    add_test(
      NAME arcade-blocks-buttons-smoke
      COMMAND sprout-runtime --package "${PROJECT_SOURCE_DIR}/games/blocks-buttons"
              --storage "${CMAKE_CURRENT_BINARY_DIR}/blocks-buttons-smoke-data"
              --seed 7 --smoke-test
    )
  endif()
endif()
