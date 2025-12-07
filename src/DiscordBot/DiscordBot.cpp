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

      if (dpp::run_once<struct register_bot_commands>()) {
        registerBulkSlashCommandsToDiscord();
      }

      // Set bot presence with current time
      auto start_time = std::chrono::system_clock::now();
      auto time_t_now = std::chrono::system_clock::to_time_t(start_time);
      std::tm tm_now = *std::localtime(&time_t_now);
      std::ostringstream oss;
      oss << std::put_time(&tm_now, "%d.%m.%Y %H:%M:%S");
      std::string time_str = oss.str();
      bot_->set_presence(dpp::presence(dpp::ps_online, dpp::at_game, "since: " + time_str));
    });

    bot_->on_slashcommand([this](const dpp::slashcommand_t &event) { handleSlashCommand(event); });

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
  logger_->info("Starting " + getName() + " in non-blocking mode...");

  try {
    // Start bot in non-blocking mode
    bot_->start(dpp::st_return);

    // Keep thread alive while running
    while (running_.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    logger_->info(getName() + " stopped gracefully");
    return true;
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

  if (bot_) {
    bot_->shutdown();
  }

  return true;
}

void DiscordBot::handleSlashCommand(const dpp::slashcommand_t &event) {
  const auto &cmd_name = event.command.get_command_name();
  logger_->info("Received slash command: " + cmd_name);

  auto it = std::find_if(commands_.begin(), commands_.end(), [&cmd_name](const SlashCommand &cmd) {
    return cmd.getName() == cmd_name;
  });

  if (it != commands_.end()) {
    const auto &handler_type = it->getHandlerType();

    if (handler_type == "simple") {
      if (cmd_name == "ping") {
        event.reply("pong!");
      }
      if (cmd_name == "help") {
        event.thinking();
        std::string help_msg = "Available commands:\n";
        for (const auto &cmd : commands_) {
          help_msg += "`/" + cmd.getName() + "` : " + cmd.getDescription() + "\n";
        }
        event.edit_response(help_msg);
      }
      if (cmd_name == "emoji") {
        event.thinking();
        event.edit_response(emojiesLib_->getRandomEmoji());
      }
    } else if (handler_type == "botself") {
      if (cmd_name == "setstatus") {
        event.thinking();
        auto message_param = event.get_parameter("message");
        if (message_param.index() == 0) {
          event.edit_response("Error: Message parameter is required.");
        }
        std::string message = std::get<std::string>(message_param);
        bot_->set_presence(dpp::presence(dpp::ps_online, dpp::at_game, message));
        event.edit_response("Bot status set to: " + message);
      }
      if (cmd_name == "stopbot") {
        event.reply("Stopping the bot...");
        auto start_time = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(start_time);
        std::tm tm_now = *std::localtime(&time_t_now);
        std::ostringstream oss;
        oss << std::put_time(&tm_now, "%d.%m.%Y %H:%M:%S");
        std::string time_str = oss.str();
        bot_->set_presence(dpp::presence(dpp::ps_online, dpp::at_game, "stopped: " + time_str));

        std::this_thread::sleep_for(std::chrono::seconds(3));
        stop();
      }
    } else {
      event.reply("Command handler for '" + cmd_name + "' not implemented yet.");
    }
  } else {
    event.reply("Unknown command: " + cmd_name);
  }
}

void DiscordBot::registerBulkSlashCommandsToDiscord() {
  std::vector<dpp::slashcommand> dpp_commands;
  dpp_commands.reserve(commands_.size());

  for (const auto &cmd : commands_) {
    dpp_commands.push_back(cmd.toDppCommand(bot_->me.id));
    logger_->info("Prepared slash command: " + cmd.getName());
  }

  bot_->global_bulk_command_create(dpp_commands, [this](
                                                     const dpp::confirmation_callback_t &callback) {
    if (callback.is_error()) {
      logger_->errorStream() << "Failed to register bulk commands: "
                             << callback.get_error().message;
    } else {
      logger_->infoStream() << "Successfully registered " << commands_.size() << " slash commands";
    }
  });
}
