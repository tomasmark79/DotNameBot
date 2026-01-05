#pragma once

#include <DotNameBotLib/version.h> // cmake configuration will generate this file
#include <Utils/UtilsFactory.hpp>

#include <EmojiModuleLib/EmojiModuleLib.hpp>
#include <Orchestrator/Orchestrator.hpp>

#include <Rss/IRssService.hpp>
#include <ServiceContainer/ServiceContainer.hpp>

#include <memory>

namespace dotnamecpp::v1 {
  using namespace dotnamecpp::utils;
  class DotNameBotLib {

  public:
    DotNameBotLib(const UtilsFactory::ApplicationContext &context);
    ~DotNameBotLib();

    DotNameBotLib(const DotNameBotLib &other) = delete;
    DotNameBotLib &operator=(const DotNameBotLib &other) = delete;
    DotNameBotLib(DotNameBotLib &&other) = delete;
    DotNameBotLib &operator=(DotNameBotLib &&other) = delete;

    [[nodiscard]] bool startOrchestration();
    [[nodiscard]] bool isInitialized() const noexcept;

  private:
    bool isInitialized_{false};
    static constexpr const char *libName_ = "DotNameBotLib v" DOTNAMEBOTLIB_VERSION;

    std::shared_ptr<dotnamecpp::logging::ILogger> logger_;
    std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager_;
    std::shared_ptr<dotnamecpp::utils::ICustomStringsLoader> customStrings_;

    std::unique_ptr<ServiceContainer> services_;
    std::unique_ptr<Orchestrator<ILifeCycle>> botOrchestrator_;
    std::shared_ptr<dotnamecpp::rss::IRssService> rssService_;
    std::shared_ptr<dotnamecpp::v1::EmojiModuleLib> emojiModuleLib_;

    std::atomic<bool> isOrchestrating_{false};
    std::thread orchestrationThread_;
  };

} // namespace dotnamecpp::v1
