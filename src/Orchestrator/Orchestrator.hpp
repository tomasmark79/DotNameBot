#pragma once

#include <ILifeCycle/ILifeCycle.hpp>
#include <ServiceContainer/ServiceContainer.hpp>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

/**
 * @brief Orchestrator class to manage lifecycle of entities
 *
 * @tparam T
 */
template <typename T>
class Orchestrator {
public:
  /**
   * @brief Construct a new Orchestrator object
   *
   * @param services Reference to the service container
   */
  explicit Orchestrator(ServiceContainer &services) : services_(services) {}

  ~Orchestrator() { stopAll(); }

  /**
   * @brief Add a lifecycle-managed entity (bot, service, etc.)
   *
   * @param item Unique pointer to the entity instance
   */
  void add(std::unique_ptr<T> item) { items_.push_back(std::move(item)); }

  /**
   * @brief Start all managed entities
   */
  void startAll() {
    if (running_.exchange(true)) {
      return; // Already running
    }

    for (auto &item : items_) {
      try {
        if (item->initialize()) {
          T *item_ptr = item.get();
          threads_.emplace_back([item_ptr]() {
            try {
              item_ptr->start();
            } catch (const std::exception &e) {
              // Item failed to start
            }
          });
        }
      } catch (const std::exception &e) {
        // Failed to initialize item
      }
    }
  }

  /**
   * @brief Stop all managed entities
   */
  void stopAll() {
    if (!running_.exchange(false)) {
      return; // Already stopped
    }

    for (auto &item : items_) {
      try {
        item->stop();
      } catch (const std::exception &e) {
        // Error stopping item
      }
    }

    for (auto &thread : threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

  /**
   * @brief Check if the orchestrator is currently running
   *
   * @return true if running, false otherwise
   */
  [[nodiscard]]
  bool isRunning() const {
    return running_.load();
  }

  /**
   * @brief Get the number of managed entities
   *
   * @return size_t Number of items
   */
  [[nodiscard]]
  size_t size() const {
    return items_.size();
  }

private:
  std::vector<std::unique_ptr<T>> items_;
  std::vector<std::thread> threads_;
  std::atomic<bool> running_{false};

  [[maybe_unused]]
  ServiceContainer &services_;
};
