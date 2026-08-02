#include "sprout/launcher/local_configuration.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using sprout::launcher::ConfigurationStore;
using sprout::launcher::LocalConfiguration;
using sprout::launcher::LocaleOverrides;
using sprout::launcher::ResolvedLocale;
using sprout::launcher::SetupStep;

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Operation>
void expect_failure(Operation operation, std::string_view message) {
  try {
    operation();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto suffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sprout-configuration-test-" + std::to_string(suffix));
  }

  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

std::string read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void overwrite_file(const std::filesystem::path& path, std::string_view contents) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream << contents;
}

void saves_and_reloads_versioned_configuration() {
  TemporaryDirectory directory;
  ConfigurationStore store(directory.path());
  LocalConfiguration configuration;
  configuration.next_setup_step = SetupStep::Locale;
  configuration.household_locale.language = "en";
  configuration.parent_credential_ref = "secret:parent-pin";

  const auto saved = store.save(configuration);
  expect(saved.revision == 1, "first save should establish revision one");
  expect(store.has_active(), "first save should create active configuration");
  expect(!store.has_last_known_good(),
         "first save should not invent a last-known-good snapshot");

  const auto loaded = ConfigurationStore(directory.path()).load_active();
  expect(loaded.schema_version == 1, "schema version should survive reload");
  expect(loaded.next_setup_step == SetupStep::Locale,
         "setup progress should survive reload");
  expect(loaded.parent_credential_ref == "secret:parent-pin",
         "only the opaque credential reference should survive reload");
}

void resolves_configuration_in_scope_order() {
  LocalConfiguration configuration;
  configuration.household_locale = LocaleOverrides{
      .language = "fr",
      .region = "CA",
  };
  configuration.device_locale.time_zone = "America/Toronto";
  configuration.profile_locales["child-alex"].language = "en";

  const auto resolved = sprout::launcher::resolve_locale(
      ResolvedLocale{.language = "en", .region = "US", .time_zone = "UTC"},
      configuration, std::string("child-alex"));
  expect(resolved.language == "en", "profile should override household language");
  expect(resolved.region == "CA", "household should override platform region");
  expect(resolved.time_zone == "America/Toronto",
         "device should override platform time zone");
}

void snapshots_and_restores_last_known_good() {
  TemporaryDirectory directory;
  ConfigurationStore store(directory.path());
  auto first = store.save(LocalConfiguration{});
  first.next_setup_step = SetupStep::Network;
  const auto second = store.save(first);
  expect(second.revision == 2, "second save should advance revision");
  expect(store.has_last_known_good(), "second save should snapshot revision one");

  overwrite_file(store.active_path(), "{broken json");
  expect_failure([&] { (void)store.load_active(); },
                 "corrupt active configuration should fail closed");
  const auto restored = store.restore_last_known_good();
  expect(restored.revision == 1, "rollback should restore the exact prior revision");
  expect(store.load_active().next_setup_step == SetupStep::Welcome,
         "rollback should reactivate the prior setup state");
}

void rejects_stale_and_invalid_writes() {
  TemporaryDirectory directory;
  ConfigurationStore store(directory.path());
  auto original = store.save(LocalConfiguration{});
  auto stale = original;
  original.next_setup_step = SetupStep::Locale;
  (void)store.save(original);

  stale.next_setup_step = SetupStep::Parent;
  expect_failure([&] { (void)store.save(stale); },
                 "stale configuration writer should be rejected");

  auto invalid = store.load_active();
  invalid.parent_credential_ref = "1234";
  expect_failure([&] { (void)store.save(invalid); },
                 "raw credential material should not be accepted as a reference");
}

void rejects_newer_and_unknown_json_without_mutation() {
  TemporaryDirectory directory;
  ConfigurationStore store(directory.path());
  (void)store.save(LocalConfiguration{});
  std::string newer = read_file(store.active_path());
  const auto version = newer.find("\"schemaVersion\": 1");
  expect(version != std::string::npos, "fixture should contain schema version");
  newer.replace(version, std::string("\"schemaVersion\": 1").size(),
                "\"schemaVersion\": 99");
  overwrite_file(store.active_path(), newer);
  expect_failure([&] { (void)store.load_active(); },
                 "newer schema should be rejected");
  expect(read_file(store.active_path()) == newer,
         "rejected newer schema should not be modified");

  std::string unknown = newer;
  const auto object = unknown.find('{');
  unknown.insert(object + 1, "\n  \"unexpected\": true,");
  overwrite_file(store.active_path(), unknown);
  expect_failure([&] { (void)store.load_active(); },
                 "unknown same-document field should be rejected");
}

void leaves_no_pending_file_after_activation() {
  TemporaryDirectory directory;
  ConfigurationStore store(directory.path());
  (void)store.save(LocalConfiguration{});
  auto pending = store.active_path();
  pending += ".pending";
  expect(!std::filesystem::exists(pending),
         "successful activation should not leave a pending file");
}

}  // namespace

int main() {
  try {
    saves_and_reloads_versioned_configuration();
    resolves_configuration_in_scope_order();
    snapshots_and_restores_last_known_good();
    rejects_stale_and_invalid_writes();
    rejects_newer_and_unknown_json_without_mutation();
    leaves_no_pending_file_after_activation();
  } catch (const std::exception& error) {
    std::cerr << "local configuration test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "local configuration tests passed\n";
  return EXIT_SUCCESS;
}
