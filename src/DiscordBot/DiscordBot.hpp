#pragma once

#include <IBot/IBot.hpp>
#include <Utils/UtilsFactory.hpp>
#include <atomic>
#include <dpp/dpp.h>
#include <memory>
#include <string>
#include <vector>

class DiscordBot : public IBot {
public:
  DiscordBot (std::shared_ptr<dotnamecpp::logging::ILogger> logger,
              std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager,
              std::shared_ptr<dotnamecpp::utils::ICustomStringsLoader> customStrings = nullptr)
    : logger_ (logger ? std::move (logger) : std::make_shared<dotnamecpp::logging::NullLogger> ())
    , assetManager_ (std::move (assetManager))
    , customStrings_ (std::move (customStrings)) {
  }

  ~DiscordBot () override {
    stop ();
  }

  // IBot interface declarations
  bool initialize () override;
  bool start () override;
  bool stop () override;

  [[nodiscard]]
  bool isRunning () const override {
    return running_.load ();
  }
  [[nodiscard]]
  std::string getName () const override {
    return "D++ Discord Bot";
  }

private:
  std::shared_ptr<dotnamecpp::logging::ILogger> logger_;
  std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager_;
  std::shared_ptr<dotnamecpp::utils::ICustomStringsLoader> customStrings_;

  bool getTokenFromFile (std::string& token);
  std::unique_ptr<dpp::cluster> bot_;
  std::atomic<bool> running_{ false };
  std::vector<dpp::slashcommand> globalCommands_;
  void setupCommands ();
};
