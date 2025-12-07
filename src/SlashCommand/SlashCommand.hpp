#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

enum class OptionType : std::uint8_t { String, Integer, Bool, User, Channel, Role };

class SlashCommand {
public:
  struct CommandOption {
    OptionType type;
    std::string name;
    std::string description;
    bool required = false;

    std::vector<std::pair<std::string, std::string>> choices;
    std::optional<int64_t> min_value;
    std::optional<int64_t> max_value;
  };

  /**
   * @brief Construct a new Slash Command object
   *
   * @param n Name of the command
   * @param d Description of the command
   * @param h Handler type
   */
  SlashCommand(std::string n, std::string d, std::string h = "")
      : name_(std::move(n)), description_(std::move(d)), handler_type_(std::move(h)) {}

  /**
   * @brief Construct a new Slash Command object with options
   *
   * @param n Name of the command
   * @param d Description of the command
   * @param opts Options for the command
   * @param h Handler type
   */
  SlashCommand(std::string n, std::string d, std::vector<CommandOption> opts, std::string h = "")
      : name_(std::move(n)), description_(std::move(d)), options_(std::move(opts)),
        handler_type_(std::move(h)) {}

  [[nodiscard]] const std::string &getName() const { return name_; }
  [[nodiscard]] const std::string &getDescription() const { return description_; }
  [[nodiscard]] const std::string &getHandlerType() const { return handler_type_; }
  [[nodiscard]] const std::vector<CommandOption> &getOptions() const { return options_; }

  SlashCommand &setDefaultPermissions(dpp::permission perms) {
    default_permissions_ = perms;
    return *this;
  }

  SlashCommand &setDmPermission(bool allowed) {
    dm_permission_ = allowed;
    return *this;
  }

  /**
   * @brief Convert to dpp::slashcommand
   *
   * @param bot_id
   * @return dpp::slashcommand
   */
  [[nodiscard]] dpp::slashcommand toDppCommand(dpp::snowflake bot_id) const;

private:
  std::string name_;
  std::string description_;
  std::vector<CommandOption> options_;
  std::string handler_type_;

  dpp::permission default_permissions_ = 0;
  bool dm_permission_ = true;

  static dpp::command_option_type toCommandOptionType(OptionType type) {
    switch (type) {
    case OptionType::String: return dpp::co_string;
    case OptionType::Integer: return dpp::co_integer;
    case OptionType::Bool: return dpp::co_boolean;
    case OptionType::User: return dpp::co_user;
    case OptionType::Channel: return dpp::co_channel;
    case OptionType::Role: return dpp::co_role;
    default: throw std::invalid_argument("Unknown OptionType");
    }
  }
};

extern const std::vector<SlashCommand> commands_;
