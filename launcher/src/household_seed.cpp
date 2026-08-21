#include "sprout/launcher/household_seed.hpp"

#include <yyjson.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace sprout::launcher {
namespace {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("Could not open household seed file");
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::string text(yyjson_val* object, const char* key) {
  auto* value = yyjson_obj_get(object, key);
  if (value == nullptr || !yyjson_is_str(value) || yyjson_get_len(value) == 0) {
    throw std::runtime_error(std::string("Household seed requires text field: ") + key);
  }
  return {yyjson_get_str(value), yyjson_get_len(value)};
}

std::vector<SeededLibraryItem> item_array(yyjson_val* object, const char* key) {
  auto* value = yyjson_obj_get(object, key);
  if (value == nullptr || !yyjson_is_arr(value)) {
    throw std::runtime_error(std::string("Household seed requires title array: ") + key);
  }
  std::vector<SeededLibraryItem> result;
  yyjson_arr_iter iterator = yyjson_arr_iter_with(value);
  while (auto* entry = yyjson_arr_iter_next(&iterator)) {
    if (!yyjson_is_obj(entry)) {
      throw std::runtime_error("Household seed item arrays must contain objects");
    }
    result.push_back({.platform = text(entry, "platform"),
                      .title = text(entry, "title")});
  }
  return result;
}

std::string normalize_title(std::string_view title) {
  std::string normalized;
  bool parenthetical = false;
  bool pending_space = false;
  for (const unsigned char value : title) {
    if (value == '(' || value == '[') {
      parenthetical = true;
      continue;
    }
    if (value == ')' || value == ']') {
      parenthetical = false;
      continue;
    }
    if (parenthetical) continue;
    if (std::isalnum(value)) {
      if (pending_space && !normalized.empty()) normalized.push_back(' ');
      normalized.push_back(static_cast<char>(std::tolower(value)));
      pending_space = false;
    } else {
      pending_space = true;
    }
  }
  return normalized;
}

const SeededProfile* find(const HouseholdSeed& seed, const std::string& id) {
  const auto item = std::find_if(seed.profiles.begin(), seed.profiles.end(),
                                 [&id](const auto& profile) {
                                   return profile.profile.id == id;
                                 });
  return item == seed.profiles.end() ? nullptr : &*item;
}

bool contains(const std::vector<SeededLibraryItem>& candidates,
              const std::string& platform,
              const std::string& title) {
  const auto normalized = normalize_title(title);
  return std::any_of(candidates.begin(), candidates.end(),
                     [&normalized, &platform](const auto& candidate) {
                       const auto normalized_candidate = normalize_title(candidate.title);
                       return candidate.platform == platform &&
                              (normalized_candidate == normalized ||
                               (normalized_candidate == "sprout mouse maze" &&
                                normalized == "mouse cheese maze"));
                     });
}

}  // namespace

bool seeded_library_item_matches(const SeededLibraryItem& seeded,
                                 const std::string& platform,
                                 const std::string& title) {
  return contains({seeded}, platform, title);
}

HouseholdSeed load_household_seed(const std::filesystem::path& path) {
  const auto encoded = read_file(path);
  yyjson_doc* document = yyjson_read(encoded.data(), encoded.size(), 0);
  if (document == nullptr) throw std::runtime_error("Household seed is invalid JSON");
  try {
    auto* root = yyjson_doc_get_root(document);
    if (!yyjson_is_obj(root) || !yyjson_is_uint(yyjson_obj_get(root, "schemaVersion")) ||
        yyjson_get_uint(yyjson_obj_get(root, "schemaVersion")) != 1) {
      throw std::runtime_error("Household seed must use schemaVersion 1");
    }
    auto* profiles = yyjson_obj_get(root, "profiles");
    if (profiles == nullptr || !yyjson_is_arr(profiles) || yyjson_arr_size(profiles) == 0) {
      throw std::runtime_error("Household seed requires profiles");
    }
    HouseholdSeed result;
    yyjson_arr_iter iterator = yyjson_arr_iter_with(profiles);
    while (auto* item = yyjson_arr_iter_next(&iterator)) {
      if (!yyjson_is_obj(item)) throw std::runtime_error("Household profile must be an object");
      const auto role_text = text(item, "role");
      const auto role = role_text == "parent" ? ProfileRole::Parent :
                        role_text == "child" ? ProfileRole::Child :
                        throw std::runtime_error("Household profile role must be parent or child");
      const auto id = text(item, "id");
      if (find(result, id) != nullptr) throw std::runtime_error("Household seed profile IDs must be unique");
      SeededProfile profile{
          .profile = {.id = id,
                      .display_name = text(item, "displayName"),
                      .role = role,
                      .avatar_ref = text(item, "avatarRef"),
                      .save_namespace = text(item, "saveNamespace"),
                      .content_policy_ref = role == ProfileRole::Child
                          ? std::optional<std::string>(text(item, "contentPolicyRef"))
                          : std::nullopt,
                      .time_policy_ref = role == ProfileRole::Child
                          ? std::optional<std::string>(text(item, "timePolicyRef"))
                          : std::nullopt,
                      .preferences_json = "{}",
                      .background_ref = text(item, "backgroundRef")},
          .curated_items = item_array(item, "curatedItems"),
          .favorite_items = item_array(item, "favoriteItems"),
      };
      result.profiles.push_back(std::move(profile));
    }
    yyjson_doc_free(document);
    return result;
  } catch (...) {
    yyjson_doc_free(document);
    throw;
  }
}

void apply_household_seed(ProfileRepository& repository, const HouseholdSeed& seed) {
  for (const auto& profile : seed.profiles) {
    if (!repository.find_profile(profile.profile.id).has_value()) {
      repository.create_profile(profile.profile);
    }
  }
}

bool seed_includes_title(const HouseholdSeed& seed, const std::string& profile_id,
                         const std::string& platform,
                         const std::string& title) {
  const auto* profile = find(seed, profile_id);
  return profile != nullptr && (profile->profile.role == ProfileRole::Parent ||
                                contains(profile->curated_items, platform, title));
}

bool seed_favorites_title(const HouseholdSeed& seed, const std::string& profile_id,
                          const std::string& platform,
                          const std::string& title) {
  const auto* profile = find(seed, profile_id);
  return profile != nullptr &&
         contains(profile->favorite_items, platform, title);
}

}  // namespace sprout::launcher
