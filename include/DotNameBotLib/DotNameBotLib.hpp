#pragma once

#include <Utils/UtilsFactory.hpp>

#include <memory>

namespace dotnamebot::v1 {
  using namespace dotnamebot::utils;
  class DotNameBotLib {

  public:
    DotNameBotLib(const UtilsFactory::ApplicationContext &context);
    ~DotNameBotLib();
    DotNameBotLib(const DotNameBotLib &other) = delete;
    DotNameBotLib &operator=(const DotNameBotLib &other) = delete;
    DotNameBotLib(DotNameBotLib &&other) = delete;
    DotNameBotLib &operator=(DotNameBotLib &&other) = delete;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]]
    const std::shared_ptr<dotnamebot::assets::IAssetManager> &getAssetManager() const noexcept;
    void performLibraryTask();

  private:
    bool isInitialized_{false};
    static constexpr const char *libName_ = "DotNameBotLib v0.0.1";

    std::shared_ptr<dotnamebot::logging::ILogger> logger_;
    std::shared_ptr<dotnamebot::assets::IAssetManager> assetManager_;
  };

} // namespace dotnamebot::v1
