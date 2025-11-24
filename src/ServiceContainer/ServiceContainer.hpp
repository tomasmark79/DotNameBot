#ifndef SERVICECONTAINER_HPP
#define SERVICECONTAINER_HPP

#include <memory>
#include <typeindex>
#include <unordered_map>

class ServiceContainer {
private:
  std::unordered_map<std::type_index, std::shared_ptr<void>> services_;

public:
  ServiceContainer() = default;
  ~ServiceContainer() = default;

  template <typename T>
  void registerService(std::shared_ptr<T> service) {
    services_[std::type_index(typeid(T))] = service;
  }

  template <typename T>
  std::shared_ptr<T> getService() {
    auto it = services_.find(std::type_index(typeid(T)));
    if (it != services_.end()) {
      return std::static_pointer_cast<T>(it->second);
    }
    return nullptr;
  }
};

#endif
