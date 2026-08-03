#include "desktop_view.hpp"
#include "sprout/launcher/daily_time_policy.hpp"
#include "sprout/launcher/local_configuration.hpp"
#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/library_presentation.hpp"
#include "sprout/launcher/local_library.hpp"
#include "sprout/launcher/native_launch_adapter.hpp"
#include "sprout/launcher/native_library.hpp"
#include "sprout/launcher/profile_archive.hpp"
#include "sprout/launcher/profile_archive_presentation.hpp"
#include "sprout/launcher/profile_repository.hpp"
#include "sprout/launcher/recovery_presentation.hpp"
#include "sprout/launcher/parent_access_store.hpp"
#include "sprout/launcher/parent_access_controller.hpp"
#include "sprout/launcher/parent_pin_presentation.hpp"
#include "sprout/launcher/profile_image_crop_presentation.hpp"
#include "sprout/launcher/profile_image_importer.hpp"
#include "sprout/launcher/setup_presentation.hpp"
#include "sprout/launcher/setup_wizard.hpp"
#include "sprout/launcher/string_compat.hpp"
#include "sprout/launcher/startup_health.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <variant>

namespace {

using sprout::launcher::Action;
using sprout::launcher::LauncherState;

class TemporaryDirectory {
 public:
  explicit TemporaryDirectory(std::string_view purpose) {
    const auto suffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sprout-" + std::string(purpose) + "-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

std::optional<Action> keyboard_action(SDL_Keycode key) {
  switch (key) {
    case SDLK_UP:
    case SDLK_w:
      return Action::Up;
    case SDLK_DOWN:
    case SDLK_s:
      return Action::Down;
    case SDLK_LEFT:
    case SDLK_a:
      return Action::Left;
    case SDLK_RIGHT:
    case SDLK_d:
      return Action::Right;
    case SDLK_RETURN:
    case SDLK_SPACE:
    case SDLK_z:
      return Action::Confirm;
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE:
    case SDLK_x:
      return Action::Back;
    case SDLK_e:
      return Action::ZoomIn;
    case SDLK_q:
      return Action::ZoomOut;
    default:
      return std::nullopt;
  }
}

std::optional<Action> controller_action(std::uint8_t button) {
  switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      return Action::Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      return Action::Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      return Action::Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      return Action::Right;
    case SDL_CONTROLLER_BUTTON_A:
      return Action::Confirm;
    case SDL_CONTROLLER_BUTTON_B:
      return Action::Back;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
      return Action::ZoomIn;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
      return Action::ZoomOut;
    default:
      return std::nullopt;
  }
}

sprout::launcher::AccessMoment current_access_time() {
  const std::time_t now = std::time(nullptr);
  if (now < 0) {
    throw std::runtime_error("System clock is unavailable");
  }
  std::tm local{};
#ifdef _WIN32
  if (localtime_s(&local, &now) != 0) {
#else
  if (localtime_r(&now, &local) == nullptr) {
#endif
    throw std::runtime_error("Local calendar date is unavailable");
  }
  std::array<char, 11> date{};
  if (std::strftime(date.data(), date.size(), "%Y-%m-%d", &local) != 10) {
    throw std::runtime_error("Local calendar date could not be formatted");
  }
  return sprout::launcher::AccessMoment{
      .utc_seconds = static_cast<std::int64_t>(now),
      .local_date = date.data(),
  };
}

sprout::launcher::TimePolicySample current_time_policy_sample() {
  const auto access = current_access_time();
  const auto monotonic = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch());
  return {
      .monotonic_milliseconds =
          static_cast<std::uint64_t>(monotonic.count()),
      .utc_seconds = access.utc_seconds,
      .local_date = access.local_date,
  };
}

SDL_Renderer* create_renderer(SDL_Window* window) {
  SDL_Renderer* renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == nullptr) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  return renderer;
}

std::vector<sprout::launcher::Profile> load_launcher_profiles(
    const sprout::launcher::ProfileRepository& repository);

std::optional<std::filesystem::path> pending_profile_image(
    const std::filesystem::path& data_root) {
  const auto import_root = data_root / "imports";
  for (const std::string_view filename : {"profile-image.png", "profile-image.jpg",
                                          "profile-image.jpeg", "profile-image.bmp"}) {
    const auto candidate = import_root / filename;
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error) && !error) {
      return candidate;
    }
  }
  return std::nullopt;
}

std::vector<sprout::launcher::LibraryEntry> discovered_library(
    const std::filesystem::path& sd_card_root) {
  const auto scan =
      sprout::launcher::LocalLibraryScanner(sd_card_root).discover();
  for (const auto& warning : scan.warnings) {
    std::cerr << "Library warning: " << warning << '\n';
  }
  std::vector<sprout::launcher::LibraryEntry> entries;
  entries.reserve(scan.items.size());
  for (auto item : scan.items) {
    entries.push_back(sprout::launcher::LibraryEntry{
        .id = item.id,
        .title = item.title,
        .platform_label =
            item.system == sprout::launcher::OnionSystem::GameBoy ? "GB" : "SFC",
        .launch_target = sprout::launcher::EmulatedLaunchTarget{
            .item_id = item.id,
            .system = item.system,
            .rom_path = std::move(item.rom_path),
            .launch_allowed = true,
        },
        .child_visible = false,
        .favorite = false,
        .recent_rank = std::nullopt,
        .launch_allowed = true,
        .unavailable_reason = {},
    });
  }
  return entries;
}

std::vector<sprout::launcher::LibraryEntry> discovered_native_library(
    const std::filesystem::path& packages_root) {
  const auto scan =
      sprout::launcher::NativePackageScanner(packages_root).discover();
  for (const auto& warning : scan.warnings) {
    std::cerr << "Library warning: " << warning << '\n';
  }
  std::vector<sprout::launcher::LibraryEntry> entries;
  entries.reserve(scan.items.size());
  for (auto item : scan.items) {
    entries.push_back(sprout::launcher::LibraryEntry{
        .id = item.id,
        .title = item.title,
        .platform_label = "ARCADE",
        .launch_target = sprout::launcher::NativeLaunchTarget{
            .item_id = item.id,
            .package_root = std::move(item.package_root),
            .profile_id = {},
            .seed = 1,
            .launch_allowed = true,
        },
        .child_visible = item.child_visible,
        .favorite = false,
        .recent_rank = std::nullopt,
        .launch_allowed = true,
        .unavailable_reason = {},
    });
  }
  return entries;
}

int run_smoke_test(SDL_Renderer* renderer) {
  LauncherState state(sprout::launcher::make_demo_household());
  sprout::launcher::render_launcher(renderer, state);
  (void)state.handle(Action::Confirm);
  sprout::launcher::render_launcher(renderer, state);
  (void)state.handle(Action::Back);
  (void)state.handle(Action::Right);
  (void)state.handle(Action::Confirm);
  sprout::launcher::render_launcher(renderer, state);

  TemporaryDirectory directory("desktop-smoke");

  sprout::launcher::ConfigurationStore recovery_configuration(
      directory.path() / "recovery-config");
  auto recovery_first = recovery_configuration.save(
      sprout::launcher::LocalConfiguration{});
  recovery_first.next_setup_step = sprout::launcher::SetupStep::Parent;
  (void)recovery_configuration.save(recovery_first);
  sprout::launcher::RecoveryPresentation recovery(recovery_configuration, 4);
  sprout::launcher::render_recovery(renderer, recovery);
  (void)recovery.handle(Action::Up);
  (void)recovery.handle(Action::Up);
  (void)recovery.handle(Action::Confirm);
  sprout::launcher::render_recovery(renderer, recovery);

  std::filesystem::create_directories(directory.path() / "data");
  sprout::launcher::ConfigurationStore configuration(directory.path() / "config");
  sprout::launcher::ProfileRepository profiles(directory.path() / "data" /
                                                "profiles.sqlite3");
  sprout::launcher::DailyTimePolicyStore time_policy(
      directory.path() / "data" / "time-policy.sqlite3");
  sprout::launcher::SetupWizard wizard(configuration, profiles, &time_policy);
  const auto source_path = directory.path() / "profile-image.bmp";
  SDL_Surface* source =
      SDL_CreateRGBSurfaceWithFormat(0, 8, 4, 32, SDL_PIXELFORMAT_RGBA32);
  if (source == nullptr) {
    std::cerr << "Profile image smoke fixture failed: " << SDL_GetError() << '\n';
    return EXIT_FAILURE;
  }
  SDL_FillRect(source, nullptr, SDL_MapRGB(source->format, 82, 142, 104));
  const int source_result = SDL_SaveBMP(source, source_path.string().c_str());
  SDL_FreeSurface(source);
  if (source_result != 0) {
    std::cerr << "Profile image smoke fixture save failed: " << SDL_GetError() << '\n';
    return EXIT_FAILURE;
  }

  sprout::launcher::SetupPresentation setup(wizard, true);
  const auto image_root = directory.path() / "data" / "profile-images";
  sprout::launcher::ProfileImageImporter importer(image_root, profiles);
  sprout::launcher::ParentAccessStore parent_access(
      directory.path() / "data" / "security.sqlite3",
      directory.path() / "secrets" / "device-access.key");
  bool imported_portrait = false;
  bool configured_pin = false;
  while (setup.step() != sprout::launcher::SetupStep::Complete) {
    sprout::launcher::render_setup(renderer, setup);
    if (setup.step() == sprout::launcher::SetupStep::ParentPin) {
      const auto event = setup.handle(Action::Confirm);
      if (event != sprout::launcher::SetupPresentationEvent::ConfigureParentPinRequested) {
        std::cerr << "Setup smoke test did not request parent PIN creation\n";
        return EXIT_FAILURE;
      }
      sprout::launcher::ParentPinPresentation pin(
          sprout::launcher::ParentPinMode::Create);
      sprout::launcher::render_parent_pin(renderer, pin);
      parent_access.set_pin("secret:parent-primary", "2468");
      setup.complete_parent_pin_step("secret:parent-primary");
      configured_pin = true;
    } else if (setup.step() == sprout::launcher::SetupStep::Avatars) {
      (void)setup.handle(Action::Right);
      (void)setup.handle(Action::Right);
      const auto event = setup.handle(Action::Confirm);
      if (event != sprout::launcher::SetupPresentationEvent::ImportParentImageRequested) {
        std::cerr << "Setup smoke test did not request the staged profile image\n";
        return EXIT_FAILURE;
      }
      sprout::launcher::ProfileImageCropPresentation crop(
          importer, "parent-primary", source_path);
      (void)crop.handle(Action::ZoomIn);
      (void)crop.handle(Action::Right);
      sprout::launcher::render_profile_image_crop(renderer, crop);
      if (crop.handle(Action::Confirm) !=
          sprout::launcher::ProfileImageCropEvent::Imported) {
        std::cerr << "Setup smoke test could not activate the profile image\n";
        return EXIT_FAILURE;
      }
      setup.complete_avatar_step();
      imported_portrait = true;
    } else {
      (void)setup.handle(Action::Confirm);
    }
    if (!setup.error_message().empty()) {
      std::cerr << "Setup smoke test failed: " << setup.error_message() << '\n';
      return EXIT_FAILURE;
    }
  }
  if (!imported_portrait || !configured_pin) {
    std::cerr << "Setup smoke test skipped a required profile or PIN flow\n";
    return EXIT_FAILURE;
  }
  const auto access_time = current_access_time();
  parent_access.grant_until_end_of_day("secret:parent-primary", "2468",
                                       access_time.utc_seconds,
                                       access_time.local_date);
  if (!parent_access.is_unlocked(access_time.utc_seconds, access_time.local_date)) {
    std::cerr << "Setup smoke test could not restore parent access\n";
    return EXIT_FAILURE;
  }
  LauncherState persisted(load_launcher_profiles(profiles));
  sprout::launcher::render_launcher(renderer, persisted, image_root);
  sprout::launcher::LibraryPresentation library(
      sprout::launcher::make_demo_library(),
      sprout::launcher::LibrarySection::Recent);
  sprout::launcher::render_library(renderer, library);
  const auto launch_event = library.handle(Action::Confirm);
  if (!launch_event.has_value() ||
      launch_event->type !=
          sprout::launcher::LibraryPresentationEventType::LaunchRequested ||
      !launch_event->launch_target.has_value()) {
    std::cerr << "Library smoke test did not emit a typed launch request\n";
    return EXIT_FAILURE;
  }
  sprout::launcher::ProfileArchiveService archives(profiles, time_policy,
                                                    image_root);
  sprout::launcher::ProfileArchivePresentation archive(
      profiles, archives, directory.path() / "exports",
      directory.path() / "imports");
  sprout::launcher::render_profile_archive(renderer, archive);
  (void)archive.handle(Action::Confirm);
  for (std::size_t index = 0;
       index < archive.choices().size() &&
       archive.choices()[archive.focus_index()] != "Alex";
       ++index) {
    (void)archive.handle(Action::Down);
  }
  sprout::launcher::render_profile_archive(renderer, archive);
  (void)archive.handle(Action::Confirm);
  if (archive.notice_is_error() || archive.notice() != "EXPORTED Alex") {
    std::cerr << "Profile archive smoke test did not export the child profile\n";
    return EXIT_FAILURE;
  }
  sprout::launcher::render_profile_archive(renderer, archive);
  return EXIT_SUCCESS;
}

