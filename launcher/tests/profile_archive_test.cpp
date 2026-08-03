#include "sprout/launcher/managed_profile_image.hpp"
#include "sprout/launcher/profile_archive.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using sprout::launcher::DailyTimePolicyStore;
using sprout::launcher::NewProfile;
using sprout::launcher::ProfileArchiveExportOptions;
using sprout::launcher::ProfileArchiveService;
using sprout::launcher::ProfileRepository;
using sprout::launcher::ProfileRole;

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
            ("sprout-profile-archive-test-" + std::to_string(suffix));
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

struct Stores {
  static std::filesystem::path prepare(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    return root;
  }

  explicit Stores(const std::filesystem::path& root)
      : profiles(prepare(root) / "profiles.sqlite3"),
        time_policy(root / "time-policy.sqlite3"),
        archives(profiles, time_policy, root / "profile-images") {}

  ProfileRepository profiles;
  DailyTimePolicyStore time_policy;
  ProfileArchiveService archives;
};

NewProfile child_profile(std::string id = "child-alex",
                         std::string save_namespace = "saves-alex",
                         std::string avatar_ref = "builtin:fox") {
  return NewProfile{
      .id = std::move(id),
      .display_name = "Alex",
      .role = ProfileRole::Child,
      .avatar_ref = std::move(avatar_ref),
      .save_namespace = std::move(save_namespace),
      .content_policy_ref = "content-default",
      .time_policy_ref = "time-default",
      .preferences_json = R"({"theme":"sprout"})",
      .background_ref = "builtin:firefly-evening",
  };
}

std::vector<std::uint8_t> png_header(std::uint32_t size) {
  std::vector<std::uint8_t> bytes{
      0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a,
      0x00, 0x00, 0x00, 0x0d, 'I', 'H', 'D', 'R',
      static_cast<std::uint8_t>((size >> 24U) & 0xffU),
      static_cast<std::uint8_t>((size >> 16U) & 0xffU),
      static_cast<std::uint8_t>((size >> 8U) & 0xffU),
      static_cast<std::uint8_t>(size & 0xffU),
      static_cast<std::uint8_t>((size >> 24U) & 0xffU),
      static_cast<std::uint8_t>((size >> 16U) & 0xffU),
      static_cast<std::uint8_t>((size >> 8U) & 0xffU),
      static_cast<std::uint8_t>(size & 0xffU),
  };
  return bytes;
}

void write_bytes(const std::filesystem::path& path,
                 const std::vector<std::uint8_t>& bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary);
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!stream) {
    throw std::runtime_error("test could not write fixture bytes");
  }
}

std::string read_text(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

void write_text(const std::filesystem::path& path, std::string_view text) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (!stream) {
    throw std::runtime_error("test could not write fixture text");
  }
}

void built_in_child_round_trips_with_daily_allowance() {
  TemporaryDirectory directory;
  const auto export_directory = directory.path() / "exports";
  std::filesystem::create_directories(export_directory);
  Stores source(directory.path() / "source");
  source.profiles.create_profile(child_profile());
  source.time_policy.set_daily_allowance("child-alex", 45 * 60);
  const auto archive_path = export_directory / "alex.sprout-profile";
  const auto exported = source.archives.export_profile("child-alex", archive_path);
  expect(exported.profile_id == "child-alex" &&
             !exported.contains_personal_image,
         "built-in profile export should describe its portable payload");

  Stores target(directory.path() / "target");
  const auto inspected = target.archives.inspect_archive(archive_path);
  expect(inspected.display_name == "Alex" &&
             inspected.role == ProfileRole::Child,
         "archive inspection should validate and summarize without mutation");
  expect(!target.profiles.find_profile("child-alex").has_value(),
         "inspection should not create the profile");
  const auto restored = target.archives.restore_profile(archive_path);
  const auto profile = target.profiles.find_profile(restored.profile_id);
  expect(profile.has_value() && profile->avatar_ref == "builtin:fox" &&
             profile->preferences_json == R"({"theme":"sprout"})" &&
             profile->background_ref == "builtin:firefly-evening",
         "built-in profile fields should round-trip");
  expect(target.time_policy.find_daily_allowance_seconds("child-alex") ==
             45 * 60,
         "child daily allowance should round-trip");
}

