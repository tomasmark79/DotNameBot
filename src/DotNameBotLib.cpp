
#include <DiscordBot/DiscordBot.hpp>
#include <DotNameBotLib/DotNameBotLib.hpp>
#include <Rss/RssManager.hpp>
#include <thread>

namespace dotnamecpp::v1 {

  DotNameBotLib::DotNameBotLib(const UtilsFactory::AppComponents &utilsComponents)
      : logger_(utilsComponents.logger ? utilsComponents.logger
                                       : std::make_shared<dotnamecpp::logging::NullLogger>()),
        assetManager_(utilsComponents.assetManager),
        customStrings_(utilsComponents.customStringsLoader) {

    if (!assetManager_ || !assetManager_->validate()) {
      logger_->errorStream() << "Invalid or missing asset manager";
      return;
    }

    // another services
    emojiModuleLib_ = std::make_shared<dotnamecpp::v1::EmojiModuleLib>(utilsComponents);
    rssService_ = std::make_shared<dotnamecpp::rss::RssManager>(logger_, assetManager_);

    // Register services to service container
    services_ = std::make_unique<ServiceContainer>();
    services_->registerService(logger_);
    services_->registerService(assetManager_);
    services_->registerService(customStrings_);
    services_->registerService(emojiModuleLib_);
    services_->registerService(rssService_);

    logger_->infoStream() << "Total services registered: " << services_->getServiceCount();

    // Initialize orchestrator with default bots
    botOrchestrator_ = std::make_unique<Orchestrator<ILifeCycle>>(*services_);

    auto discordBot = std::make_unique<dotnamecpp::discordbot::DiscordBot>(*services_);

    // callback to notify the DotNameBotLib to stop orchestration
    discordBot->setStopRequestedCallback([this]() {
      logger_->infoStream() << "Stop requested from Discord bot, stopping orchestration...";
      this->isOrchestrating_.store(false);
    });

    botOrchestrator_->add(std::move(discordBot));
    logger_->infoStream() << "Registered " << botOrchestrator_->size() << " bot(s)";

    isInitialized_ = true;
    logger_->infoStream() << libName_ << " initialized successfully. Call run() to start bots.";

    if (this->startOrchestration()) {
      logger_->infoStream() << "Orchestration started successfully during initialization.";
    } else {
      logger_->warningStream() << "Orchestration did not start during initialization. Call "
                                  "startOrchestration() to start.";
    }
  }

  DotNameBotLib::~DotNameBotLib() {
    isOrchestrating_.store(false);
    if (orchestrationThread_.joinable()) {
      orchestrationThread_.join();
    }

    if (isInitialized_) {
      logger_->infoStream() << libName_ << " ... destructed";
    } else {
      logger_->infoStream() << libName_ << " ... (not initialized) destructed";
    }
  }

  bool DotNameBotLib::startOrchestration() {
    if (!isInitialized_) {
      logger_->errorStream() << "Cannot start orchestrator: Library not initialized";
      return false;
    }
    if (isOrchestrating_.load()) {
      logger_->warningStream() << "Orchestrator is already running";
      return true;
    }

    logger_->infoStream() << "Starting orchestrator with " << botOrchestrator_->size()
                          << " bot(s)...";

    orchestrationThread_ = std::thread([this]() {
      try {
        // START
        botOrchestrator_->startAll();
        isOrchestrating_.store(true);
        logger_->infoStream() << "Orchestrator started successfully";

        // RUN LOOP
        while (isOrchestrating_.load()) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // STOP
        if (!botOrchestrator_->isRunning()) {
          logger_->warningStream() << "Orchestrator was stopped externally";
          return;
        }

        botOrchestrator_->stopAll();
        isOrchestrating_.store(false);
        logger_->infoStream() << "Orchestrator stopped successfully";

      } catch (const std::exception &e) {
        logger_->errorStream() << "Error starting orchestrator: " << e.what();
      }
    });
    return true;
  }

  bool DotNameBotLib::isInitialized() const noexcept { return isInitialized_; }

} // namespace dotnamecpp::v1
