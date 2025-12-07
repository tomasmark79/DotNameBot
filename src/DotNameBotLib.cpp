#include <DiscordBot/DiscordBot.hpp>
#include <DotNameBotLib/DotNameBotLib.hpp>

#include <filesystem>
#include <memory>

namespace dotnamecpp::v1 {
  DotNameBotLib::DotNameBotLib(std::shared_ptr<logging::ILogger> logger,
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

    // Starting all bots
    try {
      botOrchestrator_->startAll();
      logger_->infoStream() << libName_ << " started all bots successfully";

      isInitialized_ = true;

    } catch (const std::exception &e) {
      logger_->errorStream() << "Error starting " << libName_ << ": " << e.what();
    }

    // 30 seconds debug stopgap
    std::this_thread::sleep_for(std::chrono::seconds(30));

    // Stopping all bots
    try {
      botOrchestrator_->stopAll();
      logger_->infoStream() << libName_ << " stopped all bots successfully";

    } catch (const std::exception &e) {
      logger_->errorStream() << "Error stopping " << libName_ << ": " << e.what();
    }
  }

  DotNameBotLib::~DotNameBotLib() {
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

  DotNameBotLib::DotNameBotLib(DotNameBotLib &&other) noexcept
      : logger_(std::move(other.logger_)), assetManager_(std::move(other.assetManager_)),
        assetsPath_(std::move(other.assetsPath_)), isInitialized_(other.isInitialized_) {
    other.isInitialized_ = false;
    if (logger_) {
      logger_->infoStream() << libName_ << " move constructed";
    }
  }

  DotNameBotLib &DotNameBotLib::operator=(DotNameBotLib &&other) noexcept {
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

  bool DotNameBotLib::isInitialized() const noexcept {
    return isInitialized_;
  }

  const std::filesystem::path &DotNameBotLib::getAssetsPath() const noexcept {
    return assetsPath_;
  }

} // namespace dotnamecpp::v1
