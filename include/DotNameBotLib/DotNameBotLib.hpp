#pragma once
#include <DotNameBotLib/version.h> // first configuration will create this file
#include <EmojiesLib/EmojiesLib.hpp>
#include <Orchestrator/Orchestrator.hpp>
#include <ServiceContainer/ServiceContainer.hpp>
#include <Utils/UtilsFactory.hpp>

#include <string>

namespace dotnamecpp::v1 {
  class DotNameBotLib {

  public:
    /**
     * @brief Construct a new object
     *
     * @param logger
     * @param assetManager
     */
    DotNameBotLib(std::shared_ptr<logging::ILogger> logger,
                  std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager);

    /**
     * @brief Destroy the object
     *
     */
    ~DotNameBotLib();

    /**
     * @brief Don't allow copy construction
     *
     * @param other
     */
    DotNameBotLib(const DotNameBotLib &other) = delete;

    /**
     * @brief Don't allow copy assignment
     *
     * @param other
     * @return DotNameBotLib&
     */
    DotNameBotLib &operator=(const DotNameBotLib &other) = delete;

    /**
     * @brief Construct a new object by moving
     *
     * @param other
     */
    DotNameBotLib(DotNameBotLib &&other) noexcept;

    /**
     * @brief Move assignment operator - default
     *
     * @param other
     * @return DotNameBotLib&
     */
    DotNameBotLib &operator=(DotNameBotLib &&other) noexcept;

    /**
     * @brief Check if the object is initialized
     *
     * @return true
     * @return false
     */
    [[nodiscard]]
    bool isInitialized() const noexcept;

    /**
     * @brief Get the Assets Path object
     *
     * @return const std::filesystem::path&
     */
    [[nodiscard]]
    const std::filesystem::path &getAssetsPath() const noexcept;

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
