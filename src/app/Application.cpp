#include <DiscordBot/DiscordBot.hpp>
//#include <DotNameBotLib/DotNameBotLib.hpp>
#include <EmojiModuleLib/EmojiModuleLib.hpp>
#include <ILifeCycle/ILifeCycle.hpp>
#include <Orchestrator/Orchestrator.hpp>
#include <Rss/IRssService.hpp>
#include <Rss/RssManager.hpp>
#include <ServiceContainer/ServiceContainer.hpp>
#include <Utils/UtilsFactory.hpp>
#include <csignal>
#include <cxxopts.hpp>
#include <iostream>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

const std::string appName = "DotNameBot";
const std::string NA = "[Not Found]";

int main(int argc, char **argv) {
  using namespace dotnamebot;
  using namespace dotnamebot::logging;
  using namespace dotnamebot::utils;

  try {
    // ---
    cxxopts::Options options(appName, "DotNameBot C++ Application");
    options.add_options()("h,help", "Print usage");
    options.add_options()("w,write2file", "Write output to file",
                          cxxopts::value<bool>()->default_value("false"));
    auto result = options.parse(argc, argv);

#if !defined(__EMSCRIPTEN__)
    if (result.contains("help")) {
#else
    if (result.count("help") > 0) {
#endif
      std::cout << options.help() << '\n';
      return EXIT_SUCCESS;
    }

    // --- Application context (logger, asset manager, custom strings, ...)
    auto ctx = UtilsFactory::createFullContext(
        appName, LoggerConfig{.level = Level::LOG_INFO,
                              .enableFileLogging = result["write2file"].as<bool>(),
                              .logFilePath = "application.log",
                              .colorOutput = true,
                              .appPrefix = appName});

    ctx.logger->infoStream()
        << appName << " (c) "
        << ctx.customStringsLoader->getLocalizedString("Author", "cs").value_or(NA) << " - "
        << ctx.customStringsLoader->getLocalizedString("GitHub", "cs").value_or(NA) << ": "
        << ctx.customStringsLoader->getCustomKey("GitHub", "url").value_or(NA);
    ctx.logger->infoStream() << ctx.platformInfo->getPlatformName() << " platform detected.";

    // --- Service container
    ServiceContainer services;
    services.registerService<dotnamebot::logging::ILogger>(ctx.logger);
    services.registerService<dotnamebot::assets::IAssetManager>(ctx.assetManager);
    services.registerService<dotnamebot::utils::ICustomStringsLoader>(ctx.customStringsLoader);

    // EmojiModuleLib
    auto emojiLib = std::make_shared<dotnamebot::v1::EmojiModuleLib>(ctx);
    if (!emojiLib->isInitialized()) {
      ctx.logger->errorStream() << "EmojiModuleLib initialization failed";
      return EXIT_FAILURE;
    }
    services.registerService<dotnamebot::v1::EmojiModuleLib>(emojiLib);

    // RssManager (registered as IRssService interface)
    auto rssManager = std::make_shared<dotnamebot::rss::RssManager>(ctx.logger, ctx.assetManager);
    services.registerService<dotnamebot::rss::IRssService>(rssManager);

    ctx.logger->infoStream() << "Services registered: " << services.getServiceCount();

    // --- Orchestrator
    Orchestrator<ILifeCycle> orchestrator(services);

    // DiscordBot – callback for /stopbot slash command
    auto discordBot = std::make_unique<dotnamebot::discordbot::DiscordBot>(services);
    discordBot->setStopRequestedCallback([&orchestrator]() { orchestrator.stopAll(); });
    orchestrator.add(std::move(discordBot));

    // --- Signal handling: block SIGINT/SIGTERM in ALL threads (including DPP's),
    // then wait for them in the main thread via sigwait. This guarantees the
    // signal is always caught here regardless of which thread DPP creates.
#if !defined(__EMSCRIPTEN__)
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
#endif

    // --- Start all lifecycle services
    orchestrator.startAll();

#if !defined(__EMSCRIPTEN__)
    // Main thread blocks here until SIGINT or SIGTERM arrives.
    int sig = 0;
    sigwait(&sigset, &sig);
    ctx.logger->infoStream() << "Signal " << sig << " received, stopping orchestrator...";
    orchestrator.stopAll();
#endif

    // ---
    ctx.logger->infoStream() << appName << " ... shutting down";
    return EXIT_SUCCESS;

  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
}
