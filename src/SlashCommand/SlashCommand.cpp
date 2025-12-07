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
    {"removeurl",
     "remove an RSS/ATOM feed URL",
     {{OptionType::String, "url", "URL of the RSS/ATOM feed to remove", true, {}, {}, {}}},
     "rss"},
    {"refetch", "manually refetch RSS/ATOM feeds", "rss"},
    {"listurls", "get list of RSS/ATOM feed URLs", "rss"},
    {"printfeed", "get random RSS/ATOM feed items", "rss"},
    {"printcountfeeds", "get count of RSS/ATOM feed items", "rss"},
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

    if (opt.min_value) dpp_opt.set_min_value(*opt.min_value);
    if (opt.max_value) dpp_opt.set_max_value(*opt.max_value);

    cmd.add_option(dpp_opt);
  }

  cmd.set_default_permissions(default_permissions_);
  cmd.set_dm_permission(dm_permission_);

  return cmd;
}