std::vector<sprout::launcher::Profile> load_launcher_profiles(
    const sprout::launcher::ProfileRepository& repository) {
  std::vector<sprout::launcher::Profile> profiles;
  for (const auto& stored : repository.list_profiles(false)) {
    profiles.push_back(sprout::launcher::Profile{
        .id = stored.id,
        .display_name = stored.display_name,
        .role = stored.role,
        .accent_rgb = stored.role == sprout::launcher::ProfileRole::Child
                          ? 0x70B77EU
                          : 0x8E7DBEU,
        .avatar_ref = stored.avatar_ref,
    });
  }
  std::stable_sort(profiles.begin(), profiles.end(), [](const auto& left, const auto& right) {
    return left.role == sprout::launcher::ProfileRole::Child &&
           right.role == sprout::launcher::ProfileRole::Parent;
  });
  if (profiles.empty()) {
    throw std::runtime_error("Completed setup has no active profiles");
  }
  return profiles;
}

int save_screenshot(SDL_Renderer* renderer, const char* path) {
  SDL_Surface* surface =
      SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_ARGB8888);
  if (surface == nullptr) {
    std::cerr << "Screenshot surface creation failed: " << SDL_GetError() << '\n';
    return EXIT_FAILURE;
  }

  const int read_result =
      SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels,
                           surface->pitch);
  const int save_result = read_result == 0 ? SDL_SaveBMP(surface, path) : -1;
  if (read_result != 0 || save_result != 0) {
    std::cerr << "Screenshot failed: " << SDL_GetError() << '\n';
    SDL_FreeSurface(surface);
    return EXIT_FAILURE;
  }

  SDL_FreeSurface(surface);
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char* argv[]) {
  bool smoke_test = false;
  bool arcade_smoke_test = false;
  std::optional<std::string> arcade_smoke_item;
  bool screenshot = false;
  const char* screenshot_path = nullptr;
  std::string_view screenshot_screen;
  std::optional<std::filesystem::path> data_root;
  std::optional<std::filesystem::path> sd_card_root;
  std::optional<std::filesystem::path> arcade_root;
  std::optional<std::filesystem::path> runtime_executable;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--smoke-test") {
      smoke_test = true;
    } else if (argument == "--arcade-smoke-test") {
      arcade_smoke_test = true;
    } else if (argument == "--arcade-smoke-item" && index + 1 < argc) {
      arcade_smoke_item = argv[++index];
    } else if (argument == "--screenshot" && index + 1 < argc) {
      screenshot = true;
      screenshot_path = argv[++index];
      if (index + 1 < argc &&
          !sprout::launcher::starts_with(argv[index + 1], "--")) {
        screenshot_screen = argv[++index];
      }
    } else if (argument == "--data-dir" && index + 1 < argc) {
      data_root = std::filesystem::path(argv[++index]);
    } else if (argument == "--sd-root" && index + 1 < argc) {
      sd_card_root = std::filesystem::path(argv[++index]);
    } else if (argument == "--arcade-root" && index + 1 < argc) {
      arcade_root = std::filesystem::path(argv[++index]);
    } else if (argument == "--runtime" && index + 1 < argc) {
      runtime_executable = std::filesystem::path(argv[++index]);
    } else {
      std::cerr << "Unsupported or incomplete launcher argument: " << argument << '\n';
      return EXIT_FAILURE;
    }
  }

  if (arcade_root.has_value() != runtime_executable.has_value() ||
      (arcade_root.has_value() && !data_root.has_value())) {
    std::cerr << "Arcade preview requires --arcade-root, --runtime, and --data-dir together\n";
    return EXIT_FAILURE;
  }

  if (arcade_smoke_test) {
    if (!arcade_root.has_value() || !runtime_executable.has_value() ||
        !data_root.has_value()) {
      std::cerr << "Arcade smoke test requires --arcade-root, --runtime, and --data-dir\n";
      return EXIT_FAILURE;
    }
    auto entries = discovered_native_library(*arcade_root);
    if (entries.empty()) {
      std::cerr << "Arcade smoke test found no valid native packages\n";
      return EXIT_FAILURE;
    }
    auto selected = entries.begin();
    if (arcade_smoke_item.has_value()) {
      selected = std::find_if(
          entries.begin(), entries.end(),
          [&](const auto& entry) { return entry.id == *arcade_smoke_item; });
      if (selected == entries.end()) {
        std::cerr << "Arcade smoke test did not find requested item: "
                  << *arcade_smoke_item << '\n';
        return EXIT_FAILURE;
      }
    }
    auto target = std::get<sprout::launcher::NativeLaunchTarget>(
        selected->launch_target);
    target.profile_id = "diagnostic-child";
    target.seed = 1;
    target.launch_allowed = true;
    sprout::launcher::SystemLaunchProcess process;
    sprout::launcher::NativeLaunchAdapter adapter(
        *runtime_executable, *data_root / "data" / "native-games", process);
    const auto result = adapter.launch(
        target, sprout::launcher::NativeLaunchMode::SmokeTest);
    std::cout << "arcade smoke result: " << result.item_id << " outcome="
              << static_cast<int>(result.outcome);
    if (result.exit_code.has_value()) {
      std::cout << " exit=" << *result.exit_code;
    }
    if (!result.detail.empty()) {
      std::cout << " detail=" << result.detail;
    }
    std::cout << '\n';
    return result.completed() ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  if (arcade_smoke_item.has_value()) {
    std::cerr << "--arcade-smoke-item requires --arcade-smoke-test\n";
    return EXIT_FAILURE;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
    std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
    return EXIT_FAILURE;
  }

  const std::uint32_t window_flags =
      smoke_test || screenshot ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
  SDL_Window* window = SDL_CreateWindow(
      "Sprout Launcher Preview", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
      window_flags);
  if (window == nullptr) {
    std::cerr << "Window creation failed: " << SDL_GetError() << '\n';
    SDL_Quit();
    return EXIT_FAILURE;
  }

  SDL_Renderer* renderer = create_renderer(window);
  if (renderer == nullptr) {
    std::cerr << "Renderer creation failed: " << SDL_GetError() << '\n';
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_FAILURE;
  }
  SDL_RenderSetLogicalSize(renderer, 640, 480);
  const auto executable_root =
      std::filesystem::absolute(argv[0]).parent_path();
  const auto startup_splash = executable_root / "assets" / "startup-splash.png";
  const auto launcher_background =
      executable_root / "assets" / "launcher-family.png";
  const auto launcher_accents =
      executable_root / "assets" / "sprout-menu-particles-atlas.png";
  const auto built_in_avatar_root = executable_root / "assets" / "avatars";

  if (smoke_test) {
    const int result = run_smoke_test(renderer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
  }

  if (screenshot) {
    if (screenshot_screen == "startup") {
      if (!sprout::launcher::render_startup_splash(renderer, startup_splash)) {
        std::cerr << "Startup splash could not be loaded from "
                  << startup_splash << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
      }
      const int result = save_screenshot(renderer, screenshot_path);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return result;
    }
    if (screenshot_screen == "recovery") {
      TemporaryDirectory directory("recovery-screenshot");
      sprout::launcher::ConfigurationStore configuration(
          directory.path() / "config");
      auto first = configuration.save(sprout::launcher::LocalConfiguration{});
      first.next_setup_step = sprout::launcher::SetupStep::Parent;
      (void)configuration.save(first);
      sprout::launcher::RecoveryPresentation recovery(configuration, 4);
      sprout::launcher::render_recovery(renderer, recovery);
      const int result = save_screenshot(renderer, screenshot_path);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return result;
    }
    if (screenshot_screen == "pin-create" || screenshot_screen == "pin-auth") {
      sprout::launcher::ParentPinPresentation pin(
          screenshot_screen == "pin-create"
              ? sprout::launcher::ParentPinMode::Create
              : sprout::launcher::ParentPinMode::Authenticate);
      (void)pin.handle(Action::Confirm);
      (void)pin.handle(Action::Right);
      (void)pin.handle(Action::Confirm);
      sprout::launcher::render_parent_pin(renderer, pin);
      const int result = save_screenshot(renderer, screenshot_path);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return result;
    }
    if (screenshot_screen == "crop" || screenshot_screen == "profile-image") {
      if (!data_root.has_value()) {
        std::cerr << "Profile image screenshots require --data-dir with an imports folder\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
      }
      const auto source = pending_profile_image(*data_root);
      if (!source.has_value()) {
        std::cerr << "No imports/profile-image PNG, JPEG, or BMP was found\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
      }
      TemporaryDirectory directory("profile-image-screenshot");
      sprout::launcher::ProfileRepository profiles(directory.path() / "profiles.sqlite3");
      profiles.create_profile(sprout::launcher::NewProfile{
          .id = "parent-primary",
          .display_name = "Parent",
          .role = sprout::launcher::ProfileRole::Parent,
          .avatar_ref = "builtin:fox",
          .save_namespace = "saves-parent-primary",
      });
      const auto image_root = directory.path() / "profile-images";
      sprout::launcher::ProfileImageImporter importer(image_root, profiles);
      sprout::launcher::ProfileImageCropPresentation crop(
          importer, "parent-primary", *source);
      if (screenshot_screen == "crop") {
        (void)crop.handle(Action::ZoomIn);
        sprout::launcher::render_profile_image_crop(renderer, crop);
      } else {
        (void)crop.handle(Action::Confirm);
        LauncherState state(load_launcher_profiles(profiles));
        sprout::launcher::render_launcher(renderer, state, image_root);
      }
      const int result = save_screenshot(renderer, screenshot_path);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return result;
    }
    if (sprout::launcher::starts_with(screenshot_screen, "setup")) {
      TemporaryDirectory directory("setup-screenshot");
      sprout::launcher::ConfigurationStore configuration(directory.path() / "config");
      sprout::launcher::ProfileRepository profiles(directory.path() / "profiles.sqlite3");
      sprout::launcher::SetupWizard wizard(configuration, profiles);
      const bool show_import = screenshot_screen == "setup-avatars-import";
      sprout::launcher::SetupPresentation setup(wizard, show_import);
      auto target = sprout::launcher::SetupStep::Welcome;
      if (screenshot_screen == "setup-parent") {
        target = sprout::launcher::SetupStep::Parent;
      } else if (screenshot_screen == "setup-pin") {
        target = sprout::launcher::SetupStep::ParentPin;
      } else if (screenshot_screen == "setup-child") {
        target = sprout::launcher::SetupStep::Child;
      } else if (screenshot_screen == "setup-review") {
        target = sprout::launcher::SetupStep::Review;
      } else if (show_import) {
        target = sprout::launcher::SetupStep::Avatars;
      } else if (screenshot_screen != "setup") {
        std::cerr << "Unsupported setup screenshot: " << screenshot_screen << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
      }
      while (setup.step() != target) {
        if (setup.step() == sprout::launcher::SetupStep::ParentPin) {
          (void)setup.handle(Action::Right);
        }
        (void)setup.handle(Action::Confirm);
        if (!setup.error_message().empty() ||
            setup.step() == sprout::launcher::SetupStep::Complete) {
          std::cerr << "Could not reach requested setup screenshot step\n";
          SDL_DestroyRenderer(renderer);
          SDL_DestroyWindow(window);
          SDL_Quit();
          return EXIT_FAILURE;
        }
      }
      sprout::launcher::render_setup(renderer, setup);
      const int result = save_screenshot(renderer, screenshot_path);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return result;
    }
    if (screenshot_screen == "library-recent" ||
        screenshot_screen == "library-favorites" ||
        screenshot_screen == "library-all" ||
        screenshot_screen == "library-arcade") {
      auto section = sprout::launcher::LibrarySection::Recent;
      if (screenshot_screen == "library-favorites") {
        section = sprout::launcher::LibrarySection::Favorites;
      } else if (screenshot_screen == "library-all") {
        section = sprout::launcher::LibrarySection::All;
      } else if (screenshot_screen == "library-arcade") {
        section = sprout::launcher::LibrarySection::Arcade;
      }
      sprout::launcher::LibraryPresentation library(
          section == sprout::launcher::LibrarySection::Arcade &&
                  arcade_root.has_value()
              ? discovered_native_library(*arcade_root)
              : sprout::launcher::make_demo_library(),
          section);
      if (section == sprout::launcher::LibrarySection::All) {
        (void)library.handle(Action::Up);
        (void)library.handle(Action::Confirm);
      }
      sprout::launcher::render_library(renderer, library);
      const int result = save_screenshot(renderer, screenshot_path);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return result;
    }
    if (screenshot_screen == "profile-archive") {
      TemporaryDirectory directory("profile-archive-screenshot");
      sprout::launcher::ProfileRepository profiles(
          directory.path() / "profiles.sqlite3");
      profiles.create_profile(sprout::launcher::NewProfile{
          .id = "child-alex",
          .display_name = "Alex",
          .role = sprout::launcher::ProfileRole::Child,
          .avatar_ref = "builtin:sprout",
          .save_namespace = "saves-child-alex",
          .content_policy_ref = "content:child-default",
          .time_policy_ref = "time:child-default",
      });
      sprout::launcher::DailyTimePolicyStore time_policy(
          directory.path() / "time-policy.sqlite3");
      time_policy.set_daily_allowance(
          "child-alex", sprout::launcher::kDefaultChildDailyAllowanceSeconds);
      sprout::launcher::ProfileArchiveService archives(
          profiles, time_policy, directory.path() / "profile-images");
      sprout::launcher::ProfileArchivePresentation archive(
          profiles, archives, directory.path() / "exports",
          directory.path() / "imports");
      sprout::launcher::render_profile_archive(renderer, archive);
      const int result = save_screenshot(renderer, screenshot_path);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return result;
    }
    if (screenshot_screen == "profile-avatars" ||
        screenshot_screen == "profile-avatars-new" ||
        screenshot_screen == "profile-avatars-last") {
      TemporaryDirectory directory("profile-avatars-screenshot");
      sprout::launcher::ProfileRepository profiles(
          directory.path() / "profiles.sqlite3");
      profiles.create_profile(sprout::launcher::NewProfile{
          .id = "parent-primary",
          .display_name = "Parent",
          .role = sprout::launcher::ProfileRole::Parent,
          .avatar_ref = "builtin:explorer-fox",
          .save_namespace = "saves-parent-primary",
      });
      profiles.create_profile(sprout::launcher::NewProfile{
          .id = "child-alex",
          .display_name = "Alex",
          .role = sprout::launcher::ProfileRole::Child,
          .avatar_ref = "builtin:friendly-dragon",
          .save_namespace = "saves-child-alex",
          .content_policy_ref = "content:child-default",
          .time_policy_ref = "time:child-default",
      });
      sprout::launcher::ProfileAvatarPresentation avatars(
          profiles, true, "child-alex");
      if (screenshot_screen == "profile-avatars-new") {
        for (int index = 0; index < 8; ++index) {
          static_cast<void>(avatars.handle(Action::Down));
        }
      } else if (screenshot_screen == "profile-avatars-last") {
        static_cast<void>(avatars.handle(Action::Left));
      }
      sprout::launcher::render_profile_avatars(
          renderer, avatars, built_in_avatar_root);
      const int result = save_screenshot(renderer, screenshot_path);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return result;
    }
    LauncherState state(sprout::launcher::make_demo_household());
    if (screenshot_screen == "child") {
      (void)state.handle(Action::Confirm);
    } else if (screenshot_screen == "parent") {
      (void)state.handle(Action::Right);
      (void)state.handle(Action::Confirm);
    }
    sprout::launcher::render_launcher(renderer, state, {}, launcher_background,
                                      launcher_accents,
                                      built_in_avatar_root);
    const int result = save_screenshot(renderer, screenshot_path);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
  }

  if (sprout::launcher::render_startup_splash(renderer, startup_splash)) {
    SDL_Delay(900);
  }

  SDL_GameController* controller = nullptr;
  for (int index = 0; index < SDL_NumJoysticks(); ++index) {
    if (SDL_IsGameController(index)) {
      controller = SDL_GameControllerOpen(index);
      if (controller != nullptr) {
        break;
      }
    }
  }

  std::unique_ptr<sprout::launcher::ConfigurationStore> configuration;
  std::unique_ptr<sprout::launcher::StartupHealthStore> startup_health;
  std::unique_ptr<sprout::launcher::RecoveryPresentation> recovery;
  std::unique_ptr<sprout::launcher::ProfileRepository> profiles;
  std::unique_ptr<sprout::launcher::DailyTimePolicyStore> daily_time_policy;
  std::unique_ptr<sprout::launcher::ProfileArchiveService> profile_archives;
  std::unique_ptr<sprout::launcher::ProfileArchivePresentation> archive;
  std::unique_ptr<sprout::launcher::SetupWizard> wizard;
  std::unique_ptr<sprout::launcher::SetupPresentation> setup;
  std::unique_ptr<sprout::launcher::ProfileImageImporter> image_importer;
  std::unique_ptr<sprout::launcher::ProfileImageCropPresentation> image_crop;
  std::unique_ptr<sprout::launcher::ProfileAvatarPresentation> profile_avatars;
  std::unique_ptr<sprout::launcher::ParentAccessStore> parent_access;
  std::unique_ptr<sprout::launcher::ParentPinPresentation> parent_pin;
  std::unique_ptr<sprout::launcher::ParentAccessController> access_controller;
  std::unique_ptr<sprout::launcher::LibraryPresentation> library;
  sprout::launcher::SystemLaunchProcess launch_process;
  std::unique_ptr<sprout::launcher::NativeLaunchAdapter> native_launch;
  std::vector<sprout::launcher::LibraryEntry> library_entries;
  std::optional<std::string> parent_credential_ref;
  std::optional<std::filesystem::path> image_source;
  bool image_crop_advances_setup{false};
  std::optional<LauncherState> state;
  std::optional<std::uint64_t> startup_attempt_id;

  const auto initialize_persistent_launcher = [&] {
    profiles = std::make_unique<sprout::launcher::ProfileRepository>(
        *data_root / "data" / "profiles.sqlite3");
    daily_time_policy =
        std::make_unique<sprout::launcher::DailyTimePolicyStore>(
            *data_root / "data" / "time-policy.sqlite3");
    for (const auto& profile : profiles->list_profiles(false)) {
      if (profile.role == sprout::launcher::ProfileRole::Child &&
          profile.time_policy_ref == "time:child-default" &&
          !daily_time_policy->find_daily_allowance_seconds(profile.id)
               .has_value()) {
        daily_time_policy->set_daily_allowance(
            profile.id,
            sprout::launcher::kDefaultChildDailyAllowanceSeconds);
      }
    }
    image_importer = std::make_unique<sprout::launcher::ProfileImageImporter>(
        *data_root / "data" / "profile-images", *profiles);
    profile_archives =
        std::make_unique<sprout::launcher::ProfileArchiveService>(
            *profiles, *daily_time_policy,
            *data_root / "data" / "profile-images");
    parent_access = std::make_unique<sprout::launcher::ParentAccessStore>(
        *data_root / "data" / "security.sqlite3",
        *data_root / "secrets" / "device-access.key");
    image_source = pending_profile_image(*data_root);
    wizard = std::make_unique<sprout::launcher::SetupWizard>(
        *configuration, *profiles, daily_time_policy.get());
    parent_credential_ref = wizard->configuration().parent_credential_ref;
    if (wizard->current_step() == sprout::launcher::SetupStep::Complete) {
      state.emplace(load_launcher_profiles(*profiles));
      setup.reset();
    } else {
      state.reset();
      setup = std::make_unique<sprout::launcher::SetupPresentation>(
          *wizard, image_source.has_value());
    }
    if (state.has_value()) {
      access_controller =
          std::make_unique<sprout::launcher::ParentAccessController>(
              *state, parent_access.get(), parent_credential_ref);
    } else {
      access_controller.reset();
    }
  };

  try {
    library_entries = sd_card_root.has_value()
                          ? discovered_library(*sd_card_root)
                          : sprout::launcher::make_demo_library();
    if (arcade_root.has_value()) {
      auto native_entries = discovered_native_library(*arcade_root);
      library_entries.insert(library_entries.end(),
                             std::make_move_iterator(native_entries.begin()),
                             std::make_move_iterator(native_entries.end()));
      native_launch = std::make_unique<sprout::launcher::NativeLaunchAdapter>(
          *runtime_executable, *data_root / "data" / "native-games",
          launch_process);
    }
    if (data_root.has_value()) {
      std::filesystem::create_directories(*data_root / "data");
      startup_health =
          std::make_unique<sprout::launcher::StartupHealthStore>(
              *data_root / "data" / "startup-health.sqlite3");
      const auto startup = startup_health->begin_startup();
      startup_attempt_id = startup.attempt_id;
      configuration = std::make_unique<sprout::launcher::ConfigurationStore>(
          *data_root / "config");
      if (startup.recovery_required) {
        recovery =
            std::make_unique<sprout::launcher::RecoveryPresentation>(
                *configuration, startup.attempt_id);
      } else {
        initialize_persistent_launcher();
      }
    } else {
      state.emplace(sprout::launcher::make_demo_household());
      access_controller =
          std::make_unique<sprout::launcher::ParentAccessController>(
              *state, parent_access.get(), parent_credential_ref);
    }
  } catch (const std::exception& error) {
    std::cerr << "Launcher data could not be opened: " << error.what() << '\n';
    if (controller != nullptr) {
      SDL_GameControllerClose(controller);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_FAILURE;
  }
  bool running = true;
  bool dirty = true;
  const auto handle_session_action = [&](Action action) {
    try {
      if (recovery != nullptr) {
        const auto recovery_event = recovery->handle(action);
        if (recovery_event ==
            sprout::launcher::RecoveryPresentationEvent::ExitRequested) {
          return false;
        }
        if (recovery_event ==
            sprout::launcher::RecoveryPresentationEvent::ConfigurationChanged) {
          recovery.reset();
          initialize_persistent_launcher();
        }
        return true;
      }
      if (parent_pin != nullptr) {
        const auto pin_event = parent_pin->handle(action);
        if (pin_event == sprout::launcher::ParentPinEvent::Cancelled) {
          parent_pin.reset();
          return true;
        }
        if (pin_event != sprout::launcher::ParentPinEvent::Submitted) {
          return true;
        }

        std::string pin = parent_pin->take_pin();
        constexpr std::string_view kCredentialRef = "secret:parent-primary";
        parent_access->set_pin(std::string(kCredentialRef), std::move(pin));
        parent_credential_ref = kCredentialRef;
        setup->complete_parent_pin_step(std::string(kCredentialRef));
        parent_pin.reset();
        return true;
      }
      if (image_crop != nullptr) {
        const auto crop_event = image_crop->handle(action);
        if (crop_event == sprout::launcher::ProfileImageCropEvent::Cancelled) {
          image_crop.reset();
        } else if (crop_event == sprout::launcher::ProfileImageCropEvent::Imported) {
          image_crop.reset();
          if (image_crop_advances_setup && setup != nullptr) {
            setup->complete_avatar_step();
          } else {
            profile_avatars.reset();
            state.emplace(load_launcher_profiles(*profiles));
            access_controller =
                std::make_unique<sprout::launcher::ParentAccessController>(
                    *state, parent_access.get(), parent_credential_ref);
          }
          image_crop_advances_setup = false;
        }
        return true;
      }
      if (profile_avatars != nullptr) {
        const auto avatar_event = profile_avatars->handle(action);
        if (!avatar_event.has_value()) return true;
        if (avatar_event->type ==
            sprout::launcher::ProfileAvatarEventType::BackRequested) {
          profile_avatars.reset();
          return true;
        }
        if (avatar_event->type ==
            sprout::launcher::ProfileAvatarEventType::ImportRequested) {
          if (!image_source.has_value()) return true;
          image_crop =
              std::make_unique<sprout::launcher::ProfileImageCropPresentation>(
                  *image_importer, avatar_event->profile_id, *image_source);
          image_crop_advances_setup = false;
          return true;
        }
        profile_avatars.reset();
        state.emplace(load_launcher_profiles(*profiles));
        access_controller =
            std::make_unique<sprout::launcher::ParentAccessController>(
                *state, parent_access.get(), parent_credential_ref);
        return true;
      }
      if (setup != nullptr) {
        const auto setup_event = setup->handle(action);
        if (setup_event == sprout::launcher::SetupPresentationEvent::ExitRequested) {
          return false;
        }
        if (setup_event == sprout::launcher::SetupPresentationEvent::Completed) {
          parent_credential_ref = wizard->configuration().parent_credential_ref;
          state.emplace(load_launcher_profiles(*profiles));
          access_controller =
              std::make_unique<sprout::launcher::ParentAccessController>(
                  *state, parent_access.get(), parent_credential_ref);
          setup.reset();
        } else if (setup_event ==
                   sprout::launcher::SetupPresentationEvent::ConfigureParentPinRequested) {
          parent_pin = std::make_unique<sprout::launcher::ParentPinPresentation>(
              sprout::launcher::ParentPinMode::Create);
        } else if (setup_event ==
                   sprout::launcher::SetupPresentationEvent::ImportParentImageRequested) {
          try {
            image_crop = std::make_unique<sprout::launcher::ProfileImageCropPresentation>(
                *image_importer, "parent-primary", *image_source);
            image_crop_advances_setup = true;
          } catch (const std::exception& error) {
            setup->report_avatar_error(error.what());
          }
        } else if (setup_event ==
                       sprout::launcher::SetupPresentationEvent::ChooseParentAvatarRequested ||
                   setup_event ==
                       sprout::launcher::SetupPresentationEvent::ChooseChildAvatarRequested) {
          try {
            profile_avatars =
                std::make_unique<sprout::launcher::ProfileAvatarPresentation>(
                    *profiles, image_source.has_value(),
                    setup_event == sprout::launcher::SetupPresentationEvent::ChooseParentAvatarRequested
                        ? "parent-primary"
                        : "child-primary");
          } catch (const std::exception& error) {
            setup->report_avatar_error(error.what());
          }
        }
        return true;
      }

      if (library != nullptr) {
        if (!access_controller->ensure_active_profile_access(
                current_access_time())) {
          library.reset();
          return true;
        }
        const auto library_event = library->handle(action);
        if (!library_event.has_value()) {
          return true;
        }
        if (library_event->type ==
            sprout::launcher::LibraryPresentationEventType::BackRequested) {
          library.reset();
        } else if (library_event->type ==
                       sprout::launcher::LibraryPresentationEventType::LaunchRequested &&
                   library_event->launch_target.has_value()) {
          if (std::holds_alternative<sprout::launcher::NativeLaunchTarget>(
                  *library_event->launch_target)) {
            if (native_launch == nullptr || state->active_profile() == nullptr) {
              library->report_launch_result("SPROUT RUNTIME IS UNAVAILABLE");
              return true;
            }
            auto target = std::get<sprout::launcher::NativeLaunchTarget>(
                *library_event->launch_target);
            const auto* active_profile = state->active_profile();
            std::optional<std::string> policy_session;
            if (active_profile->role == sprout::launcher::ProfileRole::Child &&
                daily_time_policy != nullptr) {
              const auto sample = current_time_policy_sample();
              policy_session = "native-" + active_profile->id + "-" +
                               std::to_string(sample.monotonic_milliseconds);
              const auto decision = daily_time_policy->begin_session(
                  active_profile->id, *policy_session, target.item_id, sample);
              if (!decision.launch_allowed) {
                library->report_launch_result("DAILY PLAY TIME IS USED UP");
                return true;
              }
            }

            SDL_HideWindow(window);
            const auto launch_result = native_launch->launch(target);
            SDL_ShowWindow(window);
            SDL_RaiseWindow(window);
            SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
            std::cout << "native launch result: " << launch_result.item_id
                      << " outcome="
                      << static_cast<int>(launch_result.outcome);
            if (launch_result.exit_code.has_value()) {
              std::cout << " exit=" << *launch_result.exit_code;
            }
            if (!launch_result.detail.empty()) {
              std::cout << " detail=" << launch_result.detail;
            }
            std::cout << '\n';
            if (policy_session.has_value()) {
              const auto decision = daily_time_policy->pause_session(
                  *policy_session, current_time_policy_sample());
              if (decision.status.expired) {
                library->report_launch_result("DAILY PLAY TIME IS USED UP");
                return true;
              }
            }
            library->report_launch_result(
                launch_result.completed() ? "RETURNED TO SPROUT"
                                          : "GAME COULD NOT START: " +
                                                launch_result.detail);
          } else {
            const auto& target =
                std::get<sprout::launcher::EmulatedLaunchTarget>(
                    *library_event->launch_target);
            std::cout << "preview launch request: " << target.item_id << " ("
                      << target.rom_path.string() << ")\n";
          }
        }
        return true;
      }

      if (archive != nullptr) {
        if (!access_controller->ensure_active_profile_access(
                current_access_time())) {
          archive.reset();
          return true;
        }
        const auto archive_event = archive->handle(action);
        if (!archive_event.has_value()) {
          return true;
        }
        if (*archive_event ==
            sprout::launcher::ProfileArchivePresentationEvent::BackRequested) {
          archive.reset();
        } else {
          archive.reset();
          state.emplace(load_launcher_profiles(*profiles));
          access_controller =
              std::make_unique<sprout::launcher::ParentAccessController>(
                  *state, parent_access.get(), parent_credential_ref);
        }
        return true;
      }

      const auto session_event =
          access_controller->handle(action, current_access_time());
      if (!session_event.has_value()) {
        return true;
      }
      if (session_event->type ==
          sprout::launcher::ParentAccessEventType::ExitRequested) {
        return false;
      }
      const auto section = sprout::launcher::library_section_for_menu_target(
          session_event->target);
      if (section.has_value()) {
        auto entries = library_entries;
        const auto* active_profile = state->active_profile();
        if (active_profile != nullptr) {
          const bool child =
              active_profile->role == sprout::launcher::ProfileRole::Child;
          bool time_expired = false;
          if (child && daily_time_policy != nullptr) {
            time_expired =
                daily_time_policy
                    ->status(active_profile->id, current_time_policy_sample())
                    .expired;
          }
          for (auto& entry : entries) {
            std::visit(
                [&](auto& target) {
                  target.launch_allowed = entry.launch_allowed;
                  if constexpr (std::is_same_v<
                                    std::decay_t<decltype(target)>,
                                    sprout::launcher::NativeLaunchTarget>) {
                    target.profile_id = active_profile->id;
                    const auto sample = current_time_policy_sample();
                    target.seed =
                        sample.monotonic_milliseconds ^
                        static_cast<std::uint64_t>(sample.utc_seconds);
                    if (target.seed == 0) target.seed = 1;
                  }
                },
                entry.launch_target);
            if (child && !entry.child_visible) {
              entry.launch_allowed = false;
              entry.unavailable_reason =
                  "PARENT APPROVAL IS NOT CONFIGURED FOR THIS GAME";
            } else if (child && time_expired) {
              entry.launch_allowed = false;
              entry.unavailable_reason = "DAILY PLAY TIME IS USED UP";
            }
          }
        }
        library = std::make_unique<sprout::launcher::LibraryPresentation>(
            std::move(entries), *section);
        return true;
      }
      if (session_event->target == "Backup & Restore" &&
          profile_archives != nullptr && data_root.has_value()) {
        archive =
            std::make_unique<sprout::launcher::ProfileArchivePresentation>(
                *profiles, *profile_archives, *data_root / "exports",
                *data_root / "imports");
        return true;
      }
      if (session_event->target == "Profile Settings" && profiles != nullptr) {
        profile_avatars =
            std::make_unique<sprout::launcher::ProfileAvatarPresentation>(
                *profiles, image_source.has_value());
        return true;
      }
      std::cout << "preview action: " << session_event->target << " ("
                << session_event->profile_id << ")\n";
      return true;
    } catch (const std::exception& error) {
      std::cerr << "Launcher session failed safely: " << error.what() << '\n';
      return false;
    }
  };

  while (running) {
    SDL_Event event{};
    if (SDL_WaitEventTimeout(&event, 16) != 0) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        const auto action = keyboard_action(event.key.keysym.sym);
        if (action.has_value()) {
          running = handle_session_action(*action);
          dirty = true;
        }
      } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        const auto action = controller_action(event.cbutton.button);
        if (action.has_value()) {
          running = handle_session_action(*action);
          dirty = true;
        }
      }
    }

    if (dirty) {
      if (recovery != nullptr) {
        sprout::launcher::render_recovery(renderer, *recovery);
      } else if (parent_pin != nullptr) {
        sprout::launcher::render_parent_pin(renderer, *parent_pin);
      } else if (access_controller != nullptr &&
                 access_controller->has_pin_prompt()) {
        sprout::launcher::render_parent_pin(renderer,
                                            access_controller->pin_prompt());
      } else if (image_crop != nullptr) {
        sprout::launcher::render_profile_image_crop(renderer, *image_crop);
      } else if (profile_avatars != nullptr) {
        sprout::launcher::render_profile_avatars(
            renderer, *profile_avatars, built_in_avatar_root);
      } else if (setup != nullptr) {
        sprout::launcher::render_setup(renderer, *setup);
      } else if (library != nullptr) {
        sprout::launcher::render_library(renderer, *library);
      } else if (archive != nullptr) {
        sprout::launcher::render_profile_archive(renderer, *archive);
      } else {
        sprout::launcher::render_launcher(
            renderer, *state,
            data_root.has_value() ? *data_root / "data" / "profile-images"
                                  : std::filesystem::path{},
            launcher_background, launcher_accents, built_in_avatar_root);
      }
      if (recovery == nullptr && startup_attempt_id.has_value()) {
        try {
          startup_health->mark_ready(*startup_attempt_id);
          startup_attempt_id.reset();
        } catch (const std::exception& error) {
          std::cerr << "Launcher readiness could not be recorded: "
                    << error.what() << '\n';
          running = false;
        }
      }
      dirty = false;
    }
  }

  if (controller != nullptr) {
    SDL_GameControllerClose(controller);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return EXIT_SUCCESS;
}
