#include <DiscordBot/DiscordBot.hpp>
#include <DotNameBotLib/DotNameBotLib.hpp>

namespace dotnamecpp::v1 {

  DotNameBotLib::DotNameBotLib(std::shared_ptr<logging::ILogger> logger,
                               std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager)
      : logger_(logger ? std::move(logger) : std::make_shared<dotnamecpp::logging::NullLogger>()),
        assetManager_(std::move(assetManager)) {

    if (!assetManager_ || !assetManager_->validate()) {
      logger_->errorStream() << "Invalid or missing asset manager";
      isInitialized_ = false;
      return;
    }

    logger_->infoStream() << libName_ << " initialized ...";
    const auto logoPath = assetManager_->resolveAsset("DotNameCppLogo.svg");
    if (assetManager_->assetExists("DotNameCppLogo.svg")) {
      logger_->debugStream() << "Logo: " << logoPath << " found";
    } else {
      logger_->warningStream() << "Logo not found: " << logoPath;
    }

    // Initialize emojies library
    emojiesLib_ = std::make_shared<dotname::EmojiesLib>(assetManager_->getAssetsPath().string());

    // Register services to service container
    services_ = std::make_unique<ServiceContainer>();
    services_->registerService(logger_);
    services_->registerService(assetManager_);
    services_->registerService(emojiesLib_);
    logger_->infoStream() << "Total services registered: " << services_->getServiceCount();

    // Initialize orchestrator with default bots
    botOrchestrator_ = std::make_unique<Orchestrator<ILifeCycle>>(*services_);
    auto discordBot = std::make_unique<DiscordBot>(*services_);
    botOrchestrator_->add(std::move(discordBot));
    logger_->infoStream() << "Registered " << botOrchestrator_->size() << " bot(s)";

    isInitialized_ = true;
    logger_->infoStream() << libName_ << " initialized successfully. Call run() to start bots.";
  }

  DotNameBotLib::~DotNameBotLib() {
    if (isInitialized_) {
      // Ensure graceful shutdown
      stop();
      logger_->infoStream() << libName_ << " destructed";
    } else {
      logger_->infoStream() << libName_ << " (not initialized) destructed";
    }
  }

  DotNameBotLib::DotNameBotLib(DotNameBotLib &&other) noexcept
      : logger_(std::move(other.logger_)), assetManager_(std::move(other.assetManager_)),
        assetsPath_(std::move(other.assetsPath_)), isInitialized_(other.isInitialized_),
        shouldStop_(other.shouldStop_.load()), services_(std::move(other.services_)),
        botOrchestrator_(std::move(other.botOrchestrator_)),
        emojiesLib_(std::move(other.emojiesLib_)) {
    other.isInitialized_ = false;
    other.shouldStop_.store(false);
    if (logger_) {
      logger_->infoStream() << libName_ << " move constructed";
    }
  }

  DotNameBotLib &DotNameBotLib::operator=(DotNameBotLib &&other) noexcept {
    if (this != &other) {
      // Stop current instance
      if (isInitialized_) {
        stop();
      }

      // Move all members
      logger_ = std::move(other.logger_);
      assetManager_ = std::move(other.assetManager_);
      assetsPath_ = std::move(other.assetsPath_);
      isInitialized_ = other.isInitialized_;
      shouldStop_.store(other.shouldStop_.load());
      services_ = std::move(other.services_);
      botOrchestrator_ = std::move(other.botOrchestrator_);
      emojiesLib_ = std::move(other.emojiesLib_);

      other.isInitialized_ = false;
      other.shouldStop_.store(false);
      if (logger_) {
        logger_->infoStream() << libName_ << " move assigned";
      }
    }
    return *this;
  }

  bool DotNameBotLib::run(int durationSeconds) {
    if (!isInitialized_) {
      logger_->errorStream() << "Cannot run: " << libName_ << " is not initialized";
      return false;
    }

    // Reset stop flag
    shouldStop_.store(false);

    try {
      // Start all bots
      botOrchestrator_->startAll();
      logger_->infoStream() << libName_ << " started all bots successfully";

      // Run for specified duration
      if (durationSeconds > 0) {
        logger_->infoStream() << "Running for " << durationSeconds << " seconds...";
        for (int i = 0; i < durationSeconds && !shouldStop_.load(); ++i) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (shouldStop_.load()) {
          logger_->infoStream() << libName_ << " stopped by user request";
        } else {
          logger_->infoStream() << libName_ << " finished after " << durationSeconds << " seconds";
        }
        stop();
      } else {
        logger_->infoStream() << "Running indefinitely. Call stop() to terminate.";
        constexpr int pollIntervalMs = 100;
        while (!shouldStop_.load()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        }
        logger_->infoStream() << libName_ << " stopped";
      }

      return true;
    } catch (const std::exception &e) {
      logger_->errorStream() << "Error running " << libName_ << ": " << e.what();
      return false;
    }
  }

  void DotNameBotLib::stop() {
    if (!isInitialized_) {
      logger_->warningStream() << "Cannot stop: " << libName_ << " is not initialized";
      return;
    }

    logger_->infoStream() << "Stopping " << libName_ << "...";
    shouldStop_.store(true);

    if (botOrchestrator_) {
      try {
        botOrchestrator_->stopAll();
        logger_->infoStream() << libName_ << " stopped all bots successfully";
      } catch (const std::exception &e) {
        logger_->errorStream() << "Error stopping bots: " << e.what();
      }
    }
  }

  bool DotNameBotLib::isInitialized() const noexcept { return isInitialized_; }
  const std::filesystem::path &DotNameBotLib::getAssetsPath() const noexcept { return assetsPath_; }
  ServiceContainer &DotNameBotLib::getServices() noexcept { return *services_; }
  Orchestrator<ILifeCycle> &DotNameBotLib::getOrchestrator() noexcept { return *botOrchestrator_; }

} // namespace dotnamecpp::v1
