#pragma once

#include <dpp/dpp.h>

#include <IBot/IBot.hpp>
#include <Utils/UtilsFactory.hpp>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

class DiscordBot : public IBot {
public:
  DiscordBot(std::shared_ptr<dotnamecpp::logging::ILogger> logger,
             std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager,
             std::shared_ptr<dotnamecpp::utils::ICustomStringsLoader> customStrings = nullptr)
      : logger_(logger ? std::move(logger) : std::make_shared<dotnamecpp::logging::NullLogger>()),
        assetManager_(std::move(assetManager)), customStrings_(std::move(customStrings)) {}

  ~DiscordBot() override { stop(); }

  // IBot interface declarations
  bool initialize() override;
  bool start() override;
  bool stop() override;

  [[nodiscard]]
  bool isRunning() const override {
    return running_.load();
  }

  [[nodiscard]]
  std::string getName() const override {
    return "D++ Discord Bot";
  }

private:
  std::unique_ptr<dpp::cluster> bot_;

  std::shared_ptr<dotnamecpp::logging::ILogger> logger_;
  std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager_;
  std::shared_ptr<dotnamecpp::utils::ICustomStringsLoader> customStrings_;

  bool getTokenFromFile(std::string &token);
  std::atomic<bool> running_{false};

  std::vector<dpp::slashcommand> slashCommands_;

  enum class OptionType : std::uint8_t { String, Integer, Bool };

  struct CommandOption {
    OptionType type;
    std::string name;
    std::string description;
    bool required = false;
  };

  struct customSlashCommandContainer {
    std::string name;
    std::string description;
    std::vector<CommandOption> options;
  };

  static dpp::command_option_type toCommandOptionType(OptionType type) {
    switch (type) {
    case OptionType::String:
      return dpp::co_string;
    case OptionType::Integer:
      return dpp::co_integer;
    case OptionType::Bool:
      return dpp::co_boolean;
    default:
      throw std::invalid_argument("Unknown OptionType");
    }
  };

  const std::vector<customSlashCommandContainer> cmds = {
      {"ping", "Get pong.", {}},
      {"help", "Get help.", {}},
      {"emoji", "Get emoji.", {}},
      {"verse", "Get verse", {}},
      {"btc", "Get Bitcoin price (USD)", {}},
      {"eth", "Get Ethereum price (USD)", {}},
      {"t2e",
       "Translate text to English",
       {{OptionType::String, "message", "The message to translate", true}}},
      {"t2c",
       "Translate text to Czech",
       {{OptionType::String, "message", "The message to translate", true}}},
      {"heygoogle", "Ask Gemini", {{OptionType::String, "prompt", "A prompt", true}}},
      {"addurl",
       "Add another RSS/ATOM feed URL",
       {{OptionType::String, "url", "URL of the RSS/ATOM feed", true},
        {OptionType::Bool, "embedded", "Whether the feed should be embedded", false}}},
      {"invokerefetch", "Manually refetch RSS/ATOM feeds", {}},
      {"listurls", "Get list of RSS/ATOM feed URLs", {}},
      {"printfeed", "Get random RSS/ATOM feed items", {}},
      {"printcountfeeds", "Get count of RSS/ATOM feed items", {}},
      {"botstatus",
       "Set bot status",
       {{OptionType::String, "message", "The status message", true}}},
      {"terminal",
       "Run a command and return the output",
       {{OptionType::String, "command", "The command to run", true}}}};

  /**
   * @brief Load slash commands from configuration
   *
   */
  void loadSlashCommandsFromConfig();

  /**
   * @brief Register slash commands to Discord
   *        Very soon exceeding rate limits if many commands are registered
   *
   */
  void registerSlashCommandsToDiscord();

  /**
   * @brief Register bulk slash commands to Discord
   *        More efficient way to register multiple commands at once
   *
   */
  void registerBulkSlashCommandsToDiscord();
};