void managed_avatar_requires_consent_and_round_trips() {
  TemporaryDirectory directory;
  const auto export_directory = directory.path() / "exports";
  std::filesystem::create_directories(export_directory);
  Stores source(directory.path() / "source");
  source.profiles.create_profile(
      child_profile("child-photo", "saves-photo", "local:abcd-r1"));
  source.time_policy.set_daily_allowance("child-photo", 30 * 60);
  const auto source_paths = sprout::launcher::resolve_managed_profile_image(
      directory.path() / "source" / "profile-images", "local:abcd-r1");
  const auto portrait = png_header(256);
  const auto thumbnail = png_header(96);
  write_bytes(source_paths.portrait, portrait);
  write_bytes(source_paths.thumbnail, thumbnail);

  const auto archive_path = export_directory / "photo.sprout-profile";
  expect_failure(
      [&] { (void)source.archives.export_profile("child-photo", archive_path); },
      "managed portrait export should require explicit inclusion");
  expect(!std::filesystem::exists(archive_path),
         "declined personal-image export should not create an archive");
  const auto exported = source.archives.export_profile(
      "child-photo", archive_path,
      ProfileArchiveExportOptions{.include_managed_avatar = true});
  expect(exported.contains_personal_image,
         "explicit managed portrait export should mark personal-image content");

  Stores target(directory.path() / "target");
  (void)target.archives.restore_profile(archive_path);
  const auto restored = target.profiles.find_profile("child-photo");
  expect(restored.has_value() &&
             sprout::launcher::is_managed_profile_image_reference(
                 restored->avatar_ref),
         "managed portrait restore should create a new safe local reference");
  const auto restored_paths = sprout::launcher::resolve_managed_profile_image(
      directory.path() / "target" / "profile-images", restored->avatar_ref);
  expect(read_text(restored_paths.portrait) ==
                 std::string(portrait.begin(), portrait.end()) &&
             read_text(restored_paths.thumbnail) ==
                 std::string(thumbnail.begin(), thumbnail.end()),
         "normalized managed image bytes should round-trip exactly");
}

void corruption_and_newer_manifest_fail_before_mutation() {
  TemporaryDirectory directory;
  const auto export_directory = directory.path() / "exports";
  std::filesystem::create_directories(export_directory);
  Stores source(directory.path() / "source");
  source.profiles.create_profile(child_profile());
  source.time_policy.set_daily_allowance("child-alex", 45 * 60);
  const auto valid_path = export_directory / "valid.sprout-profile";
  (void)source.archives.export_profile("child-alex", valid_path);

  std::string corrupted = read_text(valid_path);
  const auto payload = corrupted.find("\"payload\": \"");
  expect(payload != std::string::npos, "test should locate encoded payload");
  const auto payload_byte = payload + std::string("\"payload\": \"").size();
  corrupted[payload_byte] = corrupted[payload_byte] == 'A' ? 'B' : 'A';
  const auto corrupted_path = export_directory / "corrupt.sprout-profile";
  write_text(corrupted_path, corrupted);

  Stores target(directory.path() / "target");
  expect_failure([&] { (void)target.archives.restore_profile(corrupted_path); },
                 "tampered payload should fail checksum validation");
  expect(!target.profiles.find_profile("child-alex").has_value() &&
             !target.time_policy.find_daily_allowance_seconds("child-alex")
                  .has_value(),
         "corrupt restore should not mutate profile or policy state");

  std::string newer = read_text(valid_path);
  const auto schema = newer.find("\"schemaVersion\": 1");
  expect(schema != std::string::npos, "test should locate manifest schema");
  newer[schema + std::string("\"schemaVersion\": ").size()] = '2';
  const auto newer_path = export_directory / "newer.sprout-profile";
  write_text(newer_path, newer);
  expect_failure([&] { (void)target.archives.inspect_archive(newer_path); },
                 "newer manifest schema should be rejected");
}

