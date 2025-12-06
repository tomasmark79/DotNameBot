#pragma once
#include <DotNameBotLib/version.h> // first configuration will create this file

#include <ServiceContainer/ServiceContainer.hpp>

#include <DiscordBot/DiscordBot.hpp>
#include <Orchestrator/Orchestrator.hpp>

#include <Utils/UtilsFactory.hpp>
#include <filesystem>
#include <memory>
#include <string>

#include <EmojiesLib/EmojiesLib.hpp>

namespace dotnamecpp::v1 {
  class DotNameBotLib {
  public:
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

      // Loading external CPM.cmake emojies library
      emojiesLib_ = std::make_shared<dotname::EmojiesLib>(assetManager_->getAssetsPath().string());

      // service container
      services_ = std::make_unique<ServiceContainer>();
      services_->registerService(logger_);
      services_->registerService(assetManager_);
      services_->registerService(emojiesLib_);

      logger_->infoStream() << "Total services registered: " << services_->getServiceCount();

      botOrchestrator_ = std::make_unique<Orchestrator<ILifeCycle>>(*services_);
      auto discordBot = std::make_unique<DiscordBot>(*services_);
      botOrchestrator_->add(std::move(discordBot));
      logger_->infoStream() << "Registered " << botOrchestrator_->size() << " bot(s)";

      // Start services first, then bots
      try {
        botOrchestrator_->startAll();
        logger_->infoStream() << libName_ << " started all bots successfully";

        isInitialized_ = true;

      } catch (const std::exception &e) {
        logger_->errorStream() << "Error starting " << libName_ << ": " << e.what();
      }

      // 30 seconds debug stopgap
      std::this_thread::sleep_for(std::chrono::seconds(30));

      // Stop bots first, then services (reverse order)
      try {
        botOrchestrator_->stopAll();
        logger_->infoStream() << libName_ << " stopped all bots successfully";

      } catch (const std::exception &e) {
        logger_->errorStream() << "Error stopping " << libName_ << ": " << e.what();
      }
    }

    // Destructor
    ~DotNameBotLib() {
      if (isInitialized_) {
        // Ensure graceful shutdown in destructor
        try {
          if (botOrchestrator_) {
            botOrchestrator_->stopAll();
          }
        } catch (const std::exception &e) {
          if (logger_) {
            logger_->errorStream() << "Error in destructor: " << e.what();
          }
        }
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

  private:
    const std::string libName_ = "DotNameBotLib v." DOTNAMEBOTLIB_VERSION;
    std::shared_ptr<dotnamecpp::logging::ILogger> logger_;
    std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager_;
    std::shared_ptr<dotname::EmojiesLib> emojiesLib_;

    std::filesystem::path assetsPath_;
    bool isInitialized_ = false;

    std::unique_ptr<ServiceContainer> services_;
    std::unique_ptr<Orchestrator<ILifeCycle>> botOrchestrator_;
  };

} // namespace dotnamecpp::v1
