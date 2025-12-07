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
    std::optional<int64_t> minValue;
    std::optional<int64_t> maxValue;
  };

  /**
   * @brief Construct a new Slash Command object
   *
   * @param n Name of the command
   * @param d Description of the command
   * @param h Handler type
   */
  SlashCommand(std::string n, std::string d, std::string h = "")
      : name_(std::move(n)), description_(std::move(d)), handlerType_(std::move(h)) {}

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
        handlerType_(std::move(h)) {}

  [[nodiscard]] const std::string &getName() const { return name_; }
  [[nodiscard]] const std::string &getDescription() const { return description_; }
  [[nodiscard]] const std::string &getHandlerType() const { return handlerType_; }
  [[nodiscard]] const std::vector<CommandOption> &getOptions() const { return options_; }

  SlashCommand &setDefaultPermissions(dpp::permission perms) {
    defaultPermissions_ = perms;
    return *this;
  }

  SlashCommand &setDmPermission(bool allowed) {
    dmPermission_ = allowed;
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
  std::string handlerType_;

  dpp::permission defaultPermissions_ = 0;
  bool dmPermission_ = true;

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
