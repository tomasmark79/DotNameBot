#pragma once

#include "Emoji.hpp"
#include <Utils/UtilsFactory.hpp>

#include <memory>

namespace dotnamebot::v1 {
  using namespace dotnamebot::utils;
  class EmojiModuleLib {

  public:
    EmojiModuleLib(const UtilsFactory::ApplicationContext &context);
    ~EmojiModuleLib();
    EmojiModuleLib(const EmojiModuleLib &other) = delete;
    EmojiModuleLib &operator=(const EmojiModuleLib &other) = delete;
    EmojiModuleLib(EmojiModuleLib &&other) = delete;
    EmojiModuleLib &operator=(EmojiModuleLib &&other) = delete;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]]
    const std::shared_ptr<dotnamebot::assets::IAssetManager> &getAssetManager() const noexcept;

    std::unique_ptr<dotnamebot::emoji::Emoji> emoji_;

    // Public Emoji methods
    [[nodiscard]] std::string getRandomEmoji() const;
    [[nodiscard]] std::string getEmoji() const;
    [[nodiscard]] std::string getEmoji(char32_t *code, size_t totalCodePoints) const;
    [[nodiscard]] std::string getEmoji(int32_t *code, size_t totalCodePoints) const;

  private:
    bool isInitialized_{false};
    static constexpr const char *libName_ = "EmojiModuleLib v0.0.1";

    std::shared_ptr<dotnamebot::logging::ILogger> logger_;
    std::shared_ptr<dotnamebot::assets::IAssetManager> assetManager_;
  };

} // namespace dotnamebot::v1
