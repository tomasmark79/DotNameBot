#include "DiscordBot.hpp"

bool DiscordBot::getTokenFromFile(std::string &token) {
  const auto tokenPathOpt = customStrings_->getPath("dotnamebot.token");
  if (!tokenPathOpt.has_value()) {
    logger_->error("Failed to get token file path from custom strings");
    return false;
  }
  const auto &tokenPath = tokenPathOpt.value();

  if (std::ifstream tokenFile{tokenPath}; tokenFile.is_open()) {
    std::getline(tokenFile, token);

    if (token.empty()) {
      logger_->error("Token file is empty or invalid: " + tokenPath);
      return false;
    }

    logger_->info("Token read successfully from: " + tokenPath);
    return true;
  }

  logger_->error("Failed to open token file: " + tokenPath);
  return false;
}

bool DiscordBot::initialize() {
  logger_->info("Initializing " + getName() + "...");

  std::string token;
  if (!getTokenFromFile(token)) {
    logger_->error("Failed to read token from file");
    return false;
  }

  try {
    bot_ = std::make_unique<dpp::cluster>(token, dpp::i_default_intents | dpp::i_message_content);
    bot_->log(dpp::ll_info, "Bot ID: " + std::to_string(bot_->me.id));

    bot_->on_log([&](const dpp::log_t &log) {
      if (log.message.find("Uncaught exception in thread pool") != std::string::npos) {
        logger_->critical("Uncaught exception in thread pool: " + log.message);
      } else {
        logger_->info("[DPP] " + log.message);
      }
    });

    bot_->on_ready([this](const dpp::ready_t &event) {
      logger_->info("Bot is ready! Logged in as: " + bot_->me.username);
      logger_->info("Bot ID: " + std::to_string(bot_->me.id));

      setupSlashCommands();
      registerSlashCommandsToDiscord();

      bot_->set_presence(dpp::presence(dpp::ps_online, dpp::at_game, "Running ..."));
    });

    return true;
  } catch (const std::exception &e) {
    logger_->error("Exception during " + getName() + " initialization: " + e.what());
    return false;
  }
}

bool DiscordBot::start() {
  if (!bot_) {
    logger_->error(getName() + " not initialized, cannot start");
    return false;
  }

  running_.store(true);
  logger_->info("Starting " + getName() + " with blocking call...");

  try {
    bot_->start(dpp::st_wait);
    running_.store(false);
    logger_->critical("CRITICAL: Bot has stopped unexpectedly!");
    return false;
  } catch (const std::exception &e) {
    logger_->error("Exception in " + getName() + " start: " + e.what());
    running_.store(false);
    return false;
  }
}

bool DiscordBot::stop() {
  // Atomic exchange: if already false (stopped), skip shutdown
  if (!running_.exchange(false)) {
    return true;
  }

  logger_->info("Stopping " + getName() + "...");

  if (bot_) {
    bot_->shutdown();
    logger_->info("Discord bot cluster shutdown initiated");
  }

  logger_->info(getName() + " stopped");
  return true;
}

namespace {
  dpp::command_option_type toCommandOptionType(OptionType type) {
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
  }
} // namespace

void DiscordBot::setupSlashCommands() {
  for (const auto &cmdContainer : cmds) {
    dpp::slashcommand command(cmdContainer.name, cmdContainer.description, bot_->me.id);

    for (const auto &option : cmdContainer.options) {
      try {
        command.add_option(dpp::command_option(toCommandOptionType(option.type), option.name,
                                               option.description, option.required));
      } catch (const std::invalid_argument &) {
        logger_->warning("Unknown option type for command: " + cmdContainer.name);
      }
    }
    slashCommands_.emplace_back(command);
    logger_->infoStream() << "Registered slash command: " << cmdContainer.name;
  }
}

void DiscordBot::registerSlashCommandsToDiscord() {
  for (const auto &command : slashCommands_) {
    bot_->global_command_create(
        command, [this, command](const dpp::confirmation_callback_t &callback) {
          if (callback.is_error()) {
            logger_->errorStream() << "Failed to register command '" << command.name
                                   << "': " << callback.get_error().message;
          } else {
            logger_->infoStream() << "Successfully registered command: " << command.name;
          }
        });
  }
}
