#pragma once

#include <Utils/Assets/IAssetManager.hpp>
#include <Utils/Logger/ILogger.hpp>

#include <memory>
#include <random>
#include <string>
#include <vector>

namespace dotnamebot::namegen {

  class NameGen {
  public:
    NameGen(std::shared_ptr<dotnamebot::logging::ILogger> logger,
            std::shared_ptr<dotnamebot::assets::IAssetManager> assetManager);

    ~NameGen() = default;

    NameGen(const NameGen &) = delete;
    NameGen &operator=(const NameGen &) = delete;
    NameGen(NameGen &&) = delete;
    NameGen &operator=(NameGen &&) = delete;

    bool initialize();

    [[nodiscard]] std::string generate() const;

  private:
    std::shared_ptr<dotnamebot::logging::ILogger> logger_;
    std::shared_ptr<dotnamebot::assets::IAssetManager> assetManager_;

    std::vector<std::string> adjectives_;
    std::vector<std::string> nouns_;

    mutable std::mt19937 rng_;

    [[nodiscard]] static std::string capitalize(const std::string &word);
    [[nodiscard]] static std::vector<std::string>
        parseWordList(const std::vector<std::string> &lines);
  };

} // namespace dotnamebot::namegen
