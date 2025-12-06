#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>

/**
 * @brief Generic dependency injection container
 *
 * Provides type-safe service registration and retrieval.
 * Services are stored as shared_ptr and can be accessed by type.
 */
class ServiceContainer {

public:
  ServiceContainer() = default;
  ~ServiceContainer() = default;

  // Non-copyable
  ServiceContainer(const ServiceContainer &) = delete;
  ServiceContainer &operator=(const ServiceContainer &) = delete;

  // Movable
  ServiceContainer(ServiceContainer &&) = default;
  ServiceContainer &operator=(ServiceContainer &&) = default;

  /**
   * @brief Register a service in the container
   * @tparam T Service type (usually an interface)
   * @param service Shared pointer to the service instance
   */
  template <typename T>
  void registerService(std::shared_ptr<T> service) {
    services_[std::type_index(typeid(T))] = service;
  }

  /**
   * @brief Retrieve a service from the container
   * @tparam T Service type to retrieve
   * @return Shared pointer to service, or nullptr if not found
   */
  template <typename T>
  std::shared_ptr<T> getService() {
    auto it = services_.find(std::type_index(typeid(T)));
    if (it != services_.end()) {
      return std::static_pointer_cast<T>(it->second);
    }
    return nullptr;
  }

  /**
   * @brief Retrieve a service from the container (const version)
   * @tparam T Service type to retrieve
   * @return Shared pointer to service, or nullptr if not found
   */
  template <typename T>
  std::shared_ptr<T> getService() const {
    auto it = services_.find(std::type_index(typeid(T)));
    if (it != services_.end()) {
      return std::static_pointer_cast<T>(it->second);
    }
    return nullptr;
  }

  /**
   * @brief Get the Service Count object
   *
   * @return int
   */
  [[nodiscard]]
  int getServiceCount() const noexcept {
    return static_cast<int>(services_.size());
  }

  /**
   * @brief Retrieve a service, throwing if not found
   * @tparam T Service type to retrieve
   * @return Shared pointer to service
   * @throws std::runtime_error if service not found
   */
  template <typename T>
  std::shared_ptr<T> getServiceOrThrow() {
    auto service = getService<T>();
    if (!service) {
      throw std::runtime_error("Service not found: " + std::string(typeid(T).name()));
    }
    return service;
  }

  /**
   * @brief Check if a service is registered
   * @tparam T Service type to check
   * @return true if service exists, false otherwise
   */
  template <typename T>
  bool hasService() const {
    return services_.find(std::type_index(typeid(T))) != services_.end();
  }

  /**
   * @brief Get number of registered services
   */
  [[nodiscard]]
  size_t size() const noexcept {
    return services_.size();
  }

  /**
   * @brief Clear all registered services
   */
  void clear() { services_.clear(); }

private:
  // note: RTTI (Run-Time Type Information) based storage
  std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
};
