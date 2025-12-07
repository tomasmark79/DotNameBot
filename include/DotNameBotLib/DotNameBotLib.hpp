#pragma once

#include <DotNameBotLib/version.h> // first configuration will create this file
#include <Utils/UtilsFactory.hpp>
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>

#include <EmojiesLib/EmojiesLib.hpp>
#include <Orchestrator/Orchestrator.hpp>
#include <ServiceContainer/ServiceContainer.hpp>

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
     * Automatically stops any running worker threads
     */
    ~DotNameBotLib();

    /**
     * @brief Copy construction is deleted
     */
    DotNameBotLib(const DotNameBotLib &other) = delete;

    /**
     * @brief Copy assignment is deleted
     */
    DotNameBotLib &operator=(const DotNameBotLib &other) = delete;

    /**
     * @brief Move constructor
     *
     * @param other Object to move from
     */
    DotNameBotLib(DotNameBotLib &&other) noexcept;

    /**
     * @brief Move assignment operator
     *
     * @param other Object to move from
     * @return Reference to this object
     */
    DotNameBotLib &operator=(DotNameBotLib &&other) noexcept;

    // ============================================================================
    // Main API
    // ============================================================================

    /**
     * @brief Run your business logic
     *
     * This is the main entry point for your library's functionality.
     * Starts a worker thread and runs for the specified duration.
     *
     * @param durationSeconds Duration to run in seconds (0 = run indefinitely)
     * @return true if successful
     * @return false if an error occurred or not initialized
     */
    bool run(int durationSeconds = 0);

    /**
     * @brief Stop all running processes
     *
     * Signals the worker thread to stop. Thread will be joined by run() or destructor.
     */
    void stop();

    // ============================================================================
    // Query Methods
    // ============================================================================

    /**
     * @brief Check if the library is properly initialized
     *
     * @return true if initialized and ready to use
     * @return false if initialization failed
     */
    [[nodiscard]]
    bool isInitialized() const noexcept;

    /**
     * @brief Get the assets directory path
     *
     * @return const std::filesystem::path& Path to assets
     */
    [[nodiscard]]
    const std::filesystem::path &getAssetsPath() const noexcept;

    /**
     * @brief Get access to the service container for custom implementations
     *
     * Use this to register additional services or retrieve existing ones
     * before calling run()
     *
     * @return ServiceContainer& Reference to the service container
     */
    ServiceContainer &getServices() noexcept;

    /**
     * @brief Get access to the bot orchestrator for custom bot registration
     *
     * Use this to add custom bots before calling run()
     *
     * @return Orchestrator<ILifeCycle>& Reference to the orchestrator
     */
    Orchestrator<ILifeCycle> &getOrchestrator() noexcept;

  private:
    const std::string libName_ = "DotNameBotLib v." DOTNAMEBOTLIB_VERSION;
    std::shared_ptr<dotnamecpp::logging::ILogger> logger_;
    std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager_;
    std::filesystem::path assetsPath_;
    bool isInitialized_ = false;
    std::atomic<bool> shouldStop_{false};

    std::unique_ptr<ServiceContainer> services_;
    std::unique_ptr<Orchestrator<ILifeCycle>> botOrchestrator_;
    std::shared_ptr<dotname::EmojiesLib> emojiesLib_;
  };

} // namespace dotnamecpp::v1
