#pragma once

#include <string>

/**
 * @brief Interface for lifecycle management of services or components.
 *
 */
class ILifeCycle {
public:
  ILifeCycle() = default;
  virtual ~ILifeCycle() = default;

  // Prevent copying
  ILifeCycle(const ILifeCycle &) = delete;
  ILifeCycle &operator=(const ILifeCycle &) = delete;

  // Allow moving
  ILifeCycle(ILifeCycle &&) = default;
  ILifeCycle &operator=(ILifeCycle &&) = default;

  /**
   * @brief Initialize the service
   *
   * @return true
   * @return false
   */
  virtual bool initialize() = 0;

  /**
   * @brief Start the service (blocking or non-blocking depending on implementation)
   *
   * @return true
   * @return false
   */
  virtual bool start() = 0;

  /**
   * @brief Stop the service
   *
   * @return true
   * @return false
   */
  virtual bool stop() = 0;

  /**
   * @brief Check if service is running
   *
   * @return true
   * @return false
   */
  [[nodiscard]]
  virtual bool isRunning() const = 0;

  /**
   * @brief Get the Name object
   *
   * @return std::string
   */
  [[nodiscard]]
  virtual std::string getName() const = 0;
};
