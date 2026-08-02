#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace sprout::launcher {

class ParentAccessStore {
 public:
  ParentAccessStore(std::filesystem::path database_path,
                    std::filesystem::path device_key_path);
  ~ParentAccessStore();

  ParentAccessStore(ParentAccessStore&&) noexcept;
  ParentAccessStore& operator=(ParentAccessStore&&) noexcept;
  ParentAccessStore(const ParentAccessStore&) = delete;
  ParentAccessStore& operator=(const ParentAccessStore&) = delete;

  void set_pin(const std::string& credential_ref, std::string pin);
  [[nodiscard]] bool verify_pin(const std::string& credential_ref,
                                std::string pin) const;
  void grant_until_end_of_day(const std::string& credential_ref,
                              std::string pin, std::int64_t now_utc_seconds,
                              const std::string& local_date);
  [[nodiscard]] bool is_unlocked(std::int64_t now_utc_seconds,
                                 const std::string& local_date);
  void lock();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sprout::launcher
