#include "DiscordBot.hpp"
#include <chrono>

namespace dotnamecpp::discordbot {

  DiscordBot::DiscordBot(ServiceContainer &services)
      : logger_(services.getService<dotnamecpp::logging::ILogger>()),
        assetManager_(services.getService<dotnamecpp::assets::IAssetManager>()),
        customStrings_(services.getService<dotnamecpp::utils::ICustomStringsLoader>()),
        emojiModuleLib_(services.getService<dotnamecpp::v1::EmojiModuleLib>()),
        rssService_(services.getService<dotnamecpp::rss::IRssService>()) {

    if (!logger_) {
      throw std::runtime_error("DiscordBot requires a logger");
    }

    if (!assetManager_) {
      throw std::runtime_error("DiscordBot requires an asset manager");
    }

    if (emojiModuleLib_) {
      logger_->infoStream() << "DiscordBot initialized with EmojiModuleLib, random emoji: "
                            << emojiModuleLib_->getRandomEmoji();
    } else {
      throw std::runtime_error("DiscordBot requires an EmojiModuleLib");
    }

    std::string token;
    if (!getTokenFromFile(token)) {
      logger_->error("Failed to read token from file");
      throw std::runtime_error("DiscordBot requires a valid token");
    }

    cluster_ =
        std::make_unique<dpp::cluster>(token, dpp::i_default_intents | dpp::i_message_content);
  }

  DiscordBot::~DiscordBot() {
    if (isRunning_.load()) {
      stop();
    }
  }

  bool DiscordBot::initialize() {
    logger_->info("Initializing " + getName() + "...");

    try {
      cluster_->log(dpp::ll_info, "Cluster ID: " + std::to_string(cluster_->me.id));

      on_log_handle_ = cluster_->on_log([logger = logger_](const dpp::log_t &log) {
        if (log.severity >= dpp::ll_error) {
          logger->error("[DPP] " + log.message);
        } else {
          logger->info("[DPP] " + log.message);
        }
      });

      on_ready_handle_ = cluster_->on_ready([logger = logger_, rss = rssService_, emj = emojiModuleLib_,
                                             commands = commands_,
                                             cluster_ptr = cluster_.get()](const dpp::ready_t &) {
        logger->info("Bot is ready! Logged in as: " + cluster_ptr->me.username);
        logger->info("Bot ID: " + std::to_string(cluster_ptr->me.id));

        if (dpp::run_once<struct register_cluster_commands>()) {
          // Prepare commands locally and register using cluster_ptr
          std::vector<dpp::slashcommand> dpp_commands;
          dpp_commands.reserve(commands.size());
          for (const auto &cmd : commands) {
            dpp_commands.push_back(cmd.toDppCommand(cluster_ptr->me.id));
            logger->info("Prepared slash command: " + cmd.getName());
          }
          cluster_ptr->global_bulk_command_create(
              dpp_commands, [logger, commands](const dpp::confirmation_callback_t &callback) {
            if (callback.is_error()) {
              logger->errorStream()
                  << "Failed to register bulk commands: " << callback.get_error().message;
            } else {
              logger->infoStream()
                  << "Successfully registered " << commands.size() << " slash commands";
            }
          });
        }

        // Set bot presence with current time
        auto start_time = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(start_time);
        std::tm tm_now = *std::localtime(&time_t_now);
        std::ostringstream oss;
        oss << std::put_time(&tm_now, "%d.%m.%Y %H:%M:%S");
        std::string time_str = oss.str();
        cluster_ptr->set_presence(
            dpp::presence(dpp::ps_online, dpp::at_competing, "boot<T>: " + time_str));
      });

      on_slashcommand_handle_ = cluster_->on_slashcommand(
          [this](const dpp::slashcommand_t &event) { handleSlashCommand(event); });

      // Start the periodic random feed timer
      if (!putRandomFeedTimer()) {
        logger_->error("Failed to start random feed timer.");
      }

      // Start the periodic fetch feeds timer
      if (!fetchFeedsTimer()) {
        logger_->error("Failed to start fetch feeds timer.");
      }

      return true;

    } catch (const std::exception &e) {
      logger_->error("Exception during " + getName() + " initialization: " + e.what());
      return false;
    }
  }

