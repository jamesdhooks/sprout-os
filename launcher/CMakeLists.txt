add_library(
  sprout_launcher_core
  src/built_in_background.cpp
  src/built_in_avatar.cpp
  src/demo_household.cpp
  src/framebuffer_page_writer.cpp
  src/launcher_state.cpp
  src/portrait_outline.cpp
  src/profile_select_layout.cpp
  src/window_policy.cpp
)

add_library(
  sprout_profile_store
  src/profile_repository.cpp
)

add_library(
  sprout_game_library
  src/game_dashboard_model.cpp
  src/game_library_repository.cpp
)

add_library(
  sprout_household_seed
  src/household_seed.cpp
)

add_library(
  sprout_managed_profile_images
  src/managed_profile_image.cpp
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

add_library(
  sprout_parent_access
  src/parent_access_controller.cpp
  src/parent_access_store.cpp
  src/parent_pin_presentation.cpp
)

add_library(
  sprout_daily_time_policy
  src/daily_time_policy.cpp
)

add_library(
  sprout_startup_health
  src/startup_health.cpp
)

add_library(
  sprout_recovery
  src/recovery_presentation.cpp
)

add_library(
  sprout_profile_archive
  src/profile_archive.cpp
  src/profile_archive_presentation.cpp
)

add_library(
  sprout_profile_avatars
  src/profile_avatar_presentation.cpp
)

add_library(
  sprout_onion_adapter
  src/demo_library.cpp
  src/local_library.cpp
  src/onion_launch_adapter.cpp
  src/onion_runtime_handoff.cpp
  src/pico8_catalogue.cpp
)

add_library(
  sprout_game_library_integration
  src/game_library_integration.cpp
)

add_library(
  sprout_game_dashboard_presentation
  src/game_dashboard_presentation.cpp
)

add_library(sprout_launch_process src/launch_process.cpp)

if(SPROUT_BUILD_RUNTIME)
  add_library(
    sprout_native_games
    src/native_launch_adapter.cpp
    src/native_library.cpp
  )
  target_include_directories(sprout_native_games PUBLIC include)
  target_compile_features(sprout_native_games PUBLIC cxx_std_20)
  target_link_libraries(
    sprout_native_games
    PUBLIC sprout_launch_process
    PRIVATE sprout_runtime_package
  )
endif()

target_include_directories(sprout_launcher_core PUBLIC include)
target_compile_features(sprout_launcher_core PUBLIC cxx_std_20)
target_include_directories(sprout_profile_store PUBLIC include)
target_compile_features(sprout_profile_store PUBLIC cxx_std_20)
target_link_libraries(sprout_profile_store PRIVATE sprout_sqlite)
target_include_directories(sprout_game_library PUBLIC include)
target_compile_features(sprout_game_library PUBLIC cxx_std_20)
target_link_libraries(sprout_game_library PRIVATE sprout_sqlite)
target_include_directories(sprout_household_seed PUBLIC include)
target_compile_features(sprout_household_seed PUBLIC cxx_std_20)
target_link_libraries(sprout_household_seed PRIVATE sprout_profile_store yyjson)
target_include_directories(sprout_managed_profile_images PUBLIC include)
target_compile_features(sprout_managed_profile_images PUBLIC cxx_std_20)
target_include_directories(sprout_configuration PUBLIC include)
target_compile_features(sprout_configuration PUBLIC cxx_std_20)
target_link_libraries(sprout_configuration PRIVATE yyjson)
target_include_directories(sprout_setup PUBLIC include)
target_compile_features(sprout_setup PUBLIC cxx_std_20)
target_link_libraries(
  sprout_setup
  PRIVATE
    sprout_configuration
    sprout_daily_time_policy
    sprout_profile_store
)
target_include_directories(sprout_parent_access PUBLIC include)
target_compile_features(sprout_parent_access PUBLIC cxx_std_20)
target_include_directories(
  sprout_parent_access
  SYSTEM PRIVATE "${argon2_SOURCE_DIR}/src/blake2"
)
target_link_libraries(
  sprout_parent_access
  PRIVATE sprout_argon2 sprout_sqlite sprout_launcher_core
)
if(WIN32)
  target_link_libraries(sprout_parent_access PRIVATE bcrypt)
endif()
target_include_directories(sprout_daily_time_policy PUBLIC include)
target_compile_features(sprout_daily_time_policy PUBLIC cxx_std_20)
target_link_libraries(sprout_daily_time_policy PRIVATE sprout_sqlite)
target_include_directories(sprout_startup_health PUBLIC include)
target_compile_features(sprout_startup_health PUBLIC cxx_std_20)
target_link_libraries(sprout_startup_health PRIVATE sprout_sqlite)
target_include_directories(sprout_recovery PUBLIC include)
target_compile_features(sprout_recovery PUBLIC cxx_std_20)
target_link_libraries(
  sprout_recovery
  PRIVATE sprout_configuration sprout_launcher_core
)
target_include_directories(sprout_profile_archive PUBLIC include)
target_compile_features(sprout_profile_archive PUBLIC cxx_std_20)
target_include_directories(
  sprout_profile_archive
  SYSTEM PRIVATE "${argon2_SOURCE_DIR}/src/blake2"
)
target_link_libraries(
  sprout_profile_archive
  PRIVATE
    sprout_argon2
    sprout_daily_time_policy
    sprout_managed_profile_images
    sprout_profile_store
    yyjson
)
target_include_directories(sprout_profile_avatars PUBLIC include)
target_compile_features(sprout_profile_avatars PUBLIC cxx_std_20)
target_link_libraries(
  sprout_profile_avatars
  PRIVATE sprout_launcher_core sprout_profile_store
)
target_include_directories(sprout_onion_adapter PUBLIC include)
target_compile_features(sprout_onion_adapter PUBLIC cxx_std_20)
target_link_libraries(
  sprout_onion_adapter
  PUBLIC sprout_launch_process
  PRIVATE sprout_household_seed yyjson
)
target_include_directories(sprout_game_library_integration PUBLIC include)
target_compile_features(sprout_game_library_integration PUBLIC cxx_std_20)
target_link_libraries(
  sprout_game_library_integration
  PUBLIC sprout_game_library sprout_onion_adapter sprout_household_seed
)
target_include_directories(sprout_game_dashboard_presentation PUBLIC include)
target_compile_features(sprout_game_dashboard_presentation PUBLIC cxx_std_20)
target_link_libraries(
  sprout_game_dashboard_presentation
  PUBLIC sprout_game_library sprout_launcher_core
)
target_include_directories(sprout_launch_process PUBLIC include)
target_compile_features(sprout_launch_process PUBLIC cxx_std_20)

if(SPROUT_BUILD_ONION)
  add_executable(sprout-onion-check src/onion_check.cpp)
  target_compile_features(sprout-onion-check PRIVATE cxx_std_20)
  target_link_libraries(
    sprout-onion-check
    PRIVATE
      sprout_configuration
      sprout_daily_time_policy
      sprout_launcher_core
      sprout_onion_adapter
      sprout_parent_access
      sprout_profile_archive
      sprout_profile_store
      sprout_household_seed
      sprout_startup_health
      stdc++fs
  )
  target_link_options(sprout-onion-check PRIVATE -static-libgcc -static-libstdc++)
  add_custom_command(
    TARGET sprout-onion-check
    POST_BUILD
    COMMAND "${CMAKE_STRIP}" "$<TARGET_FILE:sprout-onion-check>"
    VERBATIM
  )
endif()

if(MSVC)
  target_compile_options(sprout_launcher_core PRIVATE /W4 /permissive-)
  target_compile_options(sprout_profile_store PRIVATE /W4 /permissive-)
  target_compile_options(sprout_game_library PRIVATE /W4 /permissive-)
  target_compile_options(sprout_household_seed PRIVATE /W4 /permissive-)
  target_compile_options(sprout_managed_profile_images PRIVATE /W4 /permissive-)
  target_compile_options(sprout_configuration PRIVATE /W4 /permissive-)
  target_compile_options(sprout_setup PRIVATE /W4 /permissive-)
  target_compile_options(sprout_parent_access PRIVATE /W4 /permissive-)
  target_compile_options(sprout_daily_time_policy PRIVATE /W4 /permissive-)
  target_compile_options(sprout_startup_health PRIVATE /W4 /permissive-)
  target_compile_options(sprout_recovery PRIVATE /W4 /permissive-)
  target_compile_options(sprout_profile_archive PRIVATE /W4 /permissive-)
  target_compile_options(sprout_profile_avatars PRIVATE /W4 /permissive-)
  target_compile_options(sprout_onion_adapter PRIVATE /W4 /permissive-)
  target_compile_options(sprout_game_library_integration PRIVATE /W4 /permissive-)
  target_compile_options(sprout_game_dashboard_presentation PRIVATE /W4 /permissive-)
  target_compile_options(sprout_launch_process PRIVATE /W4 /permissive-)
  if(SPROUT_BUILD_RUNTIME)
    target_compile_options(sprout_native_games PRIVATE /W4 /permissive-)
  endif()
  if(SPROUT_BUILD_ONION)
    target_compile_options(sprout-onion-check PRIVATE /W4 /permissive-)
  endif()
else()
  target_compile_options(sprout_launcher_core PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_profile_store PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_game_library PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_household_seed PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_managed_profile_images PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_configuration PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_setup PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_parent_access PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_daily_time_policy PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_startup_health PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_recovery PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_profile_archive PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_profile_avatars PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_onion_adapter PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_game_library_integration PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_game_dashboard_presentation PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_launch_process PRIVATE -Wall -Wextra -Wpedantic)
  if(SPROUT_BUILD_RUNTIME)
    target_compile_options(sprout_native_games PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(SPROUT_BUILD_ONION)
    target_compile_options(sprout-onion-check PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

if(SPROUT_BUILD_DESKTOP OR SPROUT_BUILD_ONION_UI)
  add_library(sprout_sdl_input src/sdl_input.cpp)
  target_include_directories(sprout_sdl_input PUBLIC include)
  target_compile_features(sprout_sdl_input PUBLIC cxx_std_20)
  target_link_libraries(
    sprout_sdl_input
    PUBLIC sprout_launcher_core ${SPROUT_SDL_TARGET}
  )

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
      sprout_managed_profile_images
      sprout_launcher_core
      ${SPROUT_SDL_TARGET}
      SDL2_image
  )

  add_executable(
    sprout-launcher
    src/desktop_view.cpp
    src/main.cpp
  )

  target_compile_features(sprout-launcher PRIVATE cxx_std_20)
  target_include_directories(sprout-launcher PRIVATE src ${PROJECT_SOURCE_DIR}/shared/include)
  target_link_libraries(
    sprout-launcher
    PRIVATE
      sprout_launcher_core
      sprout_sdl_input
      sprout_profile_store
      sprout_game_library_integration
      sprout_game_dashboard_presentation
      sprout_household_seed
      sprout_profile_images
      sprout_parent_access
      sprout_profile_archive
      sprout_profile_avatars
      sprout_onion_adapter
      sprout_native_games
      sprout_setup
      sprout_recovery
      sprout_startup_health
      SDL2_image
      ${SPROUT_SDL_TARGET}
  )
  if(SPROUT_BUILD_DESKTOP)
    target_link_libraries(sprout-launcher PRIVATE SDL2::SDL2main)
  else()
    target_link_libraries(sprout-launcher PRIVATE stdc++fs)
    target_link_options(
      sprout-launcher PRIVATE
      -static-libgcc
      -static-libstdc++
      -Wl,--allow-shlib-undefined
    )
    add_custom_command(
      TARGET sprout-launcher POST_BUILD
      COMMAND "${CMAKE_STRIP}" "$<TARGET_FILE:sprout-launcher>"
      VERBATIM
    )
  endif()

  add_custom_command(
    TARGET sprout-launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:sprout-launcher>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${PROJECT_SOURCE_DIR}/shared/assets
            $<TARGET_FILE_DIR:sprout-launcher>/assets
    COMMENT "Copying launcher visual assets"
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
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  add_test(
    NAME profile-avatar-assets
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/validate-profile-avatars.py"
  )

  add_test(
    NAME pico8-mouse-cheese-campaign
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/pico8_mouse_cheese_campaign.py"
            validate
            --spec "${PROJECT_SOURCE_DIR}/pico8/mouse-cheese/campaign-spec.json"
            --campaign "${PROJECT_SOURCE_DIR}/pico8/mouse-cheese/campaign.json"
            --cart "${PROJECT_SOURCE_DIR}/pico8/mouse-cheese/mouse-cheese.p8"
  )

  add_executable(sprout-launcher-state-test tests/launcher_state_test.cpp)
  target_compile_features(sprout-launcher-state-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-launcher-state-test PRIVATE sprout_launcher_core)

  add_test(NAME launcher-state COMMAND sprout-launcher-state-test)

  add_executable(sprout-portrait-outline-test tests/portrait_outline_test.cpp)
  target_compile_features(sprout-portrait-outline-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-portrait-outline-test PRIVATE sprout_launcher_core)
  add_test(NAME portrait-outline COMMAND sprout-portrait-outline-test)

  add_executable(sprout-profile-select-layout-test
                 tests/profile_select_layout_test.cpp)
  target_compile_features(sprout-profile-select-layout-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-profile-select-layout-test PRIVATE sprout_launcher_core)
  add_test(NAME profile-select-layout COMMAND sprout-profile-select-layout-test)

  add_executable(sprout-framebuffer-page-writer-test
                 tests/framebuffer_page_writer_test.cpp)
  target_compile_features(sprout-framebuffer-page-writer-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-framebuffer-page-writer-test
                        PRIVATE sprout_launcher_core)
  add_test(NAME framebuffer-page-writer
           COMMAND sprout-framebuffer-page-writer-test)

  add_executable(sprout-window-policy-test tests/window_policy_test.cpp)
  target_compile_features(sprout-window-policy-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-window-policy-test PRIVATE sprout_launcher_core)
  add_test(NAME window-policy COMMAND sprout-window-policy-test)

  add_executable(sprout-profile-store-test tests/profile_repository_test.cpp)
  target_compile_features(sprout-profile-store-test PRIVATE cxx_std_20)
  target_link_libraries(
    sprout-profile-store-test
    PRIVATE
      sprout_profile_store
      sprout_sqlite
  )
  add_test(NAME profile-store COMMAND sprout-profile-store-test)

  add_executable(sprout-game-library-test tests/game_library_repository_test.cpp)
  target_compile_features(sprout-game-library-test PRIVATE cxx_std_20)
  target_link_libraries(
    sprout-game-library-test
    PRIVATE sprout_game_library sprout_sqlite
  )
  add_test(NAME game-library COMMAND sprout-game-library-test)

  add_executable(sprout-game-dashboard-model-test
                 tests/game_dashboard_model_test.cpp)
  target_compile_features(sprout-game-dashboard-model-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-game-dashboard-model-test
                        PRIVATE sprout_game_library)
  add_test(NAME game-dashboard-model COMMAND sprout-game-dashboard-model-test)

  add_executable(sprout-game-library-integration-test
                 tests/game_library_integration_test.cpp)
  target_compile_features(sprout-game-library-integration-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-game-library-integration-test
                        PRIVATE sprout_game_library_integration)
  add_test(NAME game-library-integration
           COMMAND sprout-game-library-integration-test)

  add_executable(sprout-game-dashboard-presentation-test
                 tests/game_dashboard_presentation_test.cpp)
  target_compile_features(sprout-game-dashboard-presentation-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-game-dashboard-presentation-test
                        PRIVATE sprout_game_dashboard_presentation)
  add_test(NAME game-dashboard-presentation
           COMMAND sprout-game-dashboard-presentation-test)

  add_executable(sprout-household-seed-test tests/household_seed_test.cpp)
  target_compile_features(sprout-household-seed-test PRIVATE cxx_std_20)
  target_compile_definitions(sprout-household-seed-test
                             PRIVATE SPROUT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  target_link_libraries(sprout-household-seed-test PRIVATE sprout_household_seed)
  add_test(NAME household-seed COMMAND sprout-household-seed-test)

  add_executable(sprout-profile-avatar-presentation-test
                 tests/profile_avatar_presentation_test.cpp)
  target_compile_features(sprout-profile-avatar-presentation-test
                          PRIVATE cxx_std_20)
  target_link_libraries(sprout-profile-avatar-presentation-test
                        PRIVATE sprout_profile_avatars sprout_profile_store)
  add_test(NAME profile-avatar-presentation
           COMMAND sprout-profile-avatar-presentation-test)

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

  add_executable(sprout-parent-access-test tests/parent_access_store_test.cpp)
  target_compile_features(sprout-parent-access-test PRIVATE cxx_std_20)
  target_link_libraries(
    sprout-parent-access-test
    PRIVATE sprout_parent_access sprout_sqlite
  )
  add_test(NAME parent-access COMMAND sprout-parent-access-test)

  add_executable(sprout-parent-pin-presentation-test
                 tests/parent_pin_presentation_test.cpp)
  target_compile_features(sprout-parent-pin-presentation-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-parent-pin-presentation-test
                        PRIVATE sprout_parent_access)
  add_test(NAME parent-pin-presentation
           COMMAND sprout-parent-pin-presentation-test)

  add_executable(sprout-parent-access-controller-test
                 tests/parent_access_controller_test.cpp)
  target_compile_features(sprout-parent-access-controller-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-parent-access-controller-test
                        PRIVATE sprout_parent_access)
  add_test(NAME parent-access-controller
           COMMAND sprout-parent-access-controller-test)

  add_executable(sprout-daily-time-policy-test
                 tests/daily_time_policy_test.cpp)
  target_compile_features(sprout-daily-time-policy-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-daily-time-policy-test
                        PRIVATE sprout_daily_time_policy sprout_sqlite)
  add_test(NAME daily-time-policy COMMAND sprout-daily-time-policy-test)

  add_executable(sprout-startup-health-test tests/startup_health_test.cpp)
  target_compile_features(sprout-startup-health-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-startup-health-test
                        PRIVATE sprout_startup_health sprout_sqlite)
  add_test(NAME startup-health COMMAND sprout-startup-health-test)

  add_executable(sprout-recovery-presentation-test
                 tests/recovery_presentation_test.cpp)
  target_compile_features(sprout-recovery-presentation-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-recovery-presentation-test
                        PRIVATE sprout_recovery sprout_configuration)
  add_test(NAME recovery-presentation
           COMMAND sprout-recovery-presentation-test)

  add_executable(sprout-mvp-integration-test tests/mvp_integration_test.cpp)
  target_compile_features(sprout-mvp-integration-test PRIVATE cxx_std_20)
  target_link_libraries(
    sprout-mvp-integration-test
    PRIVATE
      sprout_configuration
      sprout_daily_time_policy
      sprout_launcher_core
      sprout_onion_adapter
      sprout_parent_access
      sprout_profile_archive
      sprout_profile_store
      sprout_recovery
      sprout_setup
      sprout_startup_health
  )
  add_test(NAME mvp-host-integration COMMAND sprout-mvp-integration-test)

  add_executable(sprout-profile-archive-test tests/profile_archive_test.cpp)
  target_compile_features(sprout-profile-archive-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-profile-archive-test
                        PRIVATE sprout_profile_archive)
  add_test(NAME profile-archive COMMAND sprout-profile-archive-test)

  add_executable(sprout-profile-archive-presentation-test
                 tests/profile_archive_presentation_test.cpp)
  target_compile_features(sprout-profile-archive-presentation-test
                          PRIVATE cxx_std_20)
  target_link_libraries(sprout-profile-archive-presentation-test
                        PRIVATE sprout_profile_archive)
  add_test(NAME profile-archive-presentation
           COMMAND sprout-profile-archive-presentation-test)

  add_executable(sprout-onion-launch-adapter-test
                 tests/onion_launch_adapter_test.cpp)
  target_compile_features(sprout-onion-launch-adapter-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-onion-launch-adapter-test
                        PRIVATE sprout_onion_adapter)
  add_test(NAME onion-launch-adapter COMMAND sprout-onion-launch-adapter-test)

  add_executable(sprout-local-library-test tests/local_library_test.cpp)
  target_compile_features(sprout-local-library-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-local-library-test PRIVATE sprout_onion_adapter)
  add_test(NAME local-library COMMAND sprout-local-library-test)

  if(TARGET sprout_sdl_input)
    add_executable(sprout-sdl-input-test tests/sdl_input_test.cpp)
    target_compile_features(sprout-sdl-input-test PRIVATE cxx_std_20)
    target_link_libraries(
      sprout-sdl-input-test
      PRIVATE sprout_sdl_input SDL2::SDL2main
    )
    add_test(NAME sdl-input COMMAND sprout-sdl-input-test)
  endif()

  if(SPROUT_BUILD_RUNTIME)
    add_executable(sprout-native-library-test tests/native_library_test.cpp)
    target_compile_features(sprout-native-library-test PRIVATE cxx_std_20)
    target_link_libraries(sprout-native-library-test PRIVATE sprout_native_games)
    add_test(NAME native-library COMMAND sprout-native-library-test)

    add_executable(sprout-native-launch-adapter-test
                   tests/native_launch_adapter_test.cpp)
    target_compile_features(sprout-native-launch-adapter-test PRIVATE cxx_std_20)
    target_link_libraries(sprout-native-launch-adapter-test
                          PRIVATE sprout_native_games)
    add_test(NAME native-launch-adapter
             COMMAND sprout-native-launch-adapter-test)
  endif()

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
