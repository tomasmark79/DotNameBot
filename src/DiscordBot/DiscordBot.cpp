#include "DiscordBot.hpp"

namespace dotnamecpp::discordbot {

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

      bot_->on_ready([this](const dpp::ready_t &) {
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

        // Initial RSS fetch
        int itemsFetched = rssService_->refetchRssFeeds();
        if (itemsFetched >= 0) {
          logger_->info("Initial RSS fetch completed. Total items in buffer: " +
                        std::to_string(rssService_->getItemCount()));
        } else {
          logger_->error("Initial RSS fetch failed.");
        }

        // Start the periodic random feed timer
        if (!putRandomFeedTimer()) {
          logger_->error("Failed to start random feed timer.");
        }
      });

      bot_->on_slashcommand(
          [this](const dpp::slashcommand_t &event) { handleSlashCommand(event); });

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

    isRunning_.store(true);
    startTime_ = std::chrono::system_clock::now();
    logger_->info("Starting " + getName() + " in non-blocking mode...");

    try {
      // Start bot in non-blocking mode
      bot_->start(dpp::st_return);

      // Keep thread alive while running
      while (isRunning_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      logger_->info(getName() + " stopped gracefully");
      return true;
    } catch (const std::exception &e) {
      logger_->error("Exception in " + getName() + " start: " + e.what());
      isRunning_.store(false);
      return false;
    }
  }