  bool DiscordBot::start() {
    if (!cluster_) {
      logger_->error(getName() + " not initialized, cannot start");
      return false;
    }

    isRunning_.store(true);
    startTime_ = std::chrono::system_clock::now();
    logger_->info("Starting " + getName() + " in non-blocking mode...");

    try {
      cluster_->start(dpp::st_return);
      while (isRunning_.load()) {
        std::unique_lock<std::mutex> lock(cvMutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(100));
      }

      logger_->info(getName() + " stopped gracefully");
      return true;

    } catch (const std::exception &e) {
      logger_->error("Exception in " + getName() + " start: " + e.what());
      return false;
    }
  }

  bool DiscordBot::stop() {

    // Signal to stop user timers
    isPRFTRunning_.store(false);
    isFFTRunning_.store(false);
    cv_.notify_all();
    for (auto &thread : threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
    threads_.clear();

    if (cluster_) {
      logger_->info("Detaching DPP event handlers...");

      // Use cluster raw pointer to avoid using `this` in the destructor
      auto *cluster_ptr = cluster_.get();
      if (on_slashcommand_handle_ != 0) {
        cluster_ptr->on_slashcommand.detach(on_slashcommand_handle_);
      }
      if (on_ready_handle_ != 0) {
        cluster_ptr->on_ready.detach(on_ready_handle_);
      }
      if (on_log_handle_ != 0) {
        cluster_ptr->on_log.detach(on_log_handle_);
      }

      logger_->info("Shutting down Discord cluster...");
      cluster_ptr->shutdown();
    }

    logger_->info("Releasing cluster pointer to avoid destructor race (temporary workaround)");
    cluster_.release();

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
          event.edit_response(emojiModuleLib_->getRandomEmoji());
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
            event.edit_response("No RSS/ATOM feed URLs registered.");
            return;
          }

          event.edit_response("Registered RSS/ATOM feed URLs:\n");
          std::vector<std::string> splitMessages;
          if (splitDiscordMessageIfNeeded(urlsList, splitMessages)) {
            for (const auto &msgPart : splitMessages) {
              dpp::message msg(event.command.channel_id, msgPart);
              cluster_->message_create(msg);
            }
          }
        }
        if (cmd_name == "listchannelurls") {
          event.thinking();
          uint64_t channelId = event.command.channel_id;
          std::string urlsList = rssService_->listChannelUrlsAsString(channelId);
          if (urlsList.empty()) {
            event.edit_response("No RSS/ATOM feed URLs registered for this channel.");
            return;
          }

          event.edit_response("Registered RSS/ATOM feed URLs for channel " +
                              std::to_string(channelId) + ":\n");
          std::vector<std::string> splitMessages;
          if (splitDiscordMessageIfNeeded(urlsList, splitMessages)) {
            for (const auto &msgPart : splitMessages) {
              dpp::message msg(event.command.channel_id, msgPart);
              cluster_->message_create(msg);
            }
          }
        }

        if (cmd_name == "getrandomfeed") {
          event.thinking(true);

          dotnamecpp::rss::RSSItem item = rssService_->getRandomItem();
          if (item.title.empty()) {
            const std::string NoItemsMsg = "No RSS items available at the moment.";
            logger_->info(NoItemsMsg);
            event.edit_response(NoItemsMsg);
            return;
          }
          event.edit_response("Fetching a random RSS item...");

          dpp::message msg;
          if (item.embeddedType == dotnamecpp::rss::EmbeddedType::EMBEDDED_NONE) {
            msg = dpp::message(event.command.channel_id, item.toMarkdownLink());
            msg.set_flags(dpp::m_suppress_embeds);
          } else if (item.embeddedType == dotnamecpp::rss::EmbeddedType::EMBEDDED_AS_MARKDOWN) {
            msg = dpp::message(event.command.channel_id, item.toMarkdownLink());
          } else if (item.embeddedType == dotnamecpp::rss::EmbeddedType::EMBEDDED_AS_ADVANCED) {
            msg = dpp::message(event.command.channel_id, item.toEmbed());
          }

          this->postCrossPostedMessage(msg, [this, item](bool success) {
            if (success) {
              logger_->info("CrossPosted random RSS item to Discord: " + item.title);
            } else {
              logger_->error("Failed to crosspost random RSS item to Discord: " + item.title);
            }
          });

          logTheServed(item, [this, item](bool success) {
            if (success) {
              logger_->info("Served RSS item logged successfully: " + item.title);
            } else {
              logger_->error("Failed to log served RSS item: " + item.title);
            }
          });
        }