void conflicts_and_traversal_references_fail_cleanly() {
  TemporaryDirectory directory;
  const auto export_directory = directory.path() / "exports";
  std::filesystem::create_directories(export_directory);
  Stores source(directory.path() / "source");
  source.profiles.create_profile(child_profile());
  source.time_policy.set_daily_allowance("child-alex", 45 * 60);
  const auto archive_path = export_directory / "alex.sprout-profile";
  (void)source.archives.export_profile("child-alex", archive_path);
  expect_failure(
      [&] { (void)source.archives.export_profile("child-alex", archive_path); },
                 "export should not overwrite an existing archive");

  Stores id_conflict(directory.path() / "id-conflict");
  id_conflict.profiles.create_profile(child_profile());
  expect_failure(
      [&] { (void)id_conflict.archives.restore_profile(archive_path); },
                 "restore should reject an existing profile ID");

  Stores namespace_conflict(directory.path() / "namespace-conflict");
  namespace_conflict.profiles.create_profile(
      child_profile("child-other", "saves-alex"));
  expect_failure(
      [&] { (void)namespace_conflict.archives.restore_profile(archive_path); },
                 "restore should reject an existing save namespace");
  expect(!namespace_conflict.profiles.find_profile("child-alex").has_value(),
         "conflicting restore should leave target profiles unchanged");

  Stores traversal(directory.path() / "traversal");
  traversal.profiles.create_profile(
      child_profile("child-path", "saves-path", "local:../../outside"));
  traversal.time_policy.set_daily_allowance("child-path", 30 * 60);
  expect_failure(
      [&] {
        (void)traversal.archives.export_profile(
            "child-path", export_directory / "path.sprout-profile",
            ProfileArchiveExportOptions{.include_managed_avatar = true});
      },
      "managed avatar traversal reference should be rejected");
  expect(!std::filesystem::exists(directory.path() / "outside"),
         "traversal rejection should not touch an outside path");
}

void managed_image_conflict_leaves_second_target_unchanged() {
  TemporaryDirectory directory;
  const auto export_directory = directory.path() / "exports";
  std::filesystem::create_directories(export_directory);
  Stores source(directory.path() / "source");
  source.profiles.create_profile(
      child_profile("child-photo", "saves-photo", "local:abcd-r1"));
  source.time_policy.set_daily_allowance("child-photo", 30 * 60);
  const auto source_paths = sprout::launcher::resolve_managed_profile_image(
      directory.path() / "source" / "profile-images", "local:abcd-r1");
  write_bytes(source_paths.portrait, png_header(256));
  write_bytes(source_paths.thumbnail, png_header(96));
  const auto archive_path = export_directory / "photo.sprout-profile";
  (void)source.archives.export_profile(
      "child-photo", archive_path,
      ProfileArchiveExportOptions{.include_managed_avatar = true});

  const auto shared_images = directory.path() / "shared-images";
  ProfileRepository first_profiles(directory.path() / "first.sqlite3");
  DailyTimePolicyStore first_policy(directory.path() / "first-policy.sqlite3");
  ProfileArchiveService first(first_profiles, first_policy, shared_images);
  (void)first.restore_profile(archive_path);

  ProfileRepository second_profiles(directory.path() / "second.sqlite3");
  DailyTimePolicyStore second_policy(directory.path() / "second-policy.sqlite3");
  ProfileArchiveService second(second_profiles, second_policy, shared_images);
  expect_failure([&] { (void)second.restore_profile(archive_path); },
                 "managed image generation conflict should fail restore");
  expect(!second_profiles.find_profile("child-photo").has_value() &&
             !second_policy.find_daily_allowance_seconds("child-photo").has_value(),
         "image conflict should leave second profile and policy stores unchanged");
}

}  // namespace

int main() {
  try {
    built_in_child_round_trips_with_daily_allowance();
    managed_avatar_requires_consent_and_round_trips();
    corruption_and_newer_manifest_fail_before_mutation();
    conflicts_and_traversal_references_fail_cleanly();
    managed_image_conflict_leaves_second_target_unchanged();
  } catch (const std::exception& error) {
    std::cerr << "profile archive test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "profile archive tests passed\n";
  return EXIT_SUCCESS;
}