  bool DiscordBot::stop() {
    // Atomic exchange: if already false (stopped), skip shutdown
    if (!isRunning_.exchange(false)) {
      return true;
    }

    // Join all worker threads
    isRunningTimer_.store(false);
    for (auto &thread : threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
    threads_.clear();

    if (bot_) {
      logger_->info("Shutting down Discord bot...");
      bot_->shutdown();

      // Give DPP time to clean up its threads
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Explicitly destroy the bot to ensure all DPP threads are stopped
      bot_.reset();
    }

    return true;
  }

  void DiscordBot::handleSlashCommand(const dpp::slashcommand_t &event) {
    const auto &cmd_name = event.command.get_command_name();
    logger_->info("Received slash command: " + cmd_name);

    auto it =
        std::find_if(commands_.begin(), commands_.end(),
                     [&cmd_name](const SlashCommand &cmd) { return cmd.getName() == cmd_name; });

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
      } else if (handler_type == "rss") {
        if (cmd_name == "refetch") {
          event.thinking();
          int itemsFetched = rssService_->refetchRssFeeds();
          if (itemsFetched >= 0) {
            event.edit_response("Refetched RSS feeds successfully. Total items in buffer: " +
                                std::to_string(rssService_->getItemCount()));
          } else {
            event.edit_response("Failed to refetch RSS feeds.");
          }
        }
        if (cmd_name == "listurls") {
          event.thinking();
          std::string urlsList = rssService_->listUrlsAsString();
          if (urlsList.empty()) {
            return;
          }

          event.edit_response("Registered RSS/ATOM feed URLs:\n");
          std::vector<std::string> splitMessages;
          if (splitDiscordMessageIfNeeded(urlsList, splitMessages)) {
            for (const auto &msgPart : splitMessages) {
              dpp::message msg(event.command.channel_id, msgPart);
              bot_->message_create(msg);
            }
          }
        }

        if (cmd_name == "getrandomfeed") {
          event.thinking();
          dotnamecpp::rss::RSSItem item = rssService_->getRandomItem();
          if (item.title.empty()) {
            event.edit_response("No RSS items available at the moment.");
          } else {
            std::string response = item.toMarkdownLink();
            event.edit_response(response);

            // Log the served item
            logTheServed(item, [this](bool success) {
              if (success) {
                logger_->info("Served RSS item logged successfully.");
              } else {
                logger_->error("Failed to log served RSS item.");
              }
            });
          }
        }

        if (cmd_name == "addurl") {
          event.thinking();
          auto urlParam = event.get_parameter("url");
          auto embeddedParam = event.get_parameter("embedded");

          if (urlParam.index() == 0) {
            event.edit_response("Error: URL parameter is required.");
            return;
          }

          std::string url = std::get<std::string>(urlParam);
          bool embedded = false;
          if (embeddedParam.index() != 0) {
            embedded = std::get<bool>(embeddedParam);
          }

          if (rssService_->addUrl(url, embedded, event.command.channel_id)) {
            event.edit_response("Successfully added RSS/ATOM feed URL: " + url +
                                (embedded ? " (embedded)" : " (non-embedded)"));
          } else {
            event.edit_response("Failed to add RSS/ATOM feed URL: " + url);
          }
        }

        if (cmd_name == "modurl") {
          event.thinking();
          auto urlParam = event.get_parameter("url");
          auto embeddedParam = event.get_parameter("embedded");

          if (urlParam.index() == 0) {
            event.edit_response("Error: URL parameter is required.");
            return;
          }

          std::string url = std::get<std::string>(urlParam);
          bool embedded = false;
          if (embeddedParam.index() != 0) {
            embedded = std::get<bool>(embeddedParam);
          }

          if (rssService_->modUrl(url, embedded, event.command.channel_id)) {
            event.edit_response("Successfully modified RSS/ATOM feed URL: " + url +
                                (embedded ? " (embedded)" : " (non-embedded)"));
          } else {
            event.edit_response("Failed to modify RSS/ATOM feed URL: " + url);
          }
        }

        if (cmd_name == "remurl") {
          event.thinking();
          auto urlParam = event.get_parameter("url");
          if (urlParam.index() == 0) {
            event.edit_response("Error: URL parameter is required.");
            return;
          }
          std::string url = std::get<std::string>(urlParam);
          if (rssService_->remUrl(url)) {
            event.edit_response("Successfully removed RSS/ATOM feed URL: " + url);
          } else {
            event.edit_response("Failed to remove RSS/ATOM feed URL: " + url);
          }
        }

        if (cmd_name == "gettotalfeeds") {
          event.thinking();
          size_t itemCount = rssService_->getItemCount();
          event.edit_response("Total RSS items in buffer: " + std::to_string(itemCount));
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

          // Set presence before stopping
          auto start_time = std::chrono::system_clock::now();
          auto time_t_now = std::chrono::system_clock::to_time_t(start_time);
          std::tm tm_now = *std::localtime(&time_t_now);
          std::ostringstream oss;
          oss << std::put_time(&tm_now, "%d.%m.%Y %H:%M:%S");
          std::string time_str = oss.str();
          bot_->set_presence(dpp::presence(dpp::ps_online, dpp::at_game, "stopped: " + time_str));

          // Don't call stop() directly from event handler (causes deadlock in DPP thread pool)
          // Just set the running flag to false, the main loop will handle cleanup
          logger_->info("Stop requested via /stopbot command");
          isRunning_.store(false);
        }
        if (cmd_name == "uptime") {
          event.thinking();
          auto now = std::chrono::system_clock::now();
          auto uptime_duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime_);

          constexpr int SECONDS_PER_MINUTE = 60;
          constexpr int SECONDS_PER_HOUR = 3600;

          auto total_seconds = uptime_duration.count();
          int hours = static_cast<int>(total_seconds / SECONDS_PER_HOUR);
          int minutes = static_cast<int>((total_seconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE);
          int seconds = static_cast<int>(total_seconds % SECONDS_PER_MINUTE);

          std::ostringstream oss;
          oss << "Uptime: " << hours << "h " << minutes << "m " << seconds << "s";
          event.edit_response(oss.str());
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

    bot_->global_bulk_command_create(dpp_commands,
                                     [this](const dpp::confirmation_callback_t &callback) {
      if (callback.is_error()) {
        logger_->errorStream() << "Failed to register bulk commands: "
                               << callback.get_error().message;
      } else {
        logger_->infoStream() << "Successfully registered " << commands_.size()
                              << " slash commands";
      }
    });
  }

  bool DiscordBot::splitDiscordMessageIfNeeded(const std::string &message,
                                               std::vector<std::string> &outMessages) {
    if (message.length() <= MAX_DISCORD_MESSAGE_LENGTH) {
      outMessages.push_back(message);
      return true;
    }

    size_t start = 0;
    while (start < message.length()) {
      size_t end = std::min(start + MAX_DISCORD_MESSAGE_LENGTH, message.length());

      // Try to split at the last newline or space before the limit
      size_t splitPos = message.rfind('\n', end);
      if (splitPos == std::string::npos || splitPos < start) {
        splitPos = message.rfind(' ', end);
      }
      if (splitPos == std::string::npos || splitPos < start) {
        splitPos = end; // Forced split
      }

      outMessages.push_back(message.substr(start, splitPos - start));
      start = splitPos;
      if (message[start] == '\n' || message[start] == ' ') {
        ++start; // Skip the delimiter
      }
    }

    return true;
  }

  void DiscordBot::logTheServed(rss::RSSItem &item, std::function<void(bool)> onComplete) {
    constexpr dpp::snowflake LOG_CHANNEL_ID = 1453787666201182359;
    dpp::message msg(LOG_CHANNEL_ID, item.toMarkdownLink());
    msg.set_flags(dpp::m_suppress_embeds);

    bot_->message_create(msg, [this, onComplete](const dpp::confirmation_callback_t &callback) {
      if (callback.is_error()) {
        logger_->error("Failed to log served RSS item: " + callback.get_error().message);
        onComplete(false);
      } else {
        onComplete(true);
      }
    });
  }

  // bool DiscordBot::logTheServed(rss::RSSItem &item) {
  //   // TODO: define outside the magic number
  //   constexpr dpp::snowflake LOG_CHANNEL_ID = 1453787666201182359;
  //   dpp::message msg(LOG_CHANNEL_ID, item.toMarkdownLink());
  //   msg.set_flags(dpp::m_suppress_embeds);

  //   bool isSuccess{false};
  //   bot_->message_create(msg, [this](const dpp::confirmation_callback_t &callback) {
  //     if (callback.is_error()) {
  //       logger_->error("Failed to log served RSS item: " + callback.get_error().message);
  //       return;
  //     }
  //   });
  //   return isSuccess;
  // }

  // TODO: take outside the magic number
  // Timer interval for publicate message is 10 messages per hour
  // constexpr int TICK_INTERVAL_SECONDS = 6 * 60;
  constexpr int TICK_INTERVAL_SECONDS = 10;

  bool DiscordBot::putRandomFeedTimer() {
    threads_.emplace_back([this]() -> void {
      isRunningTimer_.store(true);

      while (isRunningTimer_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(TICK_INTERVAL_SECONDS));

        dotnamecpp::rss::RSSItem item = rssService_->getRandomItem();
        if (item.title.empty()) {
          logger_->info("No RSS items available at the moment.");
        }

        dpp::message msg(item.discordChannelId, item.toMarkdownLink());

        if (!item.embedded) {
          msg.set_flags(dpp::m_suppress_embeds);
        }

        bot_->message_create(msg, [this](const dpp::confirmation_callback_t &callback) {
          if (callback.is_error()) {
            logger_->error("Failed to create message: " + callback.get_error().message);
          }

          const auto &createdMessage = callback.get<dpp::message>();
          bot_->message_crosspost(createdMessage.id, createdMessage.channel_id,
                                  [this](const dpp::confirmation_callback_t &crosspostCallback) {
            if (crosspostCallback.is_error()) {
              logger_->errorStream()
                  << "Failed to crosspost message: " << crosspostCallback.get_error().message;
            } else {
              logger_->infoStream() << "Message crossposted successfully.";
            }
          });
        });
        // Log the served item
        logTheServed(item, [this](bool success) {
          if (success) {
            logger_->info("Served RSS item logged successfully.");
          } else {
            logger_->error("Failed to log served RSS item.");
          }
        });
      } // while isRunningTimer_
    });
    return true;
  }

  // TODO: take outside the magic number
  constexpr int FETCH_INTERVAL_SECONDS = 60 * 60; // 60 minutes

  bool DiscordBot::fetchFeedsTimer() {
    threads_.emplace_back([this]() -> void {
      isFetchFeedsTimerRunning_.store(true);
      while (isFetchFeedsTimerRunning_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(FETCH_INTERVAL_SECONDS));

        int itemsFetched = rssService_->refetchRssFeeds();
        if (itemsFetched >= 0) {
          logger_->info("Periodic RSS fetch completed. Total items in buffer: " +
                        std::to_string(rssService_->getItemCount()));
        } else {
          logger_->error("Periodic RSS fetch failed.");
        }
      }
    });
    return true;
  }

} // namespace dotnamecpp::discordbot