        if (cmd_name == "addurl") {
          event.thinking();
          auto urlParam = event.get_parameter("url");
          auto embeddedParam = event.get_parameter("embedded_type");

          if (urlParam.index() == 0) {
            event.edit_response("Error: URL parameter is required.");
            return;
          }

          std::string url = std::get<std::string>(urlParam);

          long embeddedType = 0;
          if (embeddedParam.index() != 0) {
            embeddedType = std::get<long>(embeddedParam);
          }

          if (rssService_->addUrl(url, embeddedType, event.command.channel_id)) {
            event.edit_response("Successfully added RSS/ATOM feed URL: " + url +
                                " with embeddedType " + std::to_string(embeddedType));
          } else {
            event.edit_response("Failed to add RSS/ATOM feed URL: " + url);
          }
        }

        if (cmd_name == "modurl") {
          event.thinking();
          auto urlParam = event.get_parameter("url");
          auto embeddedParam = event.get_parameter("embedded_type");

          if (urlParam.index() == 0) {
            event.edit_response("Error: URL parameter is required.");
            return;
          }

          std::string url = std::get<std::string>(urlParam);
          long embeddedType = 0;
          if (embeddedParam.index() != 0) {
            embeddedType = std::get<long>(embeddedParam);
          }

          if (rssService_->modUrl(url, embeddedType, event.command.channel_id)) {
            event.edit_response("Successfully modified RSS/ATOM feed URL: " + url +
                                " to embeddedType " + std::to_string(embeddedType));
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
          cluster_->set_presence(dpp::presence(dpp::ps_online, dpp::at_game, message));
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
          cluster_->set_presence(
              dpp::presence(dpp::ps_online, dpp::at_game, "stopped: " + time_str));

          // Don't call stop() directly from event handler (causes deadlock in DPP thread pool)
          // Just set the running flag to false, the main loop will handle cleanup
          logger_->info("Stop requested via /stopbot command");

          isRunning_.store(false);

          // Zavolat callback pro zastavení orchestratoru
          if (onStopRequested_) {
            onStopRequested_();
          }
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
      dpp_commands.push_back(cmd.toDppCommand(cluster_->me.id));
      logger_->info("Prepared slash command: " + cmd.getName());
    }

    // Capture logger and commands by value to avoid capturing `this` in the
    // callback that DPP may call from worker threads.
    auto cluster_ptr = cluster_.get();
    auto logger_copy = logger_;
    auto commands_copy = commands_;
    cluster_ptr->global_bulk_command_create(
        dpp_commands, [logger_copy, commands_copy](const dpp::confirmation_callback_t &callback) {
      if (callback.is_error()) {
        logger_copy->errorStream()
            << "Failed to register bulk commands: " << callback.get_error().message;
      } else {
        logger_copy->infoStream() << "Successfully registered " << commands_copy.size()
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

  void DiscordBot::logTheServed(rss::RSSItem &item, const std::function<void(bool)> &onComplete) {

    dpp::message msg(LOG_CHANNEL_ID, item.toMarkdownLink());

    // Prevent embed preview for markdown links always
    msg.set_flags(dpp::m_suppress_embeds);

    // Capture cluster raw pointer and logger to avoid capturing `this` in the
    // callbacks that may outlive the DiscordBot object.
    auto *cluster_ptr = cluster_.get();

    cluster_ptr->message_create(
        msg, [logger = logger_, onComplete](const dpp::confirmation_callback_t &callback) {
      if (callback.is_error()) {
        logger->error("Failed to log served RSS item: " + callback.get_error().message);
        if (onComplete) {
          onComplete(false);
        }
      } else {
        if (onComplete) {
          onComplete(true);
        }
      }
    });
  }

  void DiscordBot::postCrossPostedMessage(const dpp::message &msg,
                                          const std::function<void(bool)> &onComplete) {
    // Capture cluster raw pointer and logger to avoid capturing `this` in the
    // callbacks that may outlive the DiscordBot object.
    auto *cluster_ptr = cluster_.get();
    cluster_ptr->message_create(msg, [cluster_ptr, logger = logger_,
                                      onComplete](const dpp::confirmation_callback_t &callback) {
      if (callback.is_error()) {
        logger->error("Failed to create message: " + callback.get_error().message);
        if (onComplete) {
          onComplete(false);
        }
        return;
      }
      const auto &createdMessage = callback.get<dpp::message>();
      cluster_ptr->message_crosspost(
          createdMessage.id, createdMessage.channel_id,
          [logger, onComplete](const dpp::confirmation_callback_t &crosspostCallback) {
        if (crosspostCallback.is_error()) {
          logger->errorStream() << "Failed to crosspost message: "
                                << crosspostCallback.get_error().message;
          if (onComplete) {
            onComplete(false);
          }
          return;
        }
        logger->infoStream() << "Message crossposted successfully.";
        if (onComplete) {
          onComplete(true);
        }
      });
    });
  }

  bool DiscordBot::putRandomFeedTimer() {
    threads_.emplace_back([this]() -> void {
      isPRFTRunning_.store(true);

      while (isPRFTRunning_.load()) {

        // Interruptible sleep
        std::unique_lock<std::mutex> lock(cvMutex_);
        cv_.wait_for(lock, std::chrono::seconds(PUT_INTERVAL_SECONDS),
                     [this]() { return !isPRFTRunning_.load(); });

        dotnamecpp::rss::RSSItem item = rssService_->getRandomItem();
        if (item.title.empty()) {
          logger_->info("No RSS items available at the moment.");
          continue;
        }

        dpp::message msg;
        if (item.embeddedType == dotnamecpp::rss::EmbeddedType::EMBEDDED_NONE) {
          msg = dpp::message(item.discordChannelId, item.toMarkdownLink());
          msg.set_flags(dpp::m_suppress_embeds);
        } else if (item.embeddedType == dotnamecpp::rss::EmbeddedType::EMBEDDED_AS_MARKDOWN) {
          msg = dpp::message(item.discordChannelId, item.toMarkdownLink());
        } else if (item.embeddedType == dotnamecpp::rss::EmbeddedType::EMBEDDED_AS_ADVANCED) {
          msg = dpp::message(item.discordChannelId, item.toEmbed());
        }

        this->postCrossPostedMessage(msg, [this, item](bool success) {
          if (success) {
            logger_->info("CrossPosted random RSS item to Discord: " + item.title);
          } else {
            logger_->error("Failed to crosspost random RSS item to Discord: " + item.title);
          }
        });

        logTheServed(item, [this, item](bool success) {
          if (success) {
            logger_->info("Served RSS item logged successfully: " + item.title);
          } else {
            logger_->error("Failed to log served RSS item: " + item.title);
          }
        });

      } // while isRunningTimer_
    });
    return true;
  }

  bool DiscordBot::fetchFeedsTimer() {
    threads_.emplace_back([this]() -> void {
      isFFTRunning_.store(true);

      while (isFFTRunning_.load()) {

        int itemsFetched = rssService_->refetchRssFeeds();
        if (itemsFetched >= 0) {
          logger_->info("Periodic RSS fetch completed. Total items in buffer: " +
                        std::to_string(rssService_->getItemCount()));
        } else {
          logger_->error("Periodic RSS fetch failed.");
        }

        // Interruptible sleep
        std::unique_lock<std::mutex> lock(cvMutex_);
        cv_.wait_for(lock, std::chrono::seconds(FETCH_INTERVAL_SECONDS),
                     [this]() { return !isFFTRunning_.load(); });

      } // while isRunningTimer_
    });
    return true;
  }

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

} // namespace dotnamecpp::discordbot