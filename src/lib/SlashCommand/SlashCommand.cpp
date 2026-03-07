#include "SlashCommand.hpp"

// Global commands definition

const std::vector<SlashCommand> commands_ = {
    {"ping", "get pong", "simple"},
    {"help", "get help", "simple"},
    {"emoji", "get emoji", "simple"},
    {"addurl",
     "add another RSS/ATOM feed URL",
     {{.type = OptionType::String,
       .name = "url",
       .description = "URL of the RSS/ATOM feed",
       .required = true,
       .choices = {},
       .minValue = {},
       .maxValue = {}},
      {.type = OptionType::Integer,
       .name = "embedded_type",
       .description = "Whether the feed should be embeddedType 0,1,2",
       .required = false,
       .choices = {},
       .minValue = {},
       .maxValue = {}}},
     "rss"},
    {"modurl",
     "modify an existing RSS/ATOM feed URL",
     {{.type = OptionType::String,
       .name = "url",
       .description = "Existing URL of the RSS/ATOM feed",
       .required = true,
       .choices = {},
       .minValue = {},
       .maxValue = {}},
      {.type = OptionType::Integer,
       .name = "embedded_type",
       .description = "Whether the feed should be embeddedType 0,1,2",
       .required = false,
       .choices = {},
       .minValue = {},
       .maxValue = {}}},
     "rss"},
    {"remurl",
     "remove an existing RSS/ATOM feed URL",
     {{.type = OptionType::String,
       .name = "url",
       .description = "Existing URL of the RSS/ATOM feed",
       .required = true,
       .choices = {},
       .minValue = {},
       .maxValue = {}}},
     "rss"},
    {"refetch", "refetch RSS/ATOM feeds", "rss"},
    {"listurls", "get list of RSS/ATOM feed URLs", "rss"},
    {"listchannelurls", "get list of RSS/ATOM feed URLs for a specific channel", "rss"},
    {"getrandomfeed", "get random RSS/ATOM feed item", "rss"},
    {"gettotalfeeds", "get count of RSS/ATOM feed items", "rss"},
    {"uptime", "get bot uptime", "botself"},
    {"stopbot", "stop the bot", "botself"},
    {"setstatus",
     "set bot status message",
     {{.type = OptionType::String,
       .name = "message",
       .description = "The status message",
       .required = true,
       .choices = {},
       .minValue = {},
       .maxValue = {}}},
     "botself"}};

dpp::slashcommand SlashCommand::toDppCommand(dpp::snowflake bot_id) const {
  dpp::slashcommand cmd(name_, description_, bot_id);

  for (const auto &opt : options_) {
    dpp::command_option dpp_opt(toCommandOptionType(opt.type), opt.name, opt.description,
                                opt.required);

    for (const auto &[choice_name, choice_value] : opt.choices) {
      dpp_opt.add_choice(dpp::command_option_choice(choice_name, choice_value));
    }

    if (opt.minValue) {
      dpp_opt.set_min_value(*opt.minValue);
    }
    if (opt.maxValue) {
      dpp_opt.set_max_value(*opt.maxValue);
    }

    cmd.add_option(dpp_opt);
  }

  cmd.set_default_permissions(defaultPermissions_);
  cmd.set_dm_permission(dmPermission_);

  return cmd;
}