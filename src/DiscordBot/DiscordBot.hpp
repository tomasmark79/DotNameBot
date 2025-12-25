#pragma once

#include "Rss/RSSItem.hpp"
#include <SlashCommand/SlashCommand.hpp>
#include <dpp/dpp.h>

#include <ILifeCycle/ILifeCycle.hpp>
#include <ServiceContainer/ServiceContainer.hpp>

#include <Rss/IRssService.hpp>
#include <Rss/RssManager.hpp>
#include <Utils/UtilsFactory.hpp>

#include <atomic>
#include <memory>
#include <string>

#include <EmojiesLib/EmojiesLib.hpp>

namespace dotnamecpp::discordbot {

  class DiscordBot : public ILifeCycle {
  public:
    explicit DiscordBot(ServiceContainer &services)
        : logger_(services.getService<dotnamecpp::logging::ILogger>()),
          assetManager_(services.getService<dotnamecpp::assets::IAssetManager>()),
          emojiesLib_(services.getService<dotname::EmojiesLib>()),
          rssService_(services.getService<dotnamecpp::rss::IRssService>()) {

      if (!logger_) {
        throw std::runtime_error("DiscordBot requires a logger");
      }

      if (!assetManager_) {
        throw std::runtime_error("DiscordBot requires an asset manager");
      }

      if (emojiesLib_) {
        logger_->infoStream() << "DiscordBot initialized with EmojiesLib, random emoji: "
                              << emojiesLib_->getRandomEmoji();
      } else {
        throw std::runtime_error("DiscordBot requires an EmojiesLib");
      }

      // CustomStrings loader is created on-demand
      customStrings_ = dotnamecpp::utils::UtilsFactory::createCustomStringsLoader(assetManager_);
    }

    ~DiscordBot() override { stop(); }

    // ILifeCycle interface declarations

    /**
     * @brief Initialize the Discord bot
     *
     * @return true
     * @return false
     */
    bool initialize() override;

    /**
     * @brief Start the Discord bot
     *
     * @return true
     * @return false
     */
    bool start() override;

    /**
     * @brief Stop the Discord bot
     *
     * @return true
     * @return false
     */
    bool stop() override;

    /**
     * @brief Check if the Discord bot is running
     *
     * @return true
     * @return false
     */
    [[nodiscard]]
    bool isRunning() const override {
      return isRunning_.load();
    }

    /**
     * @brief Get the Name object
     *
     * @return std::string
     */
    [[nodiscard]]
    std::string getName() const override {
      return "DiscordBot.hpp";
    }

  private:
    /**
     * @brief Register bulk slash commands to Discord
     *        More efficient way to register multiple commands at once
     *
     */
    void registerBulkSlashCommandsToDiscord();

    /**
     * @brief Put a random feed timer
     *
     * @return true
     * @return false
     */
    bool putRandomFeedTimer();

    /**
     * @brief Fetch feeds timer
     *
     * @return true
     * @return false
     */
    bool fetchFeedsTimer();

    constexpr static const int MAX_DISCORD_MESSAGE_LENGTH = 2000;

    std::unique_ptr<dpp::cluster> bot_;

    std::shared_ptr<dotnamecpp::logging::ILogger> logger_;
    std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager_;
    std::shared_ptr<dotnamecpp::utils::ICustomStringsLoader> customStrings_;
    std::shared_ptr<dotname::EmojiesLib> emojiesLib_;
    std::shared_ptr<dotnamecpp::rss::IRssService> rssService_;

    bool getTokenFromFile(std::string &token);
    void handleSlashCommand(const dpp::slashcommand_t &event);
    static bool splitDiscordMessageIfNeeded(const std::string &message,
                                            std::vector<std::string> &outMessages);
    void logTheServed(rss::RSSItem &item, std::function<void(bool)> onComplete);
    std::atomic<bool> isRunning_{false};
    std::chrono::time_point<std::chrono::system_clock> startTime_;

    std::vector<std::thread> threads_;
    std::atomic<bool> isRunningTimer_{true};
    std::atomic<bool> isFetchFeedsTimerRunning_{true};
  };
} // namespace dotnamecpp::discordbot