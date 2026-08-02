add_library(
  sprout_launcher_core
  src/demo_household.cpp
  src/launcher_state.cpp
)

add_library(
  sprout_profile_store
  src/profile_repository.cpp
)

add_library(
  sprout_configuration
  src/local_configuration.cpp
)

add_library(
  sprout_setup
  src/setup_presentation.cpp
  src/setup_wizard.cpp
)

target_include_directories(sprout_launcher_core PUBLIC include)
target_compile_features(sprout_launcher_core PUBLIC cxx_std_20)
target_include_directories(sprout_profile_store PUBLIC include)
target_compile_features(sprout_profile_store PUBLIC cxx_std_20)
target_link_libraries(sprout_profile_store PRIVATE sprout_sqlite)
target_include_directories(sprout_configuration PUBLIC include)
target_compile_features(sprout_configuration PUBLIC cxx_std_20)
target_link_libraries(sprout_configuration PRIVATE yyjson)
target_include_directories(sprout_setup PUBLIC include)
target_compile_features(sprout_setup PUBLIC cxx_std_20)
target_link_libraries(
  sprout_setup
  PRIVATE
    sprout_configuration
    sprout_profile_store
)

if(MSVC)
  target_compile_options(sprout_launcher_core PRIVATE /W4 /permissive-)
  target_compile_options(sprout_profile_store PRIVATE /W4 /permissive-)
  target_compile_options(sprout_configuration PRIVATE /W4 /permissive-)
  target_compile_options(sprout_setup PRIVATE /W4 /permissive-)
else()
  target_compile_options(sprout_launcher_core PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_profile_store PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_configuration PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_setup PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SPROUT_BUILD_DESKTOP)
  add_library(
    sprout_profile_images
    src/profile_image_crop_presentation.cpp
    src/profile_image_importer.cpp
  )
  target_include_directories(sprout_profile_images PUBLIC include)
  target_compile_features(sprout_profile_images PUBLIC cxx_std_20)
  target_link_libraries(
    sprout_profile_images
    PRIVATE
      sprout_profile_store
      sprout_launcher_core
      SDL2::SDL2-static
      SDL2_image
  )

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
      sprout_profile_store
      sprout_profile_images
      sprout_setup
      SDL2_image
      SDL2::SDL2main
      SDL2::SDL2-static
  )

  if(MSVC)
    target_compile_options(sprout_profile_images PRIVATE /W4 /permissive-)
    target_compile_options(sprout-launcher PRIVATE /W4 /permissive-)
  else()
    target_compile_options(sprout_profile_images PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(sprout-launcher PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

if(BUILD_TESTING)
  add_executable(sprout-launcher-state-test tests/launcher_state_test.cpp)
  target_compile_features(sprout-launcher-state-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-launcher-state-test PRIVATE sprout_launcher_core)

  add_test(NAME launcher-state COMMAND sprout-launcher-state-test)

  add_executable(sprout-profile-store-test tests/profile_repository_test.cpp)
  target_compile_features(sprout-profile-store-test PRIVATE cxx_std_20)
  target_link_libraries(
    sprout-profile-store-test
    PRIVATE
      sprout_profile_store
      sprout_sqlite
  )
  add_test(NAME profile-store COMMAND sprout-profile-store-test)

  add_executable(sprout-configuration-test tests/local_configuration_test.cpp)
  target_compile_features(sprout-configuration-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-configuration-test PRIVATE sprout_configuration)
  add_test(NAME local-configuration COMMAND sprout-configuration-test)

  add_executable(sprout-setup-test tests/setup_wizard_test.cpp)
  target_compile_features(sprout-setup-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-setup-test PRIVATE sprout_setup)
  add_test(NAME setup-wizard COMMAND sprout-setup-test)

  add_executable(sprout-setup-presentation-test tests/setup_presentation_test.cpp)
  target_compile_features(sprout-setup-presentation-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-setup-presentation-test PRIVATE sprout_setup)
  add_test(NAME setup-presentation COMMAND sprout-setup-presentation-test)

  if(SPROUT_BUILD_DESKTOP)
    add_executable(sprout-profile-image-test tests/profile_image_importer_test.cpp)
    target_compile_features(sprout-profile-image-test PRIVATE cxx_std_20)
    target_link_libraries(
      sprout-profile-image-test
      PRIVATE
        sprout_profile_images
        SDL2::SDL2-static
        SDL2_image
    )
    add_test(NAME profile-image COMMAND sprout-profile-image-test)

    add_test(NAME launcher-desktop-smoke COMMAND sprout-launcher --smoke-test)
    set_tests_properties(
      launcher-desktop-smoke
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    )
  endif()
endif()
