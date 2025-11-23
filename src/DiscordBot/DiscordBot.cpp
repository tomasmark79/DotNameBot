#include "DiscordBot.hpp"

bool DiscordBot::getTokenFromFile (std::string& token) {
  const auto tokenPathOpt = customStrings_->getPath ("dotnamebot.token");
  if (!tokenPathOpt.has_value ()) {
    logger_->error ("Failed to get token file path from custom strings");
    return false;
  }
  const auto& tokenPath = tokenPathOpt.value ();

  if (std::ifstream tokenFile{ tokenPath }; tokenFile.is_open ()) {
    std::getline (tokenFile, token);

    if (token.empty ()) {
      logger_->error ("Token file is empty or invalid: " + tokenPath);
      return false;
    }

    logger_->info ("Token read successfully from: " + tokenPath);
    return true;
  }

  logger_->error ("Failed to open token file: " + tokenPath);
  return false;
}

bool DiscordBot::initialize () {

  logger_->info ("Initializing " + getName () + "...");

  std::string token;
  if (!getTokenFromFile (token)) {
    logger_->error ("Failed to read token from file");
    return false;
  }

  try {
    bot_ = std::make_unique<dpp::cluster> (token, dpp::i_default_intents | dpp::i_message_content);
    bot_->log (dpp::ll_info, "Bot ID: " + std::to_string (bot_->me.id));

    bot_->on_log ([&] (const dpp::log_t& log) {
      if (log.message.find ("Uncaught exception in thread pool") != std::string::npos) {
        logger_->critical ("Uncaught exception in thread pool: " + log.message);
      } else {
        logger_->info ("[DPP] " + log.message);
      }
    });

    setupCommands ();

    return true;
  } catch (const std::exception& e) {
    logger_->error ("Exception during " + getName () + " initialization: " + e.what ());
    return false;
  }
}

bool DiscordBot::start () {
  if (!bot_) {
    logger_->error (getName () + " not initialized, cannot start");
    return false;
  }

  running_.store (true);
  logger_->info ("Starting " + getName () + " with blocking call...");

  try {
    bot_->start (dpp::st_wait);
    running_.store (false);
    logger_->critical ("CRITICAL: Bot has stopped unexpectedly!");
    return false;
  } catch (const std::exception& e) {
    logger_->error ("Exception in " + getName () + " start: " + e.what ());
    running_.store (false);
    return false;
  }
}

bool DiscordBot::stop () {
  // Atomic exchange: if already false (stopped), skip shutdown
  if (!running_.exchange (false)) {
    return true;
  }

  logger_->info ("Stopping " + getName () + "...");

  if (bot_) {
    bot_->shutdown ();
    logger_->info ("Discord bot cluster shutdown initiated");
  }

  logger_->info (getName () + " stopped");
  return true;
}

void DiscordBot::setupCommands () {
  globalCommands_.clear ();
  globalCommands_.push_back (
    dpp::slashcommand ("botprofilestatus", "Set bot profile status", bot_->me.id)
      .add_option (
        dpp::command_option (dpp::co_string, "message", "The message to set as bot status", true)));

  globalCommands_.push_back (dpp::slashcommand ("t2e", "Translate text to English", bot_->me.id)
                               .add_option (dpp::command_option (
                                 dpp::co_string, "message", "The message to translate", true)));

  globalCommands_.push_back (dpp::slashcommand ("t2c", "Translate text to Czech", bot_->me.id)
                               .add_option (dpp::command_option (
                                 dpp::co_string, "message", "The message to translate", true)));

  globalCommands_.push_back (
    dpp::slashcommand ("heygoogle", "Ask Google Gemini", bot_->me.id)
      .add_option (dpp::command_option (dpp::co_string, "prompt", "Prompt", true)));

  globalCommands_.push_back (
    dpp::slashcommand ("runterminalcommand", "Run a terminal command and return the output",
                       bot_->me.id)
      .add_option (
        dpp::command_option (dpp::co_string, "command", "The terminal command to run", true)));

  globalCommands_.push_back (
    dpp::slashcommand ("addsource", "Add a new RSS source", bot_->me.id)
      .add_option (dpp::command_option (dpp::co_string, "url", "URL of the RSS feed", true))
      .add_option (dpp::command_option (dpp::co_boolean, "embedded",
                                        "Whether the feed should be embedded", false)));

  globalCommands_.push_back (dpp::slashcommand ("refetch", "Refetch all RSS feeds", bot_->me.id));
  globalCommands_.push_back (
    dpp::slashcommand ("listsources", "List all RSS sources", bot_->me.id));
  globalCommands_.push_back (
    dpp::slashcommand ("pickupfeed", "Pick a random RSS feed item", bot_->me.id));
  globalCommands_.push_back (dpp::slashcommand ("queue", "Get queue of RSS items", bot_->me.id));
  globalCommands_.push_back (dpp::slashcommand ("btc", "Get Bitcoin price (USD)", bot_->me.id));
  globalCommands_.push_back (dpp::slashcommand ("verse", "Get Bible Kralická", bot_->me.id));
  globalCommands_.push_back (dpp::slashcommand ("emoji", "Get random emoji", bot_->me.id));
  globalCommands_.push_back (dpp::slashcommand ("ping", "Get ping pong!", bot_->me.id));
  globalCommands_.push_back (dpp::slashcommand ("help", "Get list of bot commands", bot_->me.id));
}
