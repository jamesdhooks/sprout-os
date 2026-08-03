#include "sprout/launcher/setup_presentation.hpp"

#include <stdexcept>
#include <utility>

namespace sprout::launcher {

SetupPresentation::SetupPresentation(SetupWizard& wizard,
                                     bool custom_image_available,
                                     bool child_profile_available)
    : wizard_(wizard),
      custom_image_available_(custom_image_available),
      child_profile_available_(child_profile_available) {
  refresh_content();
}

SetupStep SetupPresentation::step() const noexcept { return wizard_.current_step(); }

std::size_t SetupPresentation::focus_index() const noexcept { return focus_index_; }

std::string_view SetupPresentation::title() const noexcept { return title_; }

std::string_view SetupPresentation::description() const noexcept {
  return description_;
}

ReadOnlyView<std::string_view> SetupPresentation::choices() const noexcept {
  return choices_;
}

const std::string& SetupPresentation::error_message() const noexcept {
  return error_message_;
}

std::optional<SetupPresentationEvent> SetupPresentation::handle(Action action) {
  if (action == Action::Back) {
    return SetupPresentationEvent::ExitRequested;
  }
  if (action == Action::Left || action == Action::Up) {
    move_focus(-1);
    return std::nullopt;
  }
  if (action == Action::Right || action == Action::Down) {
    move_focus(1);
    return std::nullopt;
  }
  if (action != Action::Confirm) {
    return std::nullopt;
  }

  error_message_.clear();
  try {
    if (step() == SetupStep::Avatars) {
      if (focus_index_ == 0) {
        return SetupPresentationEvent::ChooseParentAvatarRequested;
      }
      if (child_profile_available_ && focus_index_ == 1) {
        return SetupPresentationEvent::ChooseChildAvatarRequested;
      }
      const std::size_t import_index = child_profile_available_ ? 2U : 1U;
      if (custom_image_available_ && focus_index_ == import_index) {
        return SetupPresentationEvent::ImportParentImageRequested;
      }
    }
    if (step() == SetupStep::ParentPin && focus_index_ == 0) {
      return SetupPresentationEvent::ConfigureParentPinRequested;
    }
    confirm();
    focus_index_ = 0;
    refresh_content();
    if (step() == SetupStep::Complete) {
      return SetupPresentationEvent::Completed;
    }
  } catch (const std::exception& error) {
    error_message_ = error.what();
  }
  return std::nullopt;
}

void SetupPresentation::complete_avatar_step() {
  if (step() != SetupStep::Avatars) {
    throw std::logic_error("Profile image can only complete the portrait setup step");
  }
  wizard_.skip_current_step();
  focus_index_ = 0;
  error_message_.clear();
  refresh_content();
}

void SetupPresentation::report_avatar_error(std::string message) {
  if (step() != SetupStep::Avatars) {
    throw std::logic_error("Profile image errors require the portrait setup step");
  }
  error_message_ = std::move(message);
}

void SetupPresentation::complete_parent_pin_step(std::string credential_ref) {
  if (step() != SetupStep::ParentPin) {
    throw std::logic_error("Parent PIN can only complete its setup step");
  }
  wizard_.set_parent_credential_ref(std::move(credential_ref));
  focus_index_ = 0;
  error_message_.clear();
  refresh_content();
}

void SetupPresentation::refresh_content() {
  choices_.clear();
  switch (step()) {
    case SetupStep::Welcome:
      title_ = "WELCOME TO SPROUT";
      description_ = "A CALM PLACE FOR FAMILY GAMES";
      choices_ = {"GET STARTED"};
      return;
    case SetupStep::Locale:
      title_ = "LANGUAGE AND REGION";
      description_ = "CHOOSE A LOCAL DEFAULT";
      choices_ = {"ENGLISH CANADA", "ENGLISH US"};
      return;
    case SetupStep::Network:
      title_ = "PLAY OFFLINE";
      description_ = "WI-FI IS OPTIONAL AND CAN WAIT";
      choices_ = {"CONTINUE OFFLINE"};
      return;
    case SetupStep::Parent:
      title_ = "CREATE A PARENT";
      description_ = "CHOOSE A BUILT-IN PORTRAIT";
      choices_ = {"EXPLORER FOX", "MOON RABBIT"};
      return;
    case SetupStep::ParentPin:
      title_ = "PARENT PIN";
      description_ = "PROTECT PARENT MODE OFFLINE";
      choices_ = {"SET PIN", "SKIP FOR NOW"};
      return;
    case SetupStep::Child:
      title_ = "ADD A CHILD";
      description_ = "SAFE DEFAULTS ARE APPLIED AUTOMATICALLY";
      choices_ = {"ADD ALEX", "SKIP"};
      return;
    case SetupStep::Avatars:
      title_ = "PROFILE PORTRAITS";
      description_ = "CHOOSE FROM 32 BUILT-IN PORTRAITS";
      choices_ = {"CHOOSE PARENT"};
      if (child_profile_available_) choices_.push_back("CHOOSE CHILD");
      if (custom_image_available_) choices_.push_back("IMPORT FOR PARENT");
      choices_.push_back("CONTINUE");
      return;
    case SetupStep::Library:
      title_ = "GAME LIBRARY";
      description_ = "ADD YOUR GAMES AFTER SETUP";
      choices_ = {"SKIP FOR NOW"};
      return;
    case SetupStep::ChildDefaults:
      title_ = "CHILD-SAFE DEFAULTS";
      description_ = "UNKNOWN CONTENT STAYS HIDDEN";
      choices_ = {"USE SAFE DEFAULTS"};
      return;
    case SetupStep::Connectors:
      title_ = "OPTIONAL SERVICES";
      description_ = "SPROUT WORKS WITHOUT A SERVER";
      choices_ = {"STAY LOCAL"};
      return;
    case SetupStep::Review:
      title_ = "READY TO GROW";
      description_ = "SETUP CAN BE CHANGED LATER";
      choices_ = {"FINISH SETUP"};
      return;
    case SetupStep::Complete:
      title_ = "SETUP COMPLETE";
      description_ = "LOADING FAMILY PROFILES";
      choices_ = {};
      return;
  }
}

void SetupPresentation::move_focus(int delta) {
  if (choices_.size() < 2) {
    return;
  }
  const auto count = static_cast<long long>(choices_.size());
  const auto current = static_cast<long long>(focus_index_);
  focus_index_ = static_cast<std::size_t>((current + delta + count) % count);
}

void SetupPresentation::confirm() {
  switch (step()) {
    case SetupStep::Welcome:
      wizard_.skip_current_step();
      return;
    case SetupStep::Locale:
      wizard_.configure_locale(LocaleOverrides{
          .language = "en",
          .region = focus_index_ == 0 ? "CA" : "US",
          .time_zone = focus_index_ == 0 ? "America/Toronto" : "America/New_York",
      });
      return;
    case SetupStep::Network:
      wizard_.continue_offline();
      return;
    case SetupStep::Parent:
      wizard_.create_parent(NewProfile{
          .id = "parent-primary",
          .display_name = "Parent",
          .role = ProfileRole::Parent,
          .avatar_ref = focus_index_ == 0 ? "builtin:explorer-fox"
                                         : "builtin:moon-rabbit",
          .save_namespace = "saves-parent-primary",
          .content_policy_ref = std::nullopt,
          .time_policy_ref = std::nullopt,
          .preferences_json = "{}",
      });
      return;
    case SetupStep::ParentPin:
      if (focus_index_ == 1) {
        wizard_.skip_current_step();
        return;
      }
      throw std::logic_error("Parent PIN setup requires the PIN presentation");
      return;
    case SetupStep::Child:
      if (focus_index_ == 1) {
        wizard_.skip_current_step();
        return;
      }
      wizard_.create_child(NewProfile{
          .id = "child-primary",
          .display_name = "Alex",
          .role = ProfileRole::Child,
          .avatar_ref = "builtin:friendly-dragon",
          .save_namespace = "saves-child-primary",
          .content_policy_ref = "content:child-default",
          .time_policy_ref = "time:child-default",
          .preferences_json = "{}",
      });
      return;
    case SetupStep::Avatars:
    case SetupStep::Library:
    case SetupStep::ChildDefaults:
    case SetupStep::Connectors:
      wizard_.skip_current_step();
      return;
    case SetupStep::Review:
      wizard_.finish();
      return;
    case SetupStep::Complete:
      throw std::runtime_error("Setup is already complete");
  }
}

}  // namespace sprout::launcher
