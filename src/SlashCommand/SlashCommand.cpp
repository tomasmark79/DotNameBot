#include "SlashCommand.hpp"

// Global commands definition

const std::vector<SlashCommand> commands_ = {
    {"ping", "get pong", "simple"},
    {"help", "get help", "simple"},
    {"emoji", "get emoji", "simple"},
    {"addurl",
     "add another RSS/ATOM feed URL",
     {{OptionType::String, "url", "URL of the RSS/ATOM feed", true, {}, {}, {}},
      {OptionType::Bool, "embedded", "Whether the feed should be embedded", false, {}, {}, {}}},
     "rss"},
    {"modurl",
     "modify an existing RSS/ATOM feed URL",
     {{OptionType::String, "url", "Existing URL of the RSS/ATOM feed", true, {}, {}, {}},
      {OptionType::Bool, "embedded", "Whether the feed should be embedded", false, {}, {}, {}}},
     "rss"},
    {"removeurl",
     "remove an RSS/ATOM feed URL",
     {{OptionType::String, "url", "URL of the RSS/ATOM feed to remove", true, {}, {}, {}}},
     "rss"},
    {"refetch", "refetch RSS/ATOM feeds", "rss"},
    {"listurls", "get list of RSS/ATOM feed URLs", "rss"},
    {"getrandomfeed", "get random RSS/ATOM feed item", "rss"},
    {"gettotalfeeds", "get count of RSS/ATOM feed items", "rss"},
    {"uptime", "get bot uptime", "botself"},
    {"stopbot", "stop the bot", "botself"},
    {"setstatus",
     "set bot status message",
     {{OptionType::String, "message", "The status message", true, {}, {}, {}}},
     "botself"}};

dpp::slashcommand SlashCommand::toDppCommand(dpp::snowflake bot_id) const {
  dpp::slashcommand cmd(name_, description_, bot_id);

  for (const auto &opt : options_) {
    dpp::command_option dpp_opt(toCommandOptionType(opt.type), opt.name, opt.description,
                                opt.required);

    for (const auto &[choice_name, choice_value] : opt.choices) {
      dpp_opt.add_choice(dpp::command_option_choice(choice_name, choice_value));
    }

    if (opt.minValue) dpp_opt.set_min_value(*opt.minValue);
    if (opt.maxValue) dpp_opt.set_max_value(*opt.maxValue);

    cmd.add_option(dpp_opt);
  }

  cmd.set_default_permissions(defaultPermissions_);
  cmd.set_dm_permission(dmPermission_);

  return cmd;
}