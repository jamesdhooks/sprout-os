#include "desktop_view.hpp"
#include "sprout/launcher/local_configuration.hpp"
#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/profile_repository.hpp"
#include "sprout/launcher/profile_image_crop_presentation.hpp"
#include "sprout/launcher/profile_image_importer.hpp"
#include "sprout/launcher/setup_presentation.hpp"
#include "sprout/launcher/setup_wizard.hpp"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using sprout::launcher::Action;
using sprout::launcher::EventType;
using sprout::launcher::LauncherEvent;
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

bool apply_action(LauncherState& state, Action action) {
  const auto event = state.handle(action);
  if (!event.has_value()) {
    return true;
  }

  if (event->type == EventType::ExitRequested) {
    return false;
  }

  if (event->type == EventType::MenuItemInvoked) {
    std::cout << "preview action: " << event->target << " (" << event->profile_id << ")\n";
  }
  return true;
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
  std::filesystem::create_directories(directory.path() / "data");
  sprout::launcher::ConfigurationStore configuration(directory.path() / "config");
  sprout::launcher::ProfileRepository profiles(directory.path() / "data" /
                                                "profiles.sqlite3");
  sprout::launcher::SetupWizard wizard(configuration, profiles);
  sprout::launcher::SetupPresentation setup(wizard);
  while (setup.step() != sprout::launcher::SetupStep::Complete) {
    sprout::launcher::render_setup(renderer, setup);
    (void)setup.handle(Action::Confirm);
    if (!setup.error_message().empty()) {
      std::cerr << "Setup smoke test failed: " << setup.error_message() << '\n';
      return EXIT_FAILURE;
    }
  }
  LauncherState persisted(load_launcher_profiles(profiles));
  sprout::launcher::render_launcher(renderer, persisted);
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
  bool screenshot = false;
  const char* screenshot_path = nullptr;
  std::string_view screenshot_screen;
  std::optional<std::filesystem::path> data_root;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--smoke-test") {
      smoke_test = true;
    } else if (argument == "--screenshot" && index + 1 < argc) {
      screenshot = true;
      screenshot_path = argv[++index];
      if (index + 1 < argc &&
          !std::string_view(argv[index + 1]).starts_with("--")) {
        screenshot_screen = argv[++index];
      }
    } else if (argument == "--data-dir" && index + 1 < argc) {
      data_root = std::filesystem::path(argv[++index]);
    } else {
      std::cerr << "Unsupported or incomplete launcher argument: " << argument << '\n';
      return EXIT_FAILURE;
    }
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

  if (smoke_test) {
    const int result = run_smoke_test(renderer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
  }

  if (screenshot) {
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
    if (screenshot_screen.starts_with("setup")) {
      TemporaryDirectory directory("setup-screenshot");
      sprout::launcher::ConfigurationStore configuration(directory.path() / "config");
      sprout::launcher::ProfileRepository profiles(directory.path() / "profiles.sqlite3");
      sprout::launcher::SetupWizard wizard(configuration, profiles);
      const bool show_import = screenshot_screen == "setup-avatars-import";
      sprout::launcher::SetupPresentation setup(wizard, show_import);
      auto target = sprout::launcher::SetupStep::Welcome;
      if (screenshot_screen == "setup-parent") {
        target = sprout::launcher::SetupStep::Parent;
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
    LauncherState state(sprout::launcher::make_demo_household());
    if (screenshot_screen == "child") {
      (void)state.handle(Action::Confirm);
    } else if (screenshot_screen == "parent") {
      (void)state.handle(Action::Right);
      (void)state.handle(Action::Confirm);
    }
    sprout::launcher::render_launcher(renderer, state);
    const int result = save_screenshot(renderer, screenshot_path);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
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
  std::unique_ptr<sprout::launcher::ProfileRepository> profiles;
  std::unique_ptr<sprout::launcher::SetupWizard> wizard;
  std::unique_ptr<sprout::launcher::SetupPresentation> setup;
  std::unique_ptr<sprout::launcher::ProfileImageImporter> image_importer;
  std::unique_ptr<sprout::launcher::ProfileImageCropPresentation> image_crop;
  std::optional<std::filesystem::path> image_source;
  std::optional<LauncherState> state;
  try {
    if (data_root.has_value()) {
      std::filesystem::create_directories(*data_root / "data");
      configuration = std::make_unique<sprout::launcher::ConfigurationStore>(
          *data_root / "config");
      profiles = std::make_unique<sprout::launcher::ProfileRepository>(
          *data_root / "data" / "profiles.sqlite3");
      image_importer = std::make_unique<sprout::launcher::ProfileImageImporter>(
          *data_root / "data" / "profile-images", *profiles);
      image_source = pending_profile_image(*data_root);
      wizard = std::make_unique<sprout::launcher::SetupWizard>(*configuration, *profiles);
      if (wizard->current_step() == sprout::launcher::SetupStep::Complete) {
        state.emplace(load_launcher_profiles(*profiles));
      } else {
        setup = std::make_unique<sprout::launcher::SetupPresentation>(
            *wizard, image_source.has_value());
      }
    } else {
      state.emplace(sprout::launcher::make_demo_household());
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
      if (image_crop != nullptr) {
        const auto crop_event = image_crop->handle(action);
        if (crop_event == sprout::launcher::ProfileImageCropEvent::Cancelled) {
          image_crop.reset();
        } else if (crop_event == sprout::launcher::ProfileImageCropEvent::Imported) {
          image_crop.reset();
          setup->complete_avatar_step();
        }
        return true;
      }
      if (setup != nullptr) {
        const auto setup_event = setup->handle(action);
        if (setup_event == sprout::launcher::SetupPresentationEvent::ExitRequested) {
          return false;
        }
        if (setup_event == sprout::launcher::SetupPresentationEvent::Completed) {
          state.emplace(load_launcher_profiles(*profiles));
          setup.reset();
        } else if (setup_event ==
                   sprout::launcher::SetupPresentationEvent::ImportParentImageRequested) {
          try {
            image_crop = std::make_unique<sprout::launcher::ProfileImageCropPresentation>(
                *image_importer, "parent-primary", *image_source);
          } catch (const std::exception& error) {
            setup->report_avatar_error(error.what());
          }
        }
        return true;
      }
      return apply_action(*state, action);
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
      if (image_crop != nullptr) {
        sprout::launcher::render_profile_image_crop(renderer, *image_crop);
      } else if (setup != nullptr) {
        sprout::launcher::render_setup(renderer, *setup);
      } else {
        sprout::launcher::render_launcher(
            renderer, *state,
            data_root.has_value() ? *data_root / "data" / "profile-images"
                                  : std::filesystem::path{});
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
