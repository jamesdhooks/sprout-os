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
  sprout_onion_adapter
  src/demo_library.cpp
  src/library_presentation.cpp
  src/local_library.cpp
  src/onion_launch_adapter.cpp
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
target_include_directories(sprout_onion_adapter PUBLIC include)
target_compile_features(sprout_onion_adapter PUBLIC cxx_std_20)
target_link_libraries(sprout_onion_adapter PUBLIC sprout_launch_process)
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
  target_compile_options(sprout_managed_profile_images PRIVATE /W4 /permissive-)
  target_compile_options(sprout_configuration PRIVATE /W4 /permissive-)
  target_compile_options(sprout_setup PRIVATE /W4 /permissive-)
  target_compile_options(sprout_parent_access PRIVATE /W4 /permissive-)
  target_compile_options(sprout_daily_time_policy PRIVATE /W4 /permissive-)
  target_compile_options(sprout_startup_health PRIVATE /W4 /permissive-)
  target_compile_options(sprout_recovery PRIVATE /W4 /permissive-)
  target_compile_options(sprout_profile_archive PRIVATE /W4 /permissive-)
  target_compile_options(sprout_onion_adapter PRIVATE /W4 /permissive-)
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
  target_compile_options(sprout_managed_profile_images PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_configuration PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_setup PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_parent_access PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_daily_time_policy PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_startup_health PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_recovery PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_profile_archive PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_onion_adapter PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(sprout_launch_process PRIVATE -Wall -Wextra -Wpedantic)
  if(SPROUT_BUILD_RUNTIME)
    target_compile_options(sprout_native_games PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(SPROUT_BUILD_ONION)
    target_compile_options(sprout-onion-check PRIVATE -Wall -Wextra -Wpedantic)
  endif()
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
      sprout_managed_profile_images
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
      sprout_parent_access
      sprout_profile_archive
      sprout_onion_adapter
      sprout_native_games
      sprout_setup
      sprout_recovery
      sprout_startup_health
      SDL2_image
      SDL2::SDL2main
      SDL2::SDL2-static
  )

  add_custom_command(
    TARGET sprout-launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
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

  add_executable(sprout-library-presentation-test
                 tests/library_presentation_test.cpp)
  target_compile_features(sprout-library-presentation-test PRIVATE cxx_std_20)
  target_link_libraries(sprout-library-presentation-test
                        PRIVATE sprout_onion_adapter sprout_launcher_core)
  add_test(NAME library-presentation COMMAND sprout-library-presentation-test)

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
