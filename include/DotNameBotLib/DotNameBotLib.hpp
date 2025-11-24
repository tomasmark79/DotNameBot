#pragma once

#include <DotNameBotLib/version.h> // first configuration will create this file

#include <BotOrchestrator/BotOrchestrator.hpp>
#include <DiscordBot/DiscordBot.hpp>
#include <EmojiesLib/EmojiesLib.hpp>
#include <ServiceContainer/ServiceContainer.hpp>
#include <Utils/UtilsFactory.hpp>
#include <filesystem>
#include <memory>
#include <string>

namespace dotnamecpp::v1 {
  class DotNameBotLib {
  public:
    // Constructor
    DotNameBotLib(std::shared_ptr<logging::ILogger> logger,
                  std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager)
        : logger_(logger ? std::move(logger) : std::make_shared<dotnamecpp::logging::NullLogger>()),
          assetManager_(std::move(assetManager)) {
      if (assetManager_ && assetManager_->validate()) {
        logger_->infoStream() << libName_ << " initialized ...";
        const auto logoPath = assetManager_->resolveAsset("DotNameCppLogo.svg");
        if (assetManager_->assetExists("DotNameCppLogo.svg")) {
          logger_->debugStream() << "Logo: " << logoPath << " found";
        } else {
          logger_->warningStream() << "Logo not found: " << logoPath;
        }
      } else {
        logger_->errorStream() << "Invalid or missing asset manager";
      }
      // Welcome message with random emoji
      dotname::EmojiesLib emojies(assetManager_->getAssetsPath().string());
      logger_->infoStream() << "Random emoji: " << emojies.getRandomEmoji();

      // Services
      services_ = std::make_unique<ServiceContainer>();

      // Orchestrator
      orchestrator_ = std::make_unique<BotOrchestrator>(*services_);
      auto customStrings =
          dotnamecpp::utils::UtilsFactory::createCustomStringsLoader(assetManager_);
      auto discordBot = std::make_unique<DiscordBot>(logger_, assetManager_, customStrings);
      orchestrator_->registerBot(std::move(discordBot));

      // Start
      try {
        orchestrator_->startAll();
        logger_->infoStream() << libName_ << " started all bots successfully";

      } catch (const std::exception &e) {
        logger_->errorStream() << "Error starting " << libName_ << ": " << e.what();
      }

      // 30 seconds debug stopgap
      std::this_thread::sleep_for(std::chrono::seconds(30));

      // Stop
      try {
        orchestrator_->stopAll();
        logger_->infoStream() << libName_ << " stopped all bots successfully";
      } catch (const std::exception &e) {
        logger_->errorStream() << "Error stopping " << libName_ << ": " << e.what();
      }
    }

    // Destructor
    ~DotNameBotLib() {
      if (isInitialized_) {
        logger_->infoStream() << libName_ << " destructed";
      } else {
        logger_->infoStream() << libName_ << " (not initialized) destructed";
      }
    }

    // Non-copyable
    DotNameBotLib(const DotNameBotLib &other) = delete;
    DotNameBotLib &operator=(const DotNameBotLib &other) = delete;

    // Move is allowed
    DotNameBotLib(DotNameBotLib &&other) noexcept
        : logger_(std::move(other.logger_)), assetManager_(std::move(other.assetManager_)),
          assetsPath_(std::move(other.assetsPath_)), isInitialized_(other.isInitialized_) {
      other.isInitialized_ = false;
      if (logger_) {
        logger_->infoStream() << libName_ << " move constructed";
      }
    }

    // Move assignment allowed
    DotNameBotLib &operator=(DotNameBotLib &&other) noexcept {
      if (this != &other) {
        logger_ = std::move(other.logger_);
        assetManager_ = std::move(other.assetManager_);
        assetsPath_ = std::move(other.assetsPath_);
        isInitialized_ = other.isInitialized_;
        other.isInitialized_ = false;
        if (logger_) {
          logger_->infoStream() << libName_ << " move assigned";
        }
      }
      return *this;
    }

    /**
     * @brief Check if the DotNameBotLib object is initialized
     *
     * @return true
     * @return false
     */
    [[nodiscard]]
    bool isInitialized() const noexcept {
      return isInitialized_;
    }

    /**
     * @brief Get the Assets Path object
     *
     * @return const std::filesystem::path&
     */
    [[nodiscard]]
    const std::filesystem::path &getAssetsPath() const noexcept {
      return assetsPath_;
    }

    /**
     * @brief Setup bots
     *
     */
    void setupBots() {
      // Example: Setup a DotName bot
      logger_->infoStream() << "Setting up DotName bot...";

      // Bot setup logic goes here

      isInitialized_ = true;
      logger_->infoStream() << "DotName bot setup complete.";
    }

  private:
    const std::string libName_ = "DotNameBotLib v." DOTNAMEBOTLIB_VERSION;
    std::shared_ptr<dotnamecpp::logging::ILogger> logger_;
    std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager_;
    std::filesystem::path assetsPath_;
    bool isInitialized_ = false;

    std::unique_ptr<ServiceContainer> services_;
    std::unique_ptr<BotOrchestrator> orchestrator_;
  };

} // namespace dotnamecpp::v